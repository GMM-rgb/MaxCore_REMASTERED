local InstanceType = require("instance_type")

local cpath_patterns = {
    "./build/?.dylib",          "./build/lib?.dylib",
    "./build/Release/?.dylib",  "./build/Release/lib?.dylib",
    "./build/Debug/?.dylib",    "./build/Debug/lib?.dylib",
    "./build/bin/?.dylib",      "./build/bin/lib?.dylib",
    "./build/?.so",             "./build/lib?.so",
    "./build/Release/?.so",     "./build/Release/lib?.so",
    "./build/Debug/?.so",       "./build/Debug/lib?.so",
    "./build/bin/?.so",         "./build/bin/lib?.so",
    "./?.dll",                  "./lib?.dll",
    "./build/?.dll",            "./build/lib?.dll",
    "./build/Release/?.dll",    "./build/Release/lib?.dll",
    "./build/Debug/?.dll",      "./build/Debug/lib?.dll",
    "./build/bin/?.dll",        "./build/bin/lib?.dll"
};

package.cpath = package.cpath .. ";" .. table.concat(cpath_patterns, ";")

-- #service
---@class InputNativeLib
---@field get_key_code fun(keyName: string): integer
---@field is_key_down fun(keyCode: integer): boolean
---@field is_key_pressed fun(keyCode: integer): boolean
---@field is_key_released fun(keyCode: integer): boolean
---@field get_mouse_position fun(): number, number
---@field get_mouse_delta fun(): number, number
---@field set_global_input fun(isGlobal: boolean)
---@field update fun()
---@field simulate_key fun(keyCode: integer, isDown: boolean)
---@field simulate_mouse_move fun(x: number, y: number, absolute?: boolean)
---@field toggle_lock_key fun(keyCode: integer)

---@alias KeyName
---| "a"
---| "b"
---| "c"
---| "d"
---| "e"
---| "f"
---| "g"
---| "h"
---| "i"
---| "j"
---| "k"
---| "l"
---| "m"
---| "n"
---| "o"
---| "p"
---| "q"
---| "r"
---| "s"
---| "t"
---| "u"
---| "v"
---| "w"
---| "x"
---| "y"
---| "z"
---| "0"
---| "1"
---| "2"
---| "3"
---| "4"
---| "5"
---| "6"
---| "7"
---| "8"
---| "9"
---| "tab"
---| "space"
---| "return"
---| "enter"
---| "escape"
---| "backspace"
---| "delete"
---| "insert"
---| "home"
---| "end"
---| "pageup"
---| "pagedown"
---| "f1"
---| "f2"
---| "f3"
---| "f4"
---| "f5"
---| "f6"
---| "f7"
---| "f8"
---| "f9"
---| "f10"
---| "f11"
---| "f12"
---| "capslock"
---| "numlock"
---| "scrolllock"
---| "shift"
---| "ctrl"
---| "alt"
---| "lshift"
---| "rshift"
---| "lctrl"
---| "rctrl"
---| "lalt"
---| "ralt"
---| "super"
---| "win"
---| "cmd"
---| "meta"
---| "lsuper"
---| "lwin"
---| "lcmd"
---| "lmeta"
---| "rsuper"
---| "rwin"
---| "rcmd"
---| "rmeta"
---| "left"
---| "up"
---| "right"
---| "down"
---| "minus"
---| "equals"
---| "leftbracket"
---| "rightbracket"
---| "backslash"
---| "semicolon"
---| "apostrophe"
---| "grave"
---| "comma"
---| "period"
---| "slash"
---| "mouse1"
---| "mouse2"
---| "mouse3"

---@type boolean, InputNativeLib|string
local native_ok, input_native = pcall(require, "input_native")

if not native_ok then
    print("\27[33m[InputService Warning]\27[0m Could not load input_native binary.")
    print("\27[36m[InputService Debug Error]\27[0m " .. tostring(input_native))
end

