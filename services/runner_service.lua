local IS_LOVE = (_G.love ~= nil)

local InstanceType = require("instance_type")
local RunnerService = {}; RunnerService.__index = RunnerService
InstanceType.SetType(RunnerService, "RunnerService")

local DEFAULTS = {
    safe = true,
    priority = 78,
    maxCatchUp = 3,
    maxFails = 3,
    tickRate = 60,
    maxDt = 0.5,
}

---@class Logger
---@field log fun(self: Logger, ...: any)

---@class JobOptions
---@field id string | nil
---@field tag string | nil
---@field priority number | nil
---@field interval number | nil
---@field maxCatchUp number | nil
---@field once boolean | nil
---@field safe boolean | nil
---@field maxFails number | nil

---@class RunnerJob
---@field id string
---@field fn function
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

---@class RunnerConnection
---@field id string
---@field channel string
---@field Connected boolean
---@field disconnect fun(self: RunnerConnection)
---@field Pause fun(self: RunnerConnection)
---@field Resume fun(self: RunnerConnection)

---@class RunnerSignal
---@field Connect fun(self: RunnerSignal, fn: function|string, connectOpts: JobOptions|nil): RunnerConnection

---@class RunnerService
---@field Heartbeat RunnerSignal
---@field Stepped RunnerSignal
---@field RenderStepped RunnerSignal
---@field _log fun(...: any)
---@field _running boolean
---@field _jobs table<string, RunnerJob[]>
---@field _tickRate number
---@field _maxDt number
local RunnerService = {}
RunnerService.__index = RunnerService

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

function RunnerService:bind(channel, fn, opts)
    local providedOpts = opts or {}
    local activeChannel = channel or "Heartbeat"
    local uniqueId = providedOpts.id or ("%s-%d"):format(activeChannel, math.random(1000, 9999))

    local executableFn = fn
    if type(fn) == "string" then
        ---@diagnostic disable-next-line deprecated
        executableFn = (loadstring or load)(fn)
    end

    ---@type RunnerJob
    local jobData = {
        id = uniqueId,
        fn = executableFn,
        tag = providedOpts.tag,
        priority = providedOpts.priority or DEFAULTS.priority,
        interval = providedOpts.interval,
        maxCatchUp = providedOpts.maxCatchUp or DEFAULTS.maxCatchUp,
        maxFails = providedOpts.maxFails or DEFAULTS.maxFails,
        once = providedOpts.once or false,
        safe = (providedOpts.safe == nil) and DEFAULTS.safe or providedOpts.safe,
        paused = false,
        failCount = 0,
        _lastAcc = 0,
    }

    table.insert(self._jobs[activeChannel], jobData)
    table.sort(self._jobs[activeChannel], function(a, b)
        return a.priority < b.priority
    end)

    ---@type RunnerConnection
    return {
        id = uniqueId,
        channel = activeChannel,
        Connected = true,
        disconnect = function(connObj)
            if not connObj.Connected then return end
            local list = self._jobs[activeChannel]
            for i = #list, 1, -1 do
                if list[i].id == uniqueId or (jobData.tag and list[i].tag == jobData.tag) then
                    table.remove(list, i)
                    break
                end
            end
            connObj.Connected = false
        end,
        Pause = function() jobData.paused = true end,
        Resume = function() jobData.paused = false end
    }
end

--- Execute a job safely with error tracking and catch-up bounds
---@param job RunnerJob
---@param dt number
---@return boolean shouldRemove
local function executeJob(job, dt)
    if job.paused or not job.fn then return false end

    -- Handle execution interval & catch-up logic
    if job.interval and job.interval > 0 then
        job._lastAcc = job._lastAcc + dt
        if job._lastAcc < job.interval then
            return false
        end

        local executions = math.floor(job._lastAcc / job.interval)
        if executions > job.maxCatchUp then
            executions = job.maxCatchUp
        end
        job._lastAcc = job._lastAcc - (executions * job.interval)

        for _ = 1, executions do
            if job.safe then
                local ok, err = pcall(job.fn, job.interval)
                if not ok then
                    job.failCount = job.failCount + 1
                    print("\27[31m[RunnerService Job Error]\27[0m " .. tostring(err))
                    if job.failCount >= job.maxFails then
                        return true
                    end
                end
            else
                job.fn(job.interval)
            end
        end
    else
        if job.safe then
            local ok, err = pcall(job.fn, dt)
            if not ok then
                job.failCount = job.failCount + 1
                print("\27[31m[RunnerService Job Error]\27[0m " .. tostring(err))
                if job.failCount >= job.maxFails then
                    return true
                end
            end
        else
            job.fn(dt)
        end
    end

    return job.once
end

--- Step through all registered callbacks for a given frame
---@param dt number
function RunnerService:Step(dt)
    if not self._running then return end

    if dt ~= dt or dt <= 0 then dt = 1 / 60 end
    if dt > self._maxDt then dt = self._maxDt end

    for _, channelName in ipairs({ "Stepped", "Heartbeat", "RenderStepped" }) do
        local channelList = self._jobs[channelName]
        if channelList then
            for index = #channelList, 1, -1 do
                local currentJob = channelList[index]
                if currentJob then
                    local shouldRemove = executeJob(currentJob, dt)
                    if shouldRemove then
                        table.remove(channelList, index)
                    end
                end
            end
        end
    end
end

--- Main loop wrapped in pcall to prevent traceback noise on cancellation
function RunnerService:KeepAlive()
    if IS_LOVE then return end

    local timeFn = os.clock
    local lastTime = timeFn()
    local targetFrameTime = 1 / self._tickRate

    ---@type any
    local globalEnv = _G

    local ok, err = pcall(function()
        while self._running do
            local now = timeFn()
            local dt = now - lastTime
            
            if dt >= targetFrameTime then
                lastTime = now
                self:Step(dt)
            end

            -- Safely invoke global C++ sleep if bound, avoiding diagnostic errors
            if type(globalEnv.sleep) == "function" then
                globalEnv.sleep(0.001)
            end
        end
    end)

    if not ok and err and not tostring(err):find("interrupted") then
        error(err)
    end
end

function RunnerService:destroy()
    self._running = false
    self._jobs = { Heartbeat = {}, Stepped = {}, RenderStepped = {} }
end

-- #service
return RunnerService
