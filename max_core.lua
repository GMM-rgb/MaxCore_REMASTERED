--!strict Lua
--[[
    PATH: ./max_core.lua
    Framework - A Lua-based core module addon for task management, event handling, multi file modules, and more.

    DESCRIPTION:
    Easier lua scripting, better file managment, and automatic service scanning.

    AUTHOR: Maximus F. | Github: @GMM-rgb
    DATE: 2026-07-20
    VERSION: 1.6.0

    Copyright (c) 2026 GMM-rgb
    Licensed under the MIT License.
--]]

-- =========================================================================
--                          MASTER SERVICE REGISTRY
-- =========================================================================

---@class CoreServices
---@field RunnerService RunnerService
---@field runner_service RunnerService
---@field SoundService SoundService
---@field sound_service SoundService
---@field InputService InputService
---@field input_service InputService

---@class MainKit
---@field Load fun(selfObj: table)
---@field destroy fun(indentifier, scope)
---@field ResolveService fun(selfObj: table, name: any)
---@field clampDt fun(dt, maxDt)
---@field new fun(): MainKit
---@field debugMode boolean
---@field _aliases table
---@field __index table
---@field _cache table

---@class MaxCore
---@field MainKit MainKit

local debugMode = true
local PlayerManager = require("utils.player_managment.players")
local InstanceType = require("instance_type")

-- =========================================================================
--      EMMYLUA STUB TYPE CONFIGURATION (IDE Autocomplete Definitions)
-- =========================================================================
---@alias ServiceName
---| '"RunnerService"'
---| '"LoggerService"'
---| '"runner_service"'
---| '"logger_service"'

-- =========================================================================
--      SYSTEM PATH BOOTSTRAPPER (Auto-inject environment folders)
-- =========================================================================
local userPath = os.getenv("USERPROFILE") or os.getenv("HOME") or "C:/Users/maxim"
userPath = userPath:gsub("\\", "/")

local pathInjections = {
    share = userPath .. "/AppData/Roaming/luarocks/share/lua/5.4/?.lua;" .. userPath .. "/AppData/Roaming/luarocks/share/lua/5.4/?/init.lua;",
    lib   = userPath .. "/AppData/Roaming/luarocks/lib/lua/5.4/?.dll;"
};

if not package.path:find("luarocks") then
    package.path = package.path .. ";" .. pathInjections.share .. "./?.lua"
end
if not package.cpath:find("luarocks") then
    package.cpath = package.cpath .. ";" .. pathInjections.lib
end

-- =========================================================================
-- INTERNAL UTILITIES & LENUMS
-- =========================================================================
local function getLine()
    local info = debug.getinfo(2, "l")
    return info and info.currentline or -1
end

local function getFileName(nameValue)
    local info = nameValue
    local fileInfo = info.source
    return fileInfo or nil
end

local lenum = {
    Font = {
        DEFAULT = "default",
        MONOSPACE = "monospace",
        SANS_SERIF = "sans-serif",
        SERIF = "serif",
        ARIAL = "arial"
    },
    Color = {
        RED = "31",
        CYAN = "36",
        YELLOW = "33",
        MAGENTA = "35",
        GREEN = "32",
        WHITE_ON_RED = "41;37",
        WHITE = "37"
    }
}

---@alias LogLevel "ERROR" | "INFO" | "WARNING" | "DEBUG" | "SUCCESS" | "CRITICAL" | "RED" | "GREEN" | "BLUE

---@param level LogLevel
---@param message string
local function colorPrint(level, message)
    local colorKeywords = {
        ERROR = "31",
        INFO = "36",
        WARNING = "33",
        DEBUG = "35",
        SUCCESS = "32",
        CRITICAL = "41;37",
        DEFAULT = "41",
        RED = "31",
        GREEN = "4",
        BLUE = "16",
    };

    local code = colorKeywords[level] or "37"
    print("\27[" .. code .. "m" .. message .. "\27[0m")
end

local function sleep(timeWait)
    local t = os.clock()
    while os.clock() - t <= timeWait do end
end

local MainKit = {}
MainKit.__index = MainKit

function MainKit.new()
    return setmetatable({}, MainKit)
end

MainKit.clampDt = function(dt, maxDt)
    return math.min(dt, maxDt or 0.1)
end

MainKit.debugMode = debugMode

-- =========================================================================
-- CLI FLAG ENGINE
-- =========================================================================
--- Parses target execution string parameters passed via CLI args (e.g. `--test=[1]`)
---@return table<string, any>
function MainKit:Load()
    local flags = {}
    local systemArgs = _G.arg or {}

    for _, argStr in ipairs(systemArgs) do
        if argStr:sub(1, 2) == "--" then
            local rawContent = argStr:sub(3)
            local key, valueMatch = rawContent:match("^([^=]+)=(.*)$")

            if key then
                -- Strip bracket formatting wrapper if it exists: [val] -> val
                local bracketExtraction = valueMatch:match("^%[(.*)%]$")
                if bracketExtraction then valueMatch = bracketExtraction end

                -- Strip literal enclosing string quote marks: "val" or 'val' -> val
                local quoteExtraction = valueMatch:match("^\"(.*)\"$") or valueMatch:match("^'(.*)'$")
                if quoteExtraction then valueMatch = quoteExtraction end

                -- Map values to native runtime primitives
                if valueMatch == "true" then
                    flags[key] = true
                elseif valueMatch == "false" then
                    flags[key] = false
                elseif tonumber(valueMatch) then
                    flags[key] = tonumber(valueMatch)
                else
                    flags[key] = valueMatch
                end
            else
                -- Naked flags automatically initialize as simple boolean gates
                flags[rawContent] = true
            end
        end
    end

    return flags