--- Maps supported key names to Windows Virtual-Key codes.
---@type table<string, integer>
local KEY_MAP = {
    -- Letters
    ["a"] = 65, ["b"] = 66, ["c"] = 67, ["d"] = 68, ["e"] = 69, ["f"] = 70,
    ["g"] = 71, ["h"] = 72, ["i"] = 73, ["j"] = 74, ["k"] = 75, ["l"] = 76,
    ["m"] = 77, ["n"] = 78, ["o"] = 79, ["p"] = 80, ["q"] = 81, ["r"] = 82,
    ["s"] = 83, ["t"] = 84, ["u"] = 85, ["v"] = 86, ["w"] = 87, ["x"] = 88,
    ["y"] = 89, ["z"] = 90,

    -- Numbers
    ["0"] = 48, ["1"] = 49, ["2"] = 50, ["3"] = 51, ["4"] = 52,
    ["5"] = 53, ["6"] = 54, ["7"] = 55, ["8"] = 56, ["9"] = 57,

    -- Common keys
    ["tab"] = 0x09,
    ["space"] = 0x20,
    ["return"] = 0x0D,
    ["enter"] = 0x0D,
    ["escape"] = 0x1B,
    ["backspace"] = 0x08,
    ["delete"] = 0x2E,
    ["insert"] = 0x2D,
    ["home"] = 0x24,
    ["end"] = 0x23,
    ["pageup"] = 0x21,
    ["pagedown"] = 0x22,

    -- Function keys
    ["f1"] = 0x70,
    ["f2"] = 0x71,
    ["f3"] = 0x72,
    ["f4"] = 0x73,
    ["f5"] = 0x74,
    ["f6"] = 0x75,
    ["f7"] = 0x76,
    ["f8"] = 0x77,
    ["f9"] = 0x78,
    ["f10"] = 0x79,
    ["f11"] = 0x7A,
    ["f12"] = 0x7B,

    -- Lock keys
    ["capslock"] = 0x14,
    ["numlock"] = 0x90,
    ["scrolllock"] = 0x91,

    -- Generic modifiers
    ["shift"] = 0x10,
    ["ctrl"] = 0x11,
    ["alt"] = 0x12,

    -- Left/right modifiers
    ["lshift"] = 0xA0,
    ["rshift"] = 0xA1,
    ["lctrl"] = 0xA2,
    ["rctrl"] = 0xA3,
    ["lalt"] = 0xA4,
    ["ralt"] = 0xA5,

    -- Super / Windows / Command / Meta aliases
    ["super"] = 0x5B,
    ["win"] = 0x5B,
    ["cmd"] = 0x5B,
    ["meta"] = 0x5B,
    ["lsuper"] = 0x5B,
    ["lwin"] = 0x5B,
    ["lcmd"] = 0x5B,
    ["lmeta"] = 0x5B,
    ["rsuper"] = 0x5C,
    ["rwin"] = 0x5C,
    ["rcmd"] = 0x5C,
    ["rmeta"] = 0x5C,

    -- Arrow keys
    ["left"] = 0x25,
    ["up"] = 0x26,
    ["right"] = 0x27,
    ["down"] = 0x28,

    -- Punctuation (US keyboard layout)
    ["minus"] = 0xBD,
    ["equals"] = 0xBB,
    ["leftbracket"] = 0xDB,
    ["rightbracket"] = 0xDD,
    ["backslash"] = 0xDC,
    ["semicolon"] = 0xBA,
    ["apostrophe"] = 0xDE,
    ["grave"] = 0xC0,
    ["comma"] = 0xBC,
    ["period"] = 0xBE,
    ["slash"] = 0xBF,

    -- Mouse buttons (reserved state indices in the native module)
    ["mouse1"] = 1,
    ["mouse2"] = 2,
    ["mouse3"] = 3,
};

---@alias InputActionState
---| "Pressed"
---| "Released"

---@alias ActionCallback fun(actionName: string, state: InputActionState, keyName: KeyName)

---@class ActionBinding
---@field keyName KeyName
---@field callback? ActionCallback

---@type table<InputService, boolean>
local active_instances = setmetatable({}, { __mode = "k" })

---@class InputService
---@field _actions table<string, ActionBinding>
---@field _isGlobal boolean
---@field inputKeys KeyName[]
local InputService = {}
InputService.__index = InputService
InstanceType.SetType(InputService, "InputService")

---@type KeyName[]
InputService.inputKeys = {
    -- Letters
    "a", "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k", "l",
    "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x",
    "y", "z",

    -- Numbers
    "0", "1", "2", "3", "4",
    "5", "6", "7", "8", "9",

    -- Common keys
    "tab", "space", "return", "enter", "escape",
    "backspace", "delete", "insert", "home", "end",
    "pageup", "pagedown",

    -- Function keys
    "f1", "f2", "f3", "f4", "f5", "f6",
    "f7", "f8", "f9", "f10", "f11", "f12",

    -- Lock keys
    "capslock", "numlock", "scrolllock",

    -- Modifiers
    "shift", "ctrl", "alt",
    "lshift", "rshift",
    "lctrl", "rctrl",
    "lalt", "ralt",

    -- Super / Windows / Command / Meta
    "super", "win", "cmd", "meta",
    "lsuper", "lwin", "lcmd", "lmeta",
    "rsuper", "rwin", "rcmd", "rmeta",

    -- Arrows
    "left", "up", "right", "down",

    -- Punctuation
    "minus", "equals",
    "leftbracket", "rightbracket",
    "backslash", "semicolon", "apostrophe",
    "grave", "comma", "period", "slash",

    -- Mouse buttons
    "mouse1", "mouse2", "mouse3",
};

function InputService.new()
    local self = setmetatable({}, InputService)
    self._actions = {}
    self._isGlobal = true
    active_instances[self] = true
    return self
end

---@param self InputService
---@param isGlobal boolean
function InputService:SetGlobalInput(isGlobal)
    if type(isGlobal) ~= "boolean" then
        return
    end

    self._isGlobal = isGlobal

    if native_ok and type(input_native) == "table" and type(input_native.set_global_input) == "function" then
        input_native.set_global_input(isGlobal)
    end
