-- local LuaCore = require("max_core")
-- local Runtime = require("services.runner_service")
-- local PlayerUtilities = {}

-- math.randomseed(os.time() - math.ceil(math.random() * 100))

-- local function RemoveHealthComponent()

-- end

-- PlayerUtilities.PlayerData = {
--     {
--         Name = "player1",
--         HealthChanged = LuaCore.call().Event.new(),
--         RemoveHealth = RemoveHealthComponent,
--         Health = 100,
--     },
--     {
--         Name = "player2",
--         HealthChanged = LuaCore.call().Event.new(),
--         RemoveHealth = RemoveHealthComponent,
--         Health = 100,
--     },
-- };

-- --[=[
--     Extracts Player health status value from the Player Object table.
--     The following function method can accept multiple players; passing to a multi-health table.
-- ]=]
-- ---@param data {[number]: {["Name"]: string, ["Health"]: number}}
-- ---@return {[number]: number}
-- function PlayerUtilities.GetPlayerHealth(data)
--     if not data or type(data) ~= "table" then return {} end

--     ---@type {[number]: number}
--     local PlayerHealths = {}

--     for _, SelectedData in ipairs(data or {}) do
--         for DataName, DataContents in pairs(SelectedData) do
--             if DataName and DataContents and tostring(DataName) == "Health" then
--                 if DataContents ~= nil and type(DataContents) == "number" then
--                     table.insert(PlayerHealths, DataContents)
--                 end
--             end
--         end
--     end

--     return PlayerHealths
-- end

-- ---@param targetName string
-- function PlayerUtilities.PlayerData:NewPlayer(targetName)
--     if self ~= nil and type (self) == "table" then
--         for playerIndex, selectedPlayer in pairs(self) do
--             if selectedPlayer and selectedPlayer ~= "NewPlayer" then
                
--             end
--         end
--     end
-- end

-- coroutine.wrap(function()

-- end)()

-- local targetMeta = (_ENV and _ENV.PlayerUtilities) or {}

-- return setmetatable({
--     PlayerData = PlayerUtilities.PlayerData,
--     GetPlayerHealth = PlayerUtilities.GetPlayerHealth,
-- }, targetMeta)
