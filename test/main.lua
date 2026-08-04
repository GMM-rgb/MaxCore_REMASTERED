local CoreLib = require("max_core").call()
local RunnerService = CoreLib:LoadService("RunnerService")
local StorageService = CoreLib:LoadService("StorageService")
local SessionPlayer = CoreLib.PlayerDataModel.new("player123", nil, false)
local FileListing = StorageService:GetFile("./file_mappings.json"):GetContents()

-- local utf8lib = require("utf8")

-- local combined = string.char(0)

-- for i, v in utf8lib.codes("My favorite B day class is English. I like it because of how my friends are in there with me as well; moreover the teacher is a really nice guy.") do
--     local CurrentSepperator = i > 1 and "-" or ""
--     combined = combined .. CurrentSepperator .. v
-- end

-- print(combined)

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
        for _, file in ipairs(OptionalFiles) do
            if CoreLib.IsA(file, "FileObject") then
                TargetFiles[file:GetName()] = file
            end
        end
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
