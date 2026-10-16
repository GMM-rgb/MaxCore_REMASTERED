-- =========================================================================
-- services/runner_service.lua
-- =========================================================================
-- Frame/job scheduler with a cooperative, NON-BLOCKING wait.
--
--   runner:Spawn("Heartbeat", function(wait)
--       print("a")
--       local waited = wait(2)      -- suspends ONLY this job for ~2 seconds
--       print("b", waited)          -- everything else keeps running meanwhile
--   end)
--
-- Why this does not freeze the program: wait() is a coroutine yield. It
-- hands control back to the scheduler, which records "wake this job at time
-- T" and moves on to the next job. Nothing loops while waiting. The
-- busy-wait version
--     while os.clock() - t <= n do end
-- never gives control back, so no other job, event, or frame can run until
-- it finishes, and it pins a CPU core at 100% the whole time.
--
-- Idle: KeepAlive() computes how long until the next frame or next sleeping
-- job is due and sleeps the OS thread for exactly that long (native
-- `sleep_native` module). If that module isn't available it falls back to a
-- bounded wait that still yields to the OS where it can, and warns once.

local IS_LOVE = (_G.love ~= nil)

local InstanceType = require("instance_type")

-- Optional native module: a real OS sleep + a monotonic clock.
-- Make Lua look for native modules with the right extension (.dylib on
-- macOS, .so on Linux, .dll on Windows). Without this, a freshly built
-- sleep_native.dylib is never found and the pcall below fails silently.
local NativePaths = require("native_paths")
NativePaths.Apply()

---@type table?
local native, nativeErr = NativePaths.TryRequire("sleep_native")

-- Optional native task scheduler: background worker threads plus a timer
-- thread that sleeps to the exact next deadline. Without it everything
-- below still works (single-threaded, Lua-only), minus Async/ReadFile.
---@type table?
local schedModule, schedErr = NativePaths.TryRequire("scheduler_native")

local DEFAULTS = {
    safe = true,
    priority = 78,
    maxCatchUp = 3,
    maxFails = 3,
    tickRate = 60,
    maxDt = 0.5,
    maxConcurrent = 8,   -- most overlapping firings of one yieldable connection
    maxWait = 3600 * 24, -- clamp absurd waits (a day) so a bad value can't strand a job forever
}

local CHANNELS = { "Stepped", "Heartbeat", "RenderStepped" } -- execution order within a frame

---@class Logger
---@field log fun(self: Logger, ...: any)

---A finished background task, as handed back by join().
---@class TaskEvent
---@field handle integer The packed task handle this event belongs to
---@field kind '"timer"'|'"work"'
---@field ok boolean false if the task failed or was cancelled
---@field value string? The task's output (a ReadFile's bytes). Present when ok is true
---@field error string? Why it failed. Present when ok is false ("cancelled" if it was cancelled)

---Options accepted by RunnerService.new.
---@class RunnerOptions
---@field logger Logger? Receives log lines; defaults to print
---@field tickRate number? Frames per second for KeepAlive (default 60)
---@field maxDt number? Longest dt a single Step will accept, in seconds (default 0.5)
---@field native boolean? Set false to ignore the native scheduler even if it is installed
---@field workers integer? Background worker threads (default: cores minus 2, at least 1)

---The userdata returned by scheduler_native.new(). Lua only ever holds this
---handle; the threads and queues behind it live in C++.
---@class RunnerNative
---@field now fun(self: RunnerNative): number Seconds on the scheduler's monotonic clock
---@field timer fun(self: RunnerNative, seconds: number): integer? Starts a one-shot timer
---@field read_file fun(self: RunnerNative, path: string): integer? Reads a file on a worker thread
---@field cancel fun(self: RunnerNative, handle: integer): boolean
---@field pending fun(self: RunnerNative, handle: integer): boolean
---@field drain fun(self: RunnerNative, budgetSeconds: number?, maxItems: integer?): TaskEvent[]
---@field wait fun(self: RunnerNative, maxSeconds: number) Blocks until a task finishes or maxSeconds passes
---@field until_next_timer fun(self: RunnerNative, fallback: number?): number
---@field shutdown fun(self: RunnerNative) Joins every native thread

---Suspends the calling job for `seconds` and returns how long actually passed.
---@alias SpawnWait fun(seconds: number?): number

---Suspends the calling job until the background task finishes and returns its result.
---@alias SpawnJoin fun(handle: integer): TaskEvent

