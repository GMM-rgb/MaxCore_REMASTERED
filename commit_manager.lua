local StorageService = EngineCore:LoadService("StorageService")
local RuntimeService = EngineCore:LoadService("RunnerService")
local WindowService = EngineCore:LoadService("WindowService")
local ResX, ResY = WindowService:GetDisplayResolution()
local divider_string = string.rep("=", math.ceil(100 % ResX))
local borders_string = string.rep(utf8.char(32), math.ceil(100 % ResX))
local target_index = #borders_string
local starting_index = 1

for si = starting_index, target_index do
    local ending = si >= target_index
    local starting = si <= starting_index
    local valid = ending or starting

    if valid and type(borders_string) == "string" then
        borders_string = borders_string:gsub("%s", "|")
    end
end

local display_menu_components = {
    ["start"] = divider_string,
    ["end"] = divider_string,
    ["edges"] = divider_string,
}

print(display_menu_components["start"])

local function create_display_menu()

end

EngineCore.ColorPrint("INFO", "STARTED...", RuntimeService._running)

RuntimeService:KeepAlive()
