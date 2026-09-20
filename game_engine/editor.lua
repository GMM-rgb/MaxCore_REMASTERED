local core <const> = require("max_core").call()
local window = core:LoadService("WindowService")
local runtime = core:LoadService("RunnerService")
local game = window:CreateWindow("GAME", 850, 800)
local NoiseGeneration = core.NoiseClass.new()

-- print(NoiseGeneration:Sample2D(20, 20))

local function TickGame()
    if game ~= nil then
        game:IsRunning()
        game:SwapBuffers()
    end
end

runtime.Stepped:Connect(TickGame)
runtime:KeepAlive()
