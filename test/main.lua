local utf = require("utf8")
local CoreLib = require("max_core").call()
local RunnerService = CoreLib:LoadService("RunnerService")
local StorageService = CoreLib:LoadService("StorageService")
local SessionPlayer = CoreLib.PlayerDataModel.new("player123", nil, false)
local FileListing = StorageService:GetFile("./file_mappings.json"):GetContents()
local PlayerDataChange = CoreLib.Event.new("PlayerDataChanged")

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
    contents = FileListing,
    name = "file_mappings",
    extension = "json",
    directory = "./",
});

local MappingFileName = MappingBinaryFile and MappingBinaryFile:GetName()
if not MappingFileName or MappingFileName == nil or type(MappingFileName) ~= "string" then return end
print("MODIFIED WHEN:\t" .. os.date("%Y-%m-%d %H:%M:%S", StorageService:GetFile(MappingFileName):GetModifiedTime()))

---@param TargetPlayer PlayerObject
---@param PlayerContents table
local function WritePlayerData(TargetPlayer, PlayerContents)
    for PlayerDataKey, SelectedData in pairs(PlayerContents) do
        
    end
end

function TickGame(delta)
    io.stdout:setvbuf("line", 128); io.stdout:flush()
    io.stdout:write("\r" .. string.format("DELTA TIME: %G", delta))
end

RunnerService.Stepped:Connect(TickGame)
RunnerService:KeepAlive()
