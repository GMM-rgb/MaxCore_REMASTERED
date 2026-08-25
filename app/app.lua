local core <const> = require("max_core").call()
local BoostChanged = core.Event.new("BoostChanged")
local storage = core:LoadService("StorageService")
local runtime = core:LoadService("RunnerService")
local sound = core:LoadService("SoundService")
local input = core:LoadService("InputService")
local IncrementBoost = false
local IncrementIndex = 0.1

sound:SetStorageService(storage)
sound:SetCacheFolder("../audio")

local GameData <const> = storage:CreateFile({
    path = "./data.txt",
    debug = true
});

---@class AppData
---@field objects table<Instance, unknown>
---@field events table<Event>
local data = {
    objects = {
        ---@type table<SoundObject>
        sound = {
            
        },
    },
    events = {},
};

if GameData ~= nil and core.typeof(GameData) == "FileObject" then
    
end

local function ChecksumGameData()
    local ErrorReason = string.char(0) for DataIndex, DataValue in pairs(data) do
        return
    end io.stderr:write(string.format("[Game Debug]: fatal, checksum failed. \n [Reason]:\t%s", ErrorReason))
end

runtime.Stepped:Connect(function(dt)
    if input:GetGlobalInput() ~= true then
        input:SetGlobalInput(true)
    else
        ChecksumGameData()
        input:UpdateAll()
    end
end); runtime:KeepAlive()