---The function body given to Spawn.
---@alias SpawnFn fun(wait: SpawnWait, join: SpawnJoin)

---@class JobOptions
---@field id string | nil
---@field tag string | nil
---@field priority number | nil
---@field interval number | nil
---@field maxCatchUp number | nil
---@field once boolean | nil
---@field safe boolean | nil
---@field maxFails number | nil
---@field yieldable boolean | nil Run each firing of a Connect callback as its own coroutine, so it may call wait() and join()
---@field reentrant boolean | nil With yieldable: allow a new firing while the previous one is still waiting (default false: skip it)
---@field maxConcurrent integer | nil With yieldable and reentrant: most firings alive at once (default 8)

---@class RunnerJob
---@field id string
---@field fn function | nil
---@field tag string | nil
---@field priority number
---@field interval number | nil
---@field maxCatchUp number
---@field maxFails number
---@field once boolean
---@field safe boolean
---@field paused boolean
---@field failCount number
---@field _lastAcc number
---@field co thread | nil
---@field wakeAt number | nil Scheduler-clock time this coroutine should next resume (nil = ready now)
---@field sleptSince number | nil Scheduler-clock time it started waiting, used to report how long it waited
---@field seq number Insertion order, used as a stable tie-breaker when sorting by priority
---@field joinHandle integer? Native task handle this job is suspended on
---@field joinEvent TaskEvent? The result waiting to be handed to join() when the job resumes
---@field dead boolean Set when removed, so a job disconnected mid-step is never resumed

---@class RunnerConnection
---@field id string
---@field channel string
---@field Connected boolean
---@field disconnect fun(self: RunnerConnection)
---@field Pause fun(self: RunnerConnection)
---@field Resume fun(self: RunnerConnection)

---@class RunnerSignal
---@field Connect fun(self: RunnerSignal, fn: fun(dt: number)|string|SpawnFn, connectOpts: JobOptions|nil): RunnerConnection

-- Forward declaration: stepCoroutine is defined further down, but
-- _bindYieldable (above it) needs to start a firing's first slice at once.
-- A `local function` is only visible AFTER its definition, so without this
-- the call below would silently resolve to a nil global.
---@type fun(self: RunnerService, job: RunnerJob): boolean
local stepCoroutine

---@class RunnerService
---@field Heartbeat RunnerSignal
---@field Stepped RunnerSignal
---@field RenderStepped RunnerSignal
---@field _log fun(...: any)
---@field _running boolean
---@field _jobs table<string, RunnerJob[]>
---@field _tickRate number
---@field _maxDt number
---@field _now number Scheduler clock in seconds. Advances by the (clamped) dt of every Step.
---@field _seq number
---@field _warnedNoSleep boolean
---@field _native RunnerNative? Native scheduler (nil when scheduler_native is unavailable)
---@field _joining table<integer, RunnerJob> Jobs suspended on a background task, by packed native handle
---@field _ready table<integer, TaskEvent> Completions that arrived before any job joined them (handle -> event)
local RunnerService = {}
RunnerService.__index = RunnerService
InstanceType.SetType(RunnerService, "RunnerService")

-- A job's `wait` function is only valid inside the coroutine it was made
-- for. This marker table lets us tell "yielded for a wait" apart from any
-- other yield a user might do, and lets wait() fail clearly elsewhere.
local WAIT = {}
local JOIN = {}   -- marker: "suspend this job until a background task finishes"

---@param self RunnerService?
local function newClock(self)
    if self and self._native then
        return function() return self._native:now() end
    end
    if native and native.monotonic then return native.monotonic end
    return os.clock -- CPU time: last resort. Under CPU contention it under-reports elapsed time.
end

---@param opts RunnerOptions?
---@return RunnerService
function RunnerService.new(opts)
    ---@type RunnerService
    local self = setmetatable({}, RunnerService)

    local providedOpts = opts or {}

    self._log = (providedOpts.logger and providedOpts.logger.log)
        and function(...) providedOpts.logger:log(...) end
        or function(...) print("[Runner]", ...) end

    self._tickRate = providedOpts.tickRate or DEFAULTS.tickRate
    self._maxDt = providedOpts.maxDt or DEFAULTS.maxDt
    self._running = true
    self._jobs = { Heartbeat = {}, Stepped = {}, RenderStepped = {} }
    self._now = 0
    self._seq = 0
    self._warnedNoSleep = false
    self._joining = {}
    self._ready = {}
    self._native = nil
    if schedModule and providedOpts.native ~= false then
        local okNew, inst = pcall(schedModule.new, providedOpts.workers)
        if okNew then self._native = inst end
    end

    ---@param channelName string
    ---@return RunnerSignal
    local function makeSignal(channelName)
        ---@type RunnerSignal
        return {
            Connect = function(_, fn, connectOpts)
                return self:bind(channelName, fn, connectOpts or {})
            end
        }
    end

    self.Heartbeat = makeSignal("Heartbeat")
    self.Stepped = makeSignal("Stepped")
    self.RenderStepped = makeSignal("RenderStepped")

    return self
