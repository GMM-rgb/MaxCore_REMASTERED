local CoreLib <const> = require("max_core").call()
local SoundService = CoreLib:LoadService("SoundService")
local StorageManager = CoreLib:LoadService("StorageService")
local RuntimeService = CoreLib:LoadService("RunnerService")
local InputListener = CoreLib:LoadService("InputService")
local QeuryingPlayer = CoreLib.Event.new("QeuryingPlayerDataModelEvent")
local PlayerInstanced = CoreLib.Event.new("PlayerInstancedEvent")
local PreviousMouseLocation <const> = StorageManager
    :GetFile("./logs/mouse_logging.log"):Read()

local MessageDictionary = {
    [1] = "Hello World! This is a TEST message.",
};

---@param str string
---@param sep string?
---@return table<string>
function string.split(str, sep)
  sep = sep or "%s"
  local t = {}

  if sep == "" then
    for char in string.gmatch(str, ".") do
      table.insert(t, char)
    end; return t
  end

  for word in string.gmatch(str, "([^" .. sep .. "]+)") do
    table.insert(t, word)
  end

  return t
end

---@return string
local function random_message()
    math.randomseed(math.ceil(os.time()))
    return MessageDictionary[math.random(1, #MessageDictionary)]
end

function TypeRandomMessage()
    local PickedMessage = string.split(random_message(), "")

    for _, v in pairs(PickedMessage) do
        InputListener:SimulateKey(v, true)
        InputListener:SimulateKey(v, false)
        CoreLib.task.wait(0.1)
    end
end

InputListener:BindAction("StartRandomMessage", "up", function(name, state, key)
    TypeRandomMessage()
end)

---@class game
---@field _objects table<any>
---@field _events table<Event>
local game = {
    _objects = {},
    _events = {},
};

---@param AppendingData table
---@param TargetPlayerObject PlayerObject
local function ApplyPlayerDataField(TargetPlayerObject, AppendingData)
    TargetPlayerObject["data"] = nil
end

if CoreLib.IsA(QeuryingPlayer, "Event") and not QeuryingPlayer:IsConnected() then
    ---@param TargetName string
    ---@param RecievedData table
    ---@param ActivePermissions PlayerPermissions
    local QueryPlayerConnection = QeuryingPlayer:Connect(function(TargetName, RecievedData, ActivePermissions)
        local NewPlayer = CoreLib.PlayerDataModel.new(TargetName, ActivePermissions, false)
        pcall(ApplyPlayerDataField, NewPlayer, RecievedData)
    end);
end

local function InputListeningTick()
    if PreviousMouseLocation and PreviousMouseLocation ~= nil and type(PreviousMouseLocation) == "string" then
        local FormtttedPreviousLocation = PreviousMouseLocation:match("[^X|Y|:|\t]+")
        print(FormtttedPreviousLocation)
    end
end

---@param dt number
local function GameTickUpdate(dt)
    InputListener:Update(); io.stdout:flush(); io.stdout:setvbuf("line"); local FormatedPosition = string.format(
    "Delata Time:\t%G\nX:\t%G\nY:\t%G", dt, InputListener:GetMousePosition())
    CoreLib.colorPrint("INFO", tostring(FormatedPosition)); for _ = 1, 3 do
        io.stdout:write("\27[1A\27[2K"); collectgarbage("collect")
    end
end

PlayerInstanced:Connect(function()

end)

InputListener:BindAction("binding", "mouse1", function(name, state, key)
    print(name .. ":", state, key)
end)

InputListener:SetGlobalInput(true)
RuntimeService.Stepped:Connect(GameTickUpdate)
RuntimeService.Stepped:Connect(InputListeningTick)
RuntimeService:KeepAlive()

for _, file in pairs(StorageManager:ListDirectory("./") or {}) do
    if file and CoreLib.IsA(file, "FileObject") and file:IsDirectory() then
        if file._name and type(file._name) == "string" and file:GetName() == "logs" then
            if not (math.ceil(#StorageManager:ListDirectory(file._path)) >= 1) then
                StorageManager:DeleteDirectory(file._path)
            end
        end
    end
end

if CoreLib.IsA(StorageManager, "StorageService") then
    StorageManager:CreateDirectory("./logs")
end

local x, y = InputListener:GetMousePosition()
local WriteSuccess = StorageManager:CreateFile({
    path = "./logs/mouse_logging.log",
}):Write(string.format("X:\t%G\nY:\t%G", x, y))
