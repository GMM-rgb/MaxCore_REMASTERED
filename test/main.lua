local utf = require("utf8")
local CoreLib = require("max_core").call()
local InputService = CoreLib:LoadService("InputService")
local RunnerService = CoreLib:LoadService("RunnerService")
local StorageService = CoreLib:LoadService("StorageService")
local PlayerDataTemp = StorageService:GetFile("./player_data/player_data_template.json")
local SessionPlayer = CoreLib.PlayerDataModel.new("player123", nil, false)
local PlayerDataChange = CoreLib.newEvent("PlayerDataChanged")
local StartupApplication = CoreLib.newEvent("GameStartup")

InputService:SetGlobalInput(true)

-- for _, CharCode in utf.codes("A") do
--     print("CHAR:", utf.char(CharCode))
--     print("ASCII:", CharCode)
-- end

---@param actionName string
---@
InputService:BindAction("testing", "mouse1", function(actionName, state, keyName)
    print("\nKEY:", keyName, "\nSTATUS:", state)
end)

---@class GameDataModel
---@field workspace table
---@field _events {[string]: Event}
---@field files table<FileObject>

---@type GameDataModel
local GameApplication = {
    files = {},
    workspace = {},
    _events = {
        [StartupApplication:GetEventName()] = StartupApplication,
    },
};

---@return Event?
---@param TargetEventName string
local function GetGameEvent(TargetEventName)
    for EventName, EventObject in pairs(GameApplication and GameApplication._events or {}) do
        if EventName ~= nil and type(EventName) == "string" and EventObject ~= nil and CoreLib.IsA(EventObject, "Event") then
            if TargetEventName and type(TargetEventName) == "string" and tostring(TargetEventName) == EventName then return EventObject end
        end; goto continue; ::continue::
    end; return nil
end

---@param OptionalFiles table<FileObject>
local function LoadRequiredFiles(OptionalFiles)
    ---@type table<FileObject>
    local TargetFiles = {}

    if OptionalFiles ~= nil and type(OptionalFiles) == "table" then
        for _, file in ipairs(OptionalFiles) do
            if CoreLib.IsA(file, "FileObject") then
                TargetFiles[file:GetName()] = file
            end
        end
    end

    for _, LoadingFile in ipairs(TargetFiles or {}) do
        table.insert(GameApplication.files, LoadingFile)
    end
end

local MappingBinaryFile = StorageService:CreateFile({
    contents = string.format('{\n\t"mappings": [%s]\n}\r', [[null]]),
    directory = "./player_data",
    name = "file_mappings",
    extension = "json",
});

local MappingFileName = MappingBinaryFile and MappingBinaryFile:GetName()
if not MappingFileName or MappingFileName == nil or type(MappingFileName) ~= "string" then return end
print("MODIFIED WHEN:\t" .. os.date("%Y-%m-%d %H:%M:%S", StorageService:GetFile(MappingFileName):GetModifiedTime()))

---@param PlayerContents table
---@param TargetPlayer PlayerObject
local function WritePlayerData(TargetPlayer, PlayerContents)
    if TargetPlayer ~= nil and CoreLib.PlayerDataModel.PlayerExists(TargetPlayer) then
        for PlayerDataKey, SelectedData in pairs(PlayerContents or table.create(0, 0) or {}) do
            if PlayerDataKey ~= nil and SelectedData ~= nil and type(PlayerDataKey) == "string" then
                local _config = { extension = "json", name = TargetPlayer:GetName(), directory = "./player_data" }
                local DataFile = StorageService:CreateFile(_config) or StorageService:GetFile("./player_data" .. TargetPlayer:GetName() .. ".json")
                if not DataFile or not CoreLib.IsA(DataFile, "FileObject") then return end
                DataFile:Write(tostring())
            end
        end
    end
end

local function TickMainGame()

end

function TickGame(delta)
    xpcall(TickMainGame, function(...)
        
    end); InputService:Update()
end

StartupApplication:Connect(LoadRequiredFiles)
PlayerDataChange:Connect(WritePlayerData)
RunnerService.Stepped:Connect(TickGame)
StartupApplication:Fire({})
RunnerService:KeepAlive()
