local <const> math = require("math")
local <const> string = require("string")
local EngineCore = require("max_core").call()
local BaseInterface = setmetatable({}, nil)
BaseInterface.__index = BaseInterface
BaseInterface.Instance = {}

local function kpairs(t)
    return function(tbl, i)
        i = i + 2; local v = tbl[i]
        if v ~= nil then return i, v end
    end, t, -1
end

---@class BaseInterface
---@field ImportOutline fun(): nil
---@field _base_object GameObject?
---@field _border_object LineObject[]
---@field _service WindowService
---@field _win_id integer

---@param WindowService WindowService
---@param TargetWindow integer
function BaseInterface.Instance.new(WindowService, TargetWindow)
    local self = setmetatable({}, BaseInterface)
    self._border_object = {} --- @type LineObject[]
    self._base_object = nil --- @type GameObject?
    self._service = WindowService
    self._win_id = TargetWindow
    return self
end

---@param self BaseInterface 
---@param color number[] -- set the border color in RGB scale. 
---@param thickness number -- not used in processing currently.
function BaseInterface:ImportOutline(color, thickness)
    if self._base_object and type(self._base_object) ~= "nil" then
        local InstanceLine = EngineCore.Event.new("InstanceLine")
        local CurrentX, CurrentY = 0, 0 --- @type integer, integer
        local SelectedWindow = self._service:GetWindow(self._win_id)

        InstanceLine:Connect(function()
            
        end)

        for index, vertice in ipairs(self._base_object.Vertices) do
            
        end
    end
end

return BaseInterface
