-- #service

local InstanceType = require("instance_type")

local cpath_patterns = {
    "./build/?.dylib",          "./build/lib?.dylib",
    "./build/Release/?.dylib",  "./build/Release/lib?.dylib",
    "./build/Debug/?.dylib",    "./build/Debug/lib?.dylib",
    "./build/bin/?.dylib",      "./build/bin/lib?.dylib",
    "./build/?.so",             "./build/lib?.so",
    "./build/Release/?.so",     "./build/Release/lib?.so",
    "./build/?.dll",            "./build/Release/?.dll"
};

package.cpath = package.cpath .. ";" .. table.concat(cpath_patterns, ";")

---@class InputNativeLib
---@field is_key_down fun(keyCode: integer): boolean
---@field is_key_pressed fun(keyCode: integer): boolean
---@field is_key_released fun(keyCode: integer): boolean
---@field get_mouse_position fun(): number, number
---@field get_mouse_delta fun(): number, number
---@field set_global_input fun(isGlobal: boolean)
---@field update fun()

---@type boolean, InputNativeLib|string
local native_ok, input_native = pcall(require, "input_native")

---@type boolean
local IS_LOVE = (_G.love ~= nil) and (_G.love.keyboard ~= nil)

if not native_ok then
    print("\27[33m[InputService Warning]\27[0m Could not load input_native binary.")
    print("\27[36m[InputService Debug Error]\27[0m " .. tostring(input_native))
end

--- Mapping of key names to Virtual Key codes (VK Codes)
---@type table<string, integer>
local KEY_MAP = {
    ["a"] = 65, ["b"] = 66, ["c"] = 67, ["d"] = 68, ["e"] = 69, ["f"] = 70,
    ["g"] = 71, ["h"] = 72, ["i"] = 73, ["j"] = 74, ["k"] = 75, ["l"] = 76,
    ["m"] = 77, ["n"] = 78, ["o"] = 79, ["p"] = 80, ["q"] = 81, ["r"] = 82,
    ["s"] = 83, ["t"] = 84, ["u"] = 85, ["v"] = 86, ["w"] = 87, ["x"] = 88,
    ["y"] = 89, ["z"] = 90,

    ["0"] = 48, ["1"] = 49, ["2"] = 50, ["3"] = 51, ["4"] = 52,
    ["5"] = 53, ["6"] = 54, ["7"] = 55, ["8"] = 56, ["9"] = 57,

    ["space"]  = 32, 
    ["return"] = 13, 
    ["enter"]  = 13, 
    ["escape"] = 27, 
    ["tab"]    = 9,
    ["shift"]  = 16, 
    ["ctrl"]   = 17, 
    ["alt"]    = 18,

    ["super"]  = 91, ["win"]   = 91, ["cmd"]   = 91, ["meta"]  = 91,
    ["lsuper"] = 91, ["lwin"]  = 91, ["lcmd"]  = 91, ["lmeta"] = 91,
    ["rsuper"] = 92, ["rwin"]  = 92, ["rcmd"]  = 92, ["rmeta"] = 92,

    ["left"]  = 37, ["up"]    = 38, ["right"] = 39, ["down"]  = 40,
    ["mouse1"] = 1, ["mouse2"] = 2, ["mouse3"] = 3
}

local LOVE_KEY_MAP = {
    ["super"] = "lgui", ["win"] = "lgui", ["cmd"] = "lgui", ["meta"] = "lgui",
    ["lsuper"] = "lgui", ["lwin"] = "lgui", ["lcmd"] = "lgui", ["lmeta"] = "lgui",
    ["rsuper"] = "rgui", ["rwin"] = "rgui", ["rcmd"] = "rgui", ["rmeta"] = "rgui"
}

---@alias ActionCallback fun(actionName: string, state: "Pressed" | "Released", keyName: string)

---@class ActionBinding
---@field keyName string
---@field callback? ActionCallback

---@type table<InputService, boolean>
local active_instances = setmetatable({}, { __mode = "k" })

---@class InputService
---@field _actions table<string, ActionBinding>
---@field _isGlobal boolean
local InputService = {}
InputService.__index = InputService
InstanceType.SetType(InputService, "InputService")

function InputService.new()
    local self = setmetatable({}, InputService)
    self._actions = {}
    self._isGlobal = true
    active_instances[self] = true
    return self
end

function InputService:SetGlobalInput(isGlobal)
    self._isGlobal = isGlobal
    if native_ok and type(input_native) == "table" and type(input_native.set_global_input) == "function" then
        input_native.set_global_input(isGlobal)
    end
end

function InputService:GetGlobalInput()
    return self._isGlobal
end

function InputService:_ResolveTarget(target)
    local binding = self._actions[target]
    local keyName = binding and binding.keyName or target:lower()
    return keyName, KEY_MAP[keyName]
end

function InputService:BindAction(actionName, keyName, callback)
    self._actions[actionName] = {
        keyName = keyName:lower(),
        callback = callback
    }
end

function InputService:UnbindAction(actionName)
    self._actions[actionName] = nil
end

function InputService:IsKeyDown(target)
    local keyName, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_down) == "function" then
        return input_native.is_key_down(keyCode)
    elseif IS_LOVE then
        if keyName:find("mouse") then
            local btnIndex = tonumber(keyName:match("%d+")) or 1
            return love.mouse.isDown(btnIndex)
        else
            local loveKey = LOVE_KEY_MAP[keyName] or keyName
            ---@cast loveKey love.KeyConstant
            return love.keyboard.isDown(loveKey)
        end
    end
    return false
end

function InputService:IsKeyPressed(target)
    local _, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_pressed) == "function" then
        return input_native.is_key_pressed(keyCode)
    end
    return false
end

function InputService:IsKeyReleased(target)
    local _, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_released) == "function" then
        return input_native.is_key_released(keyCode)
    end
    return false
end

function InputService:GetMousePosition()
    if native_ok and type(input_native) == "table" and type(input_native.get_mouse_position) == "function" then
        return input_native.get_mouse_position()
    elseif IS_LOVE then
        return love.mouse.getPosition()
    end
    return 0, 0
end

function InputService:GetMouseDelta()
    if native_ok and type(input_native) == "table" and type(input_native.get_mouse_delta) == "function" then
        return input_native.get_mouse_delta()
    end
    return 0, 0
end

function InputService:_ProcessCallbacks()
    for actionName, binding in pairs(self._actions) do
        if binding.callback then
            if self:IsKeyPressed(actionName) then
                binding.callback(actionName, "Pressed", binding.keyName)
            elseif self:IsKeyReleased(actionName) then
                binding.callback(actionName, "Released", binding.keyName)
            end
        end
    end
end

function InputService:Update()
    if native_ok and type(input_native) == "table" and type(input_native.update) == "function" then
        input_native.update()
    end
    self:_ProcessCallbacks()
end

function InputService.UpdateAll()
    if native_ok and type(input_native) == "table" and type(input_native.update) == "function" then
        input_native.update()
    end
    for instance in pairs(active_instances) do
        instance:_ProcessCallbacks()
    end
end

function InputService.HookLove()
    if not IS_LOVE then return end
    local prevUpdate = love.update
    love.update = function(dt)
        InputService.UpdateAll()
        if prevUpdate then
            prevUpdate(dt)
        end
    end
end

-- #service
return InputService