end

-- =========================================================================
-- TASK ENGINE
-- =========================================================================
local task = {}
function task.seed(fn, ...)
    local coro = coroutine.create(fn)
    local s, err = coroutine.resume(coro, ...)

    if not s then
        colorPrint("ERROR", "Task Error: " .. err)
    end

    local oldCoro = coro
    local success = MainKit:WaitForCondition(function()
        return oldCoro ~= nil
    end, 1)

    if coroutine.status(coro) == "dead" then
        if MainKit.debugMode then
            colorPrint("DEBUG", "Thread task completed. - MAXCORE.LUA - " .. tostring(getLine()))
        end
        local trySuccess, errorClose = pcall(function()
            if success then
                colorPrint("INFO", "Closed old coroutine runtime thread.")
            end
        end)
        if errorClose then
            colorPrint("CRITICAL", "Closing failed on coroutine thread." .. errorClose)
        elseif trySuccess then
            colorPrint("SUCCESS", "Success on pcall coroutine thread.")
        end
    end
end

function task.wait(n)
    n = n or 0
    return coroutine.wrap(function()
        sleep(n)
        return true
    end)()
end

function MainKit:Wait(n)
    return task.wait(n)
end

---@param condition fun(): boolean
---@param timeout number
---@return boolean
function MainKit:WaitForCondition(condition, timeout)
    local startTime = os.clock()
    while not condition() do
        if os.clock() - startTime > timeout then return false end
        task.wait(0.1)
    end colorPrint("SUCCESS", "Condition met within timeout.")
    return true
end

-- =========================================================================
-- DESTRUCTION PATTERNS
-- =========================================================================
function MainKit:destroy(identifier, scope)
    if identifier == nil then error("Destroy function requires an identifier.") end
    
    if type(identifier) == "string" and scope ~= nil then
        local varName = identifier
        local value = scope
        local targetScope = _G
        local destroyable = type(value) == "table" and value or {_value = value}
        
        destroyable.Destroy = function()
            task.seed(function()
                if targetScope[varName] ~= nil then
                    targetScope[varName] = nil
                    colorPrint("SUCCESS", "Variable destroyed: " .. varName)
                    collectgarbage()
                end
            end)
        end
        
        if type(value) ~= "table" then
            setmetatable(destroyable, {
                __tostring = function() return tostring(value) end,
                __index = function(t, k)
                    if k == "Destroy" or k == "_value" then return rawget(t, k) end
                    return nil
                end
            })
        end
        targetScope[varName] = destroyable
        return destroyable
    end
    
    local varName = tostring(identifier)
    task.seed(function()
        if _G[varName] ~= nil then
            _G[varName] = nil
            collectgarbage()
            colorPrint("SUCCESS", "Object Variable destroyed successfully: " .. varName)
        end
    end)
end

-- =========================================================================
--                                EVENT ENGINE
-- =========================================================================
---@class Event
---@field _listeners table<integer, function>
---@field _connected boolean
---@field _nextId integer
---@field _name string
local Event = {}; Event.__index = Event
InstanceType.SetType(Event, "Event")

---@param EventName string|nil
---@return Event
function Event.new(EventName)
    local FormatedEventName = tostring(EventName or "EventObject")
    if MainKit.debugMode then colorPrint("DEBUG", "Created Event: " .. FormatedEventName) end
    return setmetatable({ _listeners = {}, _nextId = 1, _name = FormatedEventName, _connected = false }, Event)
end

---@return boolean
function Event:IsConnected()
    if InstanceType.GetType(self) ~= "Event" then return false end
    return self._connected
end

function Event:Connect(fn, ...)
    local id = self._nextId
    self._nextId = self._nextId + 1
    self._listeners[id] = fn

    if not self._connected then
        self._connected = true
    end return {
        disconnect = function()
            self._listeners[id] = nil
            if #self._listeners <= 0 then
                self._connected = false
            end
        end
    }
end

---@param TargetNameSelection string
function Event:RenameEvent(TargetNameSelection)
    if not TargetNameSelection or not self._name or type(self._name) ~= "string" then return end
    if TargetNameSelection == nil or type(TargetNameSelection) ~= "string" then return end

    local PreviousObjectName = tostring(self._name or "EventObject")
    self._name = tostring(TargetNameSelection) or "EventObject"

    local RenamingSucess = MainKit:WaitForCondition(function()
       return TargetNameSelection == self._name 
    end, 10)

    if RenamingSucess ~= nil and type(RenamingSucess) == "boolean" and RenamingSucess then
        colorPrint("INFO", "RENAMED EVENT:\t" .. PreviousObjectName .. " => " .. tostring(self._name))
    end
