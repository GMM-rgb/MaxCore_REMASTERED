local EngineCore = require("max_core").call(); EngineCore:Load()
local WindowService = EngineCore:LoadService("WindowService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local PropertyWindow = WindowService:CreateWindow(nil, 300, 700)
local ExitEvent = EngineCore.Event.new("exit-program-event")

if not PropertyWindow or EngineCore.typeof(PropertyWindow) ~= "WindowObject" then
    return
end

local function new_button()

end

local screen_xd, screen_yd = PropertyWindow:GetDimensions()
local active_properties = {}
local outline_configuration = {
    { 20, 20, screen_xd - 40, 20 },
    { 20, screen_yd - 40, screen_xd - 40, screen_yd - 40}
};

local function instance_border()
    for _, v in ipairs(outline_configuration) do
        local outline = PropertyWindow:CreateLine(table.unpack(v))
        if outline:typeof() ~= "GameObject" then goto continue end
        PropertyWindow:RenderObject(outline) ::continue::
    end
end

if PropertyWindow then
    instance_border()
end

RuntimeService.Stepped:Connect(function()
    if not PropertyWindow then return end
    if not PropertyWindow:IsRunning() then ExitEvent:Fire(nil, true) end
    PropertyWindow:SwapBuffers()
end)

ExitEvent:Connect(os.exit)
RuntimeService:KeepAlive()
