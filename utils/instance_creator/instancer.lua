local InstanceTyper = require("instance_type")
local InstanceBuilder = setmetatable({}, nil)
---@private
---@type MaxCore?
InstanceBuilder.CoreLib = nil
---@type table<EventConnection>
InstanceBuilder.InstanceEventListeners = {}

---@class Instance
---@field Parent Instance
---@field ParentChanged Event
---@field __max_type string
---@field GetParent fun(selfObj: Instance)
---@field SetParent fun(selfObj: Instance, TargetParent: Instance)

---@return Instance
function InstanceBuilder.new()
    local self = setmetatable(InstanceBuilder, InstanceBuilder)
    self.ParentChanged = self.CoreLib.Event.new("ParentChanged")
    InstanceTyper.SetType(self, "Instance"); return self
end

---@param self Instance
---@param TargetParent Instance
function InstanceBuilder:SetParent(TargetParent)
    if not self or self.__max_type == nil or not InstanceTyper.IsA(self, "Instance") then return end
    if self.ParentChanged == nil or not InstanceTyper.IsA(self.ParentChanged, "Event") then return end
    self.ParentChanged:Fire(TargetParent); if self.Parent == nil or self.Parent ~= TargetParent then
        self.Parent = TargetParent and TargetParent or nil
    end
end

---@param self Instance
function InstanceBuilder:GetParent()
    return self and self.Parent
end

---@param self Instance
function InstanceBuilder:SetInstanceInherent()

end

return InstanceBuilder
