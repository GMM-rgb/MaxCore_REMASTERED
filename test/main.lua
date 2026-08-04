local utf = require("utf8")
local CoreLib = require("max_core").call()
local RunnerService = CoreLib:LoadService("RunnerService")
local StorageService = CoreLib:LoadService("StorageService")
local PlayerDataTemp = StorageService:GetFile("./player_data/player_data_template.json")
local SessionPlayer = CoreLib.PlayerDataModel.new("player123", nil, false)
local PlayerDataChange = CoreLib.newEvent("PlayerDataChanged")
local StartupApplication = CoreLib.newEvent("GameStartup")

for _, CharCode in utf.codes("A") do
    print("CHAR:", utf.char(CharCode))
    print("ASCII:", CharCode)
end

---@class GameDataModel
---@field workspace table
---@field files table<FileObject>

---@type GameDataModel
local GameApplication = {
    workspace = {},
    files = {},
};

---@param OptionalFiles table<FileObject>
local function LoadRequiredFiles(OptionalFiles)
    ---@type table<FileObject>
    local TargetFiles = {}

    if OptionalFiles ~= nil and type(OptionalFiles) == "table" then
        for _, file in pairs(OptionalFiles) do
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

function TickGame(delta)
    io.stdout:setvbuf("line", 128); io.stdout:flush()
    io.stdout:write("\r" .. string.format("DELTA TIME: %G", delta))
end

StartupApplication:Connect(LoadRequiredFiles)
PlayerDataChange:Connect(WritePlayerData)
RunnerService.Stepped:Connect(TickGame)
StartupApplication:Fire({})
RunnerService:KeepAlive()
