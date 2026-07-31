-- local LuaExtensionCore = require("max_core").call()
-- local PlayerUtilities = require("player_utilities")
-- local VectorUtilites = require("vector_utils")
-- local TargetPlayers = { PlayerUtilities.PlayerData[1], PlayerUtilities.PlayerData[2] }
-- local PlayerScanEvent = LuaExtensionCore and LuaExtensionCore.Event.new()
-- local SoundService = LuaExtensionCore:LoadService("SoundService")
-- local RunService = LuaExtensionCore:LoadService("RunnerService")

-- local sound = SoundService:LoadSound("Roblox OOF Song.wav")

-- print(sound)
-- print(sound.__index)
-- print(sound._path)

-- sound:Play()

-- ---@class SqaureInstancer
-- ---@field x number
-- ---@field y number

-- if _G.love and love.window then
--     local x, y = love.window.getDesktopDimensions()
--     local icon = love.image.newImageData("game_application_icon.png")
--     love.window.setTitle("GAME")
--     love.window.setIcon(icon)
--     love.window.setMode(x / 2, y / 2 - 60, {
--         vsync = true,
--         resizable = true,
--         minheight = y / 3,
--         minwidth = x / 4,
--     });
-- end

-- function love.load()
--     love.graphics.setDefaultFilter("nearest", "nearest")
-- end

-- ---@param CenterOrigin SqaureInstancer
-- local function CreateSqaureMesh(CenterOrigin, r, g, b)
--     love.graphics.setColor(r, g, b)
--     love.graphics.setLineWidth(4)
--     love.graphics.setLineJoin("bevel")
--     love.graphics.setLineStyle("smooth")

--     local rectArgs = { "fill", CenterOrigin.x, CenterOrigin.y, 100, 100 }

--     ---@diagnostic disable-next-line: param-type-mismatch
--     love.graphics.rectangle(unpack(rectArgs))
-- end

-- function love.draw()
--     local cx, cy = love.graphics.getWidth() / 2, love.graphics.getHeight() / 2

--     love.graphics.push()
--     love.graphics.translate(cx, cy)

--     CreateSqaureMesh({x = -50, y = -50}, 1, 1, 1)

--     love.graphics.pop()
-- end

-- function love.quit()
--     RunService:destroy()
-- end