end

-- Jobs sort by priority, then by insertion order, so equal-priority jobs
-- always run in the order they were added (table.sort alone is not stable).
---@param a RunnerJob
---@param b RunnerJob
---@return boolean
local function byPriority(a, b)
    if a.priority ~= b.priority then return a.priority < b.priority end
    return a.seq < b.seq
end

---@param self RunnerService
---@param channel string
---@param job RunnerJob
---@return RunnerConnection
local function register(self, channel, job)
    local list = self._jobs[channel]
    if not list then
        error(("RunnerService: unknown channel %q (valid: Stepped, Heartbeat, RenderStepped)"):format(tostring(channel)), 3)
    end

    self._seq = self._seq + 1
    job.seq = self._seq
    job.dead = false
    list[#list + 1] = job
    table.sort(list, byPriority)

    ---@type RunnerConnection
    local conn
    conn = {
        id = job.id,
        channel = channel,
        Connected = true,
        disconnect = function(connObj)
            if not connObj.Connected then return end
            connObj.Connected = false
            job.dead = true -- a job removed mid-Step must not get resumed again this frame
            if job.joinHandle then
                if self._joining[job.joinHandle] == job then self._joining[job.joinHandle] = nil end
                if self._native then self._native:cancel(job.joinHandle) end
                job.joinHandle = nil
            end
            local current = self._jobs[channel]
            -- Remove THIS job by identity, never by tag/id: two jobs can
            -- share a tag, and removing the wrong one is a silent bug.
            for i = #current, 1, -1 do
                if current[i] == job then
                    table.remove(current, i)
                    break
                end
            end
        end,
        Pause = function() job.paused = true end,
        Resume = function() job.paused = false end,
    }
    return conn
end

---@param prefix string
---@return string
local function newJobId(prefix)
    return ("%s-%d"):format(prefix, math.random(100000, 999999))
end

--- Connect a plain callback to a channel. Runs every frame (or every
--- `interval` seconds). Cannot call wait(); use Spawn for that.
---@param channel string
---@param fn function|string
---@param opts JobOptions|nil
---@return RunnerConnection
function RunnerService:bind(channel, fn, opts)
    local providedOpts = opts or {}
    local activeChannel = channel or "Heartbeat"

    local executableFn = fn
    if type(fn) == "string" then
        ---@diagnostic disable-next-line: deprecated
        local chunk, err = (loadstring or load)(fn)
        if not chunk then
            error("RunnerService:bind could not compile the given code: " .. tostring(err), 2)
        end
        executableFn = chunk
    end
    if type(executableFn) ~= "function" then
        error("RunnerService:bind expects a function or a string of Lua code", 2)
    end

    if providedOpts.yieldable then
        return self:_bindYieldable(activeChannel, executableFn --[[@as SpawnFn]], providedOpts)
    end

    ---@type RunnerJob
    local jobData = {
        id = providedOpts.id or newJobId(activeChannel),
        fn = executableFn --[[@as function]],
        tag = providedOpts.tag,
        priority = providedOpts.priority or DEFAULTS.priority,
        interval = providedOpts.interval,
        maxCatchUp = providedOpts.maxCatchUp or DEFAULTS.maxCatchUp,
        maxFails = providedOpts.maxFails or DEFAULTS.maxFails,
        once = providedOpts.once or false,
        safe = (providedOpts.safe == nil) and DEFAULTS.safe or (providedOpts.safe or true),
        paused = false,
        failCount = 0,
        _lastAcc = 0,
        co = nil,
        wakeAt = nil,
        seq = 0,
        dead = false,
    }
    return register(self, activeChannel, jobData)
end

--- Connect a callback that is allowed to wait. Each time the channel fires,
--- `fn(wait, join)` starts as its OWN coroutine, so it can suspend without
--- blocking anything else. Used by Connect(fn, { yieldable = true }).
---
--- A connection never piles up: while a firing is still waiting, the next
--- one is skipped (reentrant = false, the default). With reentrant = true,
--- firings may overlap, but never more than maxConcurrent at once. The excess
--- is skipped, not queued, so a fast channel cannot grow memory without bound.
---@param channel string
---@param fn SpawnFn
---@param opts JobOptions
---@return RunnerConnection
function RunnerService:_bindYieldable(channel, fn, opts)
    ---@type RunnerJob[]
    local live = {}
    local limit = opts.reentrant and (opts.maxConcurrent or DEFAULTS.maxConcurrent) or 1
    if limit < 1 then limit = 1 end
    local connected = true

    -- The launcher runs on the channel like any bound callback. It only
    -- decides whether to start a firing; the firing itself is a Spawn job.
    local launcher = self:bind(channel, function()
        if not connected then return end
        -- Drop firings that are no longer running. A firing is finished when
        -- its job was removed (dead) OR its coroutine ended, whether it
        -- returned normally or died with an error.
        for i = #live, 1, -1 do
            local j = live[i]
            if j.dead or (j.co and coroutine.status(j.co) == "dead") then table.remove(live, i) end
        end
        if #live >= limit then return end

        local conn = self:Spawn(channel, fn, { tag = opts.tag, priority = opts.priority, maxFails = opts.maxFails })
        local job = self:_jobOf(conn)
        live[#live + 1] = job
        -- Start it now, in the same step the channel fired. Without this the
        -- firing would not begin until the NEXT step, adding a frame of delay
        -- to every wait() it makes and stretching a 0.25s cycle to ~0.29s.
        if stepCoroutine(self, job) then self:_killJob(channel, job) end
    end, {
        id = opts.id, tag = opts.tag, priority = opts.priority,
        interval = opts.interval, maxCatchUp = opts.maxCatchUp,
        once = opts.once, safe = opts.safe, maxFails = opts.maxFails,
    })

    ---@type RunnerConnection
    local conn
    conn = {
        id = launcher.id,
        channel = channel,
        Connected = true,
        disconnect = function(connObj)
            if not connObj.Connected then return end
            connObj.Connected = false
            connected = false
            launcher:disconnect()
            -- Kill every firing still in flight; they must not outlive the connection.
            for i = #live, 1, -1 do self:_killJob(channel, live[i]) end
            live = {}
        end,
        Pause = function() launcher:Pause() end,
        Resume = function() launcher:Resume() end,
    }
    return conn
end

---Finds the job a connection controls (the newest job on its channel with its id).
---@param conn RunnerConnection
---@return RunnerJob
function RunnerService:_jobOf(conn)
    local list = self._jobs[conn.channel]
    for i = #list, 1, -1 do
        if list[i].id == conn.id then return list[i] end
    end
    error("RunnerService: internal error, job for connection " .. tostring(conn.id) .. " not found", 2)
end

---Removes a job from its channel by identity and marks it dead so it is never resumed again.
---@param channel string
---@param job RunnerJob
function RunnerService:_killJob(channel, job)
    job.dead = true
    if job.joinHandle then
        if self._joining[job.joinHandle] == job then self._joining[job.joinHandle] = nil end
        if self._native then self._native:cancel(job.joinHandle) end
        job.joinHandle = nil
    end
    local list = self._jobs[channel]
    for i = #list, 1, -1 do
        if list[i] == job then table.remove(list, i) break end
    end
end

--- Spawn a coroutine job. The function receives `wait(seconds)`, which
--- suspends only this job and returns how many seconds actually passed.
---
---     runner:Spawn("Heartbeat", function(wait)
---         while true do
---             doSomething()
---             wait(0.5)
---         end
---     end)
---
--- The job starts on the NEXT step (not inside this call), so Spawn always
--- returns immediately, even if the function waits or errors right away.
---@param channel string
---@param fn SpawnFn
---@param opts JobOptions|nil
---@return RunnerConnection
function RunnerService:Spawn(channel, fn, opts)
    local activeChannel = channel or "Heartbeat"
    local providedOpts = opts or {}
    if type(fn) ~= "function" then
        error("RunnerService:Spawn expects a function", 2)
    end

    local job -- forward declaration so wait() can reach its own job

    --- Parks this job until a background task (from Async/ReadFile) finishes,
    --- then returns its event: { ok, value | error, handle, kind }.
    --- Only this job is parked; everything else keeps running.
    ---@type SpawnJoin
    local function join(handle)
        if coroutine.running() ~= job.co then
            error("join() can only be called from inside the job it was given to", 2)
        end
        if type(handle) ~= "number" then
            error("join(handle) expects a handle from ReadFile/Async, got " .. type(handle), 2)
        end
        return coroutine.yield(JOIN, handle)
    end

    --- Suspends this job for `seconds` (0 or nil = resume next step).
    --- Returns the seconds that actually elapsed.
    ---@type SpawnWait
    local function wait(seconds)
        if coroutine.running() ~= job.co then
            error("wait() can only be called from inside the job it was given to", 2)
        end
        if seconds ~= nil and type(seconds) ~= "number" then
            error("wait(seconds) expects a number, got " .. type(seconds), 2)
        end
        -- NaN, negative, and nil all mean "next step". Clamp huge values.
        local s = seconds or 0
        if s ~= s or s < 0 then s = 0 end
        if s > DEFAULTS.maxWait then s = DEFAULTS.maxWait end
        return coroutine.yield(WAIT, s)
    end

    ---@type RunnerJob
    job = {
        id = providedOpts.id or newJobId("Spawn"),
        fn = nil,
        tag = providedOpts.tag,
        priority = providedOpts.priority or DEFAULTS.priority,
        interval = nil,
        maxCatchUp = DEFAULTS.maxCatchUp,
        maxFails = providedOpts.maxFails or DEFAULTS.maxFails,
        once = false,
        safe = true,
        paused = false,
        failCount = 0,
        _lastAcc = 0,
        co = coroutine.create(function() fn(wait, join) end),
        wakeAt = nil, -- ready immediately: first slice runs on the next Step
        sleptSince = nil,
        seq = 0,
        dead = false,
    }
    return register(self, activeChannel, job)
end

--- True when background threads are available.
---@return boolean
function RunnerService:HasNative()
    return self._native ~= nil
end

--- Reads a file on a background thread. Returns a handle for join(), or
--- nil if the native scheduler isn't available. The file's bytes arrive in
--- the event's `value`; on failure `ok` is false and `error` says why.
---@param path string
---@return integer? handle
function RunnerService:ReadFile(path)
    if not self._native then return nil end
    return self._native:read_file(path)
end

--- Cancels a background task. A job parked on it is woken with
--- { ok = false, error = "cancelled" } rather than left parked forever.
---@param handle integer
---@return boolean cancelled
function RunnerService:CancelTask(handle)
    if not self._native then return false end
    local was = self._native:cancel(handle)
    self._ready[handle] = nil
    local parked = self._joining[handle]
    if parked then
        self._joining[handle] = nil
        parked.joinEvent = { ok = false, error = "cancelled", handle = handle, kind = "work" }
        parked.wakeAt = self._now + 1e-9
    end
    return was
end

--- Collects finished background tasks and wakes the jobs waiting on them.
--- Called once per Step. Time-budgeted so a flood of completions can't
--- stretch a frame.
---@param budget number? seconds (default 2ms)
function RunnerService:Pump(budget)
    local native_ = self._native
    if not native_ then return end
    for _, ev in ipairs(native_:drain(budget or 0.002)) do
        if ev.kind == "work" then
            local job = self._joining[ev.handle]
            if job then
                self._joining[ev.handle] = nil
                job.joinEvent = ev
                job.wakeAt = self._now + 1e-9
            else
                -- Nobody is suspended on it yet (or nothing has joined it yet). Keep it
                -- briefly so a late join() still gets the result; bounded so an
                -- un-joined flood can't grow without limit.
                local count = 0
                for _ in pairs(self._ready) do count = count + 1 end
                if count < 4096 then self._ready[ev.handle] = ev end
            end
        end
    end
end

---@param label string
---@param err any
local function logError(label, err)
    print("\27[31m[RunnerService " .. label .. "]\27[0m " .. tostring(err))
end

--- Advances one coroutine job by at most one slice.
---@param self RunnerService
---@param job RunnerJob
---@return boolean shouldRemove
function stepCoroutine(self, job)
    local co = job.co --[[@as thread]]
    if coroutine.status(co) == "dead" then return true end

    -- Still sleeping? Compare against the absolute wake time. No per-frame
    -- subtraction, so a late frame can't make a wait run long, and an early
    -- one can't make it run short.
    if job.wakeAt and self._now < job.wakeAt then return false end

    local waited = 0
    if job.sleptSince then waited = self._now - job.sleptSince end
    -- Remember when this wait was SUPPOSED to end. If the frame landed late,
    -- the next wait is measured from here instead of from "now", so the
    -- overshoot of one wait doesn't get added on top of the next one.
    local scheduledWake = job.wakeAt
    job.wakeAt, job.sleptSince = nil, nil

    -- What the job gets back from the call it was suspended in:
    --   wait(n) -> a number (seconds that really passed)
    --   join(h) -> a TaskEvent (the finished task's result)
    -- Each branch resumes with exactly one of those, never a mix.
    local ok, tag, seconds
    if job.joinHandle then
        ---@type TaskEvent?
        local event = job.joinEvent
        job.joinHandle, job.joinEvent = nil, nil
        ok, tag, seconds = coroutine.resume(co, event)
    else
        ok, tag, seconds = coroutine.resume(co, waited)
    end

    if not ok then
        job.failCount = job.failCount + 1
        logError("Job Error", debug.traceback(co, tostring(tag)))
        -- A coroutine that errored is dead; there is nothing to retry.
        return true
    end

    if coroutine.status(co) == "dead" then return true end

    if tag == JOIN then
        local handle = seconds -- the second yielded value is the handle here
        job.sleptSince = self._now
        local early = self._ready[handle]
        if early then
            -- The task finished before we got around to parking on it.
            self._ready[handle] = nil
            job.joinHandle, job.joinEvent = handle, early
            job.wakeAt = self._now + 1e-9
        elseif not self._native then
            job.joinHandle, job.joinEvent = handle, { ok = false, error = "no native scheduler available", handle = handle, kind = "work" }
            job.wakeAt = self._now + 1e-9
        else
            job.joinHandle = handle
            job.wakeAt = math.huge        -- parked: only a completion (or disconnect) wakes it
            self._joining[handle] = job
        end
        return false
    end

    if tag == WAIT then
        job.sleptSince = self._now
        if (seconds or 0) <= 0 then
            -- wait(0): resume on the next step, never within this one.
            job.wakeAt = self._now + 1e-9
        else
            -- Anchor to the previous scheduled wake when we resumed late, but
            -- never further back than one maxDt (a long stall must not make
            -- the next several waits fire instantly to "catch up").
            local anchor = self._now
            if scheduledWake and scheduledWake < self._now and (self._now - scheduledWake) < self._maxDt then
                anchor = scheduledWake
            end
            job.wakeAt = anchor + seconds
            if job.wakeAt <= self._now then job.wakeAt = self._now + 1e-9 end
        end
    else
        -- The job yielded without using wait(): treat as "resume next step".
        job.wakeAt = self._now + 1e-9
        job.sleptSince = self._now
    end
    return false
end

--- Execute a plain (non-coroutine) job safely with error tracking and catch-up bounds
---@param job RunnerJob
---@param dt number
---@return boolean shouldRemove
local function stepCallback(job, dt)
    if not job.fn then return false end

    ---@param arg number
    ---@return boolean shouldRemove
    local function call(arg)
        if job.safe then
            local ok, err = pcall(job.fn, arg)
            if not ok then
                job.failCount = job.failCount + 1
                logError("Job Error", err)
                if job.failCount >= job.maxFails then return true end
            end
            return false
        end
        job.fn(arg)
        return false
    end

    if job.interval and job.interval > 0 then
        job._lastAcc = job._lastAcc + dt
        if job._lastAcc < job.interval then return false end

        local executions = math.floor(job._lastAcc / job.interval)
        if executions > job.maxCatchUp then
            -- We're further behind than we're willing to catch up. Drop the
            -- backlog instead of carrying it, or it would re-trigger forever.
            executions = job.maxCatchUp
            job._lastAcc = 0
        else
            job._lastAcc = job._lastAcc - executions * job.interval
        end

        for _ = 1, executions do
            if call(job.interval) then return true end
            if job.dead then return false end
        end
    else
        if call(dt) then return true end
    end

    return job.once == true
end

---@param self RunnerService
---@param job RunnerJob
---@param dt number
---@return boolean shouldRemove
local function executeJob(self, job, dt)
    if job.paused or job.dead then return false end
    if job.co then return stepCoroutine(self, job) end
    return stepCallback(job, dt)
end

--- Step through all registered callbacks for a given frame
---@param dt number
function RunnerService:Step(dt)
    if not self._running then return end

    if dt ~= dt or dt <= 0 then dt = 1 / 60 end
    if dt > self._maxDt then dt = self._maxDt end
    self._now = self._now + dt
    self:Pump()

    for _, channelName in ipairs(CHANNELS) do
        local channelList = self._jobs[channelName]
        if channelList then
            -- Iterate over a snapshot: a job may Spawn/disconnect others
            -- while it runs, and mutating the live list mid-loop skips or
            -- double-runs entries.
            local snapshot = table.move(channelList, 1, #channelList, 1, {})
            for i = 1, #snapshot do
                local currentJob = snapshot[i]
                if not currentJob.dead then
                    local shouldRemove = executeJob(self, currentJob, dt)
                    if shouldRemove and not currentJob.dead then
                        currentJob.dead = true
                        for j = #channelList, 1, -1 do
                            if channelList[j] == currentJob then
                                table.remove(channelList, j)
                                break
                            end
                        end
                    end
                end
            end
        end
    end
end

--- Seconds until something needs to run: the next frame, or an earlier
--- wake-up of a sleeping job. Never negative.
---@param self RunnerService
---@param untilFrame number Seconds until the next frame is due
---@return number
local function secondsUntilNextWork(self, untilFrame)
    local soonest = untilFrame
    for _, list in pairs(self._jobs) do
        for i = 1, #list do
            local j = list[i]
            if j.co and not j.paused and not j.dead and j.wakeAt then
                local remaining = j.wakeAt - self._now
                if remaining < soonest then soonest = remaining end
            end
        end
    end
    if self._native then
        local t = self._native:until_next_timer(soonest)
        if t < soonest then soonest = t end
    end
    return soonest > 0 and soonest or 0
end

--- Idle the OS thread for `seconds` without spinning.
---@param self RunnerService
---@param seconds number
local function idle(self, seconds)
    if seconds <= 0 then return end
    if self._native then
        -- Wakes after `seconds` OR the moment a background task completes.
        self._native:wait(seconds)
        return
    end
    if native and native.sleep then
        native.sleep(seconds)
        return
    end
    ---@type any
    local globalEnv = _G
    if type(globalEnv.sleep) == "function" then
        globalEnv.sleep(seconds)
        return
    end
    -- No OS sleep is available. Pure Lua cannot give the CPU back, so the
    -- loop will spin until the next frame is due. Say so once, loudly,
    -- instead of silently pinning a core.
    if not self._warnedNoSleep then
        self._warnedNoSleep = true
        print("[RunnerService] WARNING: no OS sleep available (native module 'sleep_native' " ..
            "or a global sleep(seconds)). The main loop will busy-wait between frames and " ..
            "use a full CPU core. Build sleep_native.cpp to fix this.")
        if nativeErr then
            -- Lua's own message lists every path it searched, which is exactly
            -- what you need to see when the file exists but is not found.
            print("[RunnerService] require('sleep_native') said:\n" .. nativeErr)
        end
    end
end

--- Main loop. Sleeps between frames instead of spinning. Wrapped in pcall
--- so Ctrl+C / cancellation doesn't dump a traceback.
function RunnerService:KeepAlive()
    if IS_LOVE then return end

    local clock = newClock(self)
    local lastTime = clock()
    local targetFrameTime = 1 / self._tickRate

    local ok, err = pcall(function()
        while self._running do
            local now = clock()
            local elapsed = now - lastTime

            if elapsed >= targetFrameTime then
                lastTime = now
                self:Step(elapsed)
            else
                -- Not time for a frame yet: sleep until the frame is due, or
                -- until a sleeping job needs to wake, whichever is sooner.
                local untilFrame = targetFrameTime - elapsed
                idle(self, secondsUntilNextWork(self, untilFrame))
            end
        end
    end)

    if not ok and err and not tostring(err):find("interrupted") then
        error(err)
    end
end

function RunnerService:destroy()
    self._running = false
    if self._native then
        pcall(self._native.shutdown, self._native)  -- joins workers + timer thread
        self._native = nil
    end
    self._joining, self._ready = {}, {}
    for _, list in pairs(self._jobs) do
        for i = 1, #list do list[i].dead = true end
    end
    self._jobs = { Heartbeat = {}, Stepped = {}, RenderStepped = {} }
end

-- #service
return RunnerService