end

---@return string
---@nodiscard
function Event:GetEventName()
    if not self._name or type(self._name) ~= "string" then return "EventObject" end
    return self._name
end

function Event:Fire(...)
    for _, listener in pairs(self._listeners) do listener(...) end
end

-- =========================================================================
-- DYNAMIC AUTO-SCANNING SERVICE ENGINE (The Fetcher)
-- =========================================================================
MainKit._cache = {}
MainKit._aliases = {
    RunService = "RunnerService",
}

local function getServiceFiles()
    local files = {}
    local isWindows = (package.config:sub(1,1) == "\\")
    local cmd = isWindows and "dir /s /b *.lua 2>nul" or "find . -name '*.lua' 2>/dev/null"
    
    local p = io.popen(cmd)
    if p then
        for line in p:lines() do
            local cleanName = line:match("([^/\\]+)%.lua$")
            if cleanName then
                table.insert(files, { name = cleanName, path = line })
            end
        end
        p:close()
    end
    return files
end

function MainKit:ResolveService(name)
    local cleanRequest = name:gsub("%.lua$", "")
    local actual = self._aliases[cleanRequest] or cleanRequest
    if self._cache[actual] then return self._cache[actual] end

    local foundPath = nil
    local files = getServiceFiles()
    
    local cwd = os.getenv("PWD") or io.popen("cd"):read("*l") or ""
    cwd = cwd:gsub("\\", "/"):gsub("%-", "%%-"):gsub("%.", "%%.")
    if not cwd:match("/$") and cwd ~= "" then cwd = cwd .. "/" end

    for _, fileData in ipairs(files) do
        local normalizedFile = fileData.name:gsub("_", ""):lower()
        local normalizedActual = actual:gsub("_", ""):lower()

        if normalizedFile == normalizedActual then
            local file = io.open(fileData.path, "r")
            if file then
                local content = file:read("*a")
                file:close()
                
                if content:find("%-%-%s*#service") then
                    local luaPath = fileData.path:gsub("\\", "/")
                    
                    if cwd ~= "" then
                        luaPath = luaPath:gsub("^" .. cwd, "")
                    end
                    
                    luaPath = luaPath:gsub("^[a-zA-Z]:/", "")
                    luaPath = luaPath:gsub("%.lua$", "")
                    luaPath = luaPath:gsub("^%.%/", "")
                    luaPath = luaPath:gsub("/", ".")
                    
                    foundPath = luaPath
                    break
                end
            end
        end
    end

    if not foundPath then 
        error("Service '" .. tostring(actual) .. "' could not be auto-discovered recursively. Ensure it has the `-- #service` tag at the bottom.") 
    end

    local success, module = pcall(require, foundPath)
    if not success then error("Failed to load discovered service: " .. tostring(module)) end

    local instance = type(module) == "table" and type(module.new) == "function" and module.new() or module
    
    self._cache[actual] = instance
    return instance
end

-- =========================================================================
-- GLOBAL FRAMEWORK INTERFACES
-- =========================================================================
setmetatable(MainKit, {
    __newindex = function(table, key, value)
        if key == "CoreDebugEnabled" and type(value) == "boolean" then
            MainKit.debugMode = value
        else
            rawset(table, key, value)
        end
    end,
    __index = function(table, key)
        if key == "CoreDebugEnabled" then return MainKit.debugMode end
        return rawget(table, key)
    end
});

local function UploadTypeData()
    InstanceType.SetType(Event, "Event")
end

local function call(_, env)
    UploadTypeData(); return {
        MainKit = MainKit,
        Event = Event,
        task = task,
        lenum = lenum,
        sleep = sleep,
        colorPrint = colorPrint,
        PlayerDataModel = PlayerManager,

        ---@overload fun(selfObj: table, serviceName: "StorageService"): StorageService
        ---@overload fun(selfObj: table, serviceName: "RunnerService"): RunnerService
        ---@overload fun(selfObj: table, serviceName: "SoundService"): SoundService
        ---@overload fun(selfObj: table, serviceName: "InputService"): InputService
        ---@overload fun(selfObj: table, serviceName: string): any
        LoadService = function(selfObj, serviceName)
            return MainKit:ResolveService(serviceName)
        end,
        
        --- Fetches execution flag hashes parsed 
        --- **OUT** from the system terminal environment.
        ---@return table<string, any>
        Load = function(selfObj)
            return MainKit:Load()
        end,
        
        -- Instance Type Utilities
        InstanceType = InstanceType,
        typeof = InstanceType.GetType,
        IsA = InstanceType.IsA,
        
        -- Shortcuts
        newEvent = Event.new,
        wait = task.wait,
        seed = task.seed,
    };
end

PlayerManager.core = call()
return { call = call }
