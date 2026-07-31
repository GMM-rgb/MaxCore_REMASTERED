local InstanceType = require("instance_type")
---@diagnostic disable-next-line
local ext = (package.config:sub(1,1) == "\\") and "dll" or (jit and jit.os == "OSX" or package.cpath:find("%.dylib") and "dylib" or "so")

package.cpath = package.cpath 
    .. ";./build/?." .. ext
    .. ";./build/Release/?." .. ext
    .. ";./build/Debug/?." .. ext
    .. ";./build/bin/?." .. ext
    .. ";./build/bin/Release/?." .. ext
    .. ";./build/*/?/?." .. ext

local native_ok, input_native = pcall(require, "input_native")
local IS_LOVE = (_G.love ~= nil) and (_G.love.keyboard ~= nil)

if not native_ok and not IS_LOVE then
    print("\27[33m[InputService Warning]\27[0m input_native shared library could not be loaded from build directories.")
end

-- =========================================================================
-- KEY CODE MAPPINGS (String to Virtual Key Code)
-- =========================================================================
local KEY_MAP = {
    ["a"] = 65, ["b"] = 66, ["c"] = 67, ["d"] = 68, ["e"] = 69, ["f"] = 70,
    ["g"] = 71, ["h"] = 72, ["i"] = 73, ["j"] = 74, ["k"] = 75, ["l"] = 76,
    ["m"] = 77, ["n"] = 78, ["o"] = 79, ["p"] = 80, ["q"] = 81, ["r"] = 82,
    ["s"] = 83, ["t"] = 84, ["u"] = 85, ["v"] = 86, ["w"] = 87, ["x"] = 88,
    ["y"] = 89, ["z"] = 90,
    ["0"] = 48, ["1"] = 49, ["2"] = 50, ["3"] = 51, ["4"] = 52,
    ["5"] = 53, ["6"] = 54, ["7"] = 55, ["8"] = 56, ["9"] = 57,
    ["space"] = 32, ["return"] = 13, ["enter"] = 13, ["escape"] = 27, ["tab"] = 9,
    ["shift"] = 16, ["ctrl"] = 17, ["alt"] = 18,
    ["left"] = 37, ["up"] = 38, ["right"] = 39, ["down"] = 40,
    ["mouse1"] = 1, ["mouse2"] = 2, ["mouse3"] = 3
}

-- =========================================================================
-- MAIN INPUT SERVICE CLASS
-- =========================================================================

---@class InputService
---@field _actions table<string, string>
local InputService = {}
InputService.__index = InputService
InstanceType.SetType(InputService, "InputService")

function InputService.new()
    local self = setmetatable({}, InputService)
    self._actions = {}
    return self
end

--- Internal helper to resolve action names or raw key names into key identifiers.
---@param target string
---@return string keyName, number|nil keyCode
function InputService:_ResolveTarget(target)
    local keyName = (self._actions[target] or target):lower()
    return keyName, KEY_MAP[keyName]
end

--- Binds a custom action string to a target key name.
---@param actionName string Name of the action (e.g. "Jump")
---@param keyName string Physical key name (e.g. "space")
function InputService:BindAction(actionName, keyName)
    self._actions[actionName] = keyName:lower()
end

--- Unbinds a previously registered action name.
---@param actionName string
function InputService:UnbindAction(actionName)
    self._actions[actionName] = nil
end

--- Checks if a key or bound action is currently held down.
---@param target string Action name (e.g. "Jump") or key name (e.g. "space")
---@return boolean
function InputService:IsKeyDown(target)
    local keyName, keyCode = self:_ResolveTarget(target)

    if native_ok and keyCode and type(input_native.is_key_down) == "function" then
        return input_native.is_key_down(keyCode)
    elseif IS_LOVE then
        if keyName:find("mouse") then
            local btnIndex = tonumber(keyName:match("%d+")) or 1
            return love.mouse.isDown(btnIndex)
        else
            return love.keyboard.isDown(keyName)
        end
    end
    return false
end

--- Checks if a key or bound action was pressed on THIS exact frame.
---@param target string Action name or key name
---@return boolean
function InputService:IsKeyPressed(target)
    local _, keyCode = self:_ResolveTarget(target)

    if native_ok and keyCode and type(input_native.is_key_pressed) == "function" then
        return input_native.is_key_pressed(keyCode)
    end
    return false
end

--- Checks if a key or bound action was released on THIS exact frame.
---@param target string Action name or key name
---@return boolean
function InputService:IsKeyReleased(target)
    local _, keyCode = self:_ResolveTarget(target)

    if native_ok and keyCode and type(input_native.is_key_released) == "function" then
        return input_native.is_key_released(keyCode)
    end
    return false
end

--- Gets the current mouse cursor coordinates.
---@return number x, number y
function InputService:GetMousePosition()
    if native_ok and type(input_native.get_mouse_position) == "function" then
        return input_native.get_mouse_position()
    elseif IS_LOVE then
        return love.mouse.getPosition()
    end
    return 0, 0
end

--- Gets the relative mouse movement (delta) since the last frame.
---@return number dx, number dy
function InputService:GetMouseDelta()
    if native_ok and type(input_native.get_mouse_delta) == "function" then
        return input_native.get_mouse_delta()
    end
    return 0, 0
end

--- Call this at the END of every game loop iteration to push key states to previous frame.
function InputService:Update()
    if native_ok and type(input_native.end_frame) == "function" then
        input_native.end_frame()
    end
end

-- #service
return InputService