end

---@param self InputService
---@return boolean
function InputService:GetGlobalInput()
    return self._isGlobal
end

---@param self InputService
---@param target string
---@return string, integer?
function InputService:_ResolveTarget(target)
    local binding = self._actions[target]
    local keyName = binding and binding.keyName or target:lower()
    return keyName, KEY_MAP[keyName]
end

---@param self InputService
---@param actionName string
---@param keyName KeyName
---@param callback ActionCallback
function InputService:BindAction(actionName, keyName, callback)
    assert(type(actionName) == "string" and actionName ~= "", "actionName must be a non-empty string")
    assert(type(keyName) == "string", "keyName must be a string")
    assert(type(callback) == "function", "callback must be a function")

    local normalizedKey = keyName:lower()
    assert(KEY_MAP[normalizedKey] ~= nil, "unsupported key name: " .. normalizedKey)
    ---@cast normalizedKey KeyName

    self._actions[actionName] = {
        keyName = normalizedKey,
        callback = callback,
    };
end

---@param self InputService
---@param actionName string
function InputService:UnbindAction(actionName)
    self._actions[actionName] = nil
end

---@param self InputService
---@param target KeyName | string
---@return boolean
function InputService:IsKeyDown(target)
    local _, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_down) == "function" then
        return input_native.is_key_down(keyCode)
    end
    return false
end

---@param self InputService
---@param target KeyName | string
---@return boolean
function InputService:IsKeyPressed(target)
    local _, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_pressed) == "function" then
        return input_native.is_key_pressed(keyCode)
    end
    return false
end

---@param self InputService
---@param target KeyName | string
---@return boolean
function InputService:IsKeyReleased(target)
    local _, keyCode = self:_ResolveTarget(target)
    if native_ok and keyCode and type(input_native) == "table" and type(input_native.is_key_released) == "function" then
        return input_native.is_key_released(keyCode)
    end
    return false
end

---@param self InputService
---@return number, number
function InputService:GetMousePosition()
    if native_ok and type(input_native) == "table" and type(input_native.get_mouse_position) == "function" then
        return input_native.get_mouse_position()
    end
    return 0, 0
end

---@param self InputService
---@return number, number
function InputService:GetMouseDelta()
    if native_ok and type(input_native) == "table" and type(input_native.get_mouse_delta) == "function" then
        return input_native.get_mouse_delta()
    end
    return 0, 0
end

--- Simulates a key press or release event at the OS level.
---@overload fun(self: InputService, target: KeyName, isDown: boolean)
---@overload fun(self: InputService, target: integer, isDown: boolean)
---@param self InputService
---@param target KeyName | string | integer
---@param isDown boolean
function InputService:SimulateKey(target, isDown)
    local keyCode

    if type(target) == "number" then
        keyCode = target
    else
        local normalizedKey = target:lower()
        if normalizedKey == "mouse1" or normalizedKey == "mouse2" or normalizedKey == "mouse3" then
            error("SimulateKey does not simulate mouse buttons; use a mouse-button injection API", 2)
        end
        keyCode = KEY_MAP[normalizedKey]
    end

    if not keyCode then
        error("unsupported key target: " .. tostring(target), 2)
    end

    if native_ok and type(input_native) == "table" and type(input_native.simulate_key) == "function" then
        input_native.simulate_key(keyCode, isDown)
    end
end

--- Moves mouse cursor globally across screen coordinates.
---@param self InputService
---@param x number
---@param y number
---@param absolute? boolean
function InputService:SimulateMouseMove(x, y, absolute)
    if absolute == nil then
        absolute = true
    end

    if native_ok and type(input_native) == "table" and type(input_native.simulate_mouse_move) == "function" then
        input_native.simulate_mouse_move(x, y, absolute)
    end
end

--- Toggles Caps Lock, Num Lock, or Scroll Lock.
---@param self InputService
---@param lockKeyName "capslock" | "numlock" | "scrolllock"
function InputService:ToggleLockKey(lockKeyName)
    local keyCode = KEY_MAP[lockKeyName:lower()]
    assert(keyCode == 0x14 or keyCode == 0x90 or keyCode == 0x91, "lockKeyName must be capslock, numlock, or scrolllock")

    if native_ok and type(input_native) == "table" and type(input_native.toggle_lock_key) == "function" then
        input_native.toggle_lock_key(keyCode)
    end
end

---@param self InputService
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

---@param self InputService
function InputService:Update()
    -- Use this when this instance is the only InputService being updated.
    -- For several instances, call InputService.UpdateAll() once per frame instead.
    if native_ok and type(input_native) == "table" and type(input_native.update) == "function" then
        input_native.update()
    end

    self:_ProcessCallbacks()
end

function InputService.UpdateAll()
    -- Poll native state exactly once, then dispatch the same frame's edges to all instances.
    if native_ok and type(input_native) == "table" and type(input_native.update) == "function" then
        input_native.update()
    end

    for instance in pairs(active_instances) do
        instance:_ProcessCallbacks()
    end
end

return InputService -- #service
