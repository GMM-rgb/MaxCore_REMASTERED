local InstanceTyper = require("instance_type")
---@class InstanceBuilder : Instance
---@field new fun(TempDirectory: string): Instance
---@field ImportCoreLibrary fun(src: MaxCore)
---@field InstanceEventListeners table<EventConnection>
---@field CoreLib MaxCore?
local InstanceBuilder = setmetatable({}, nil)
---@type table<EventConnection>
InstanceBuilder.InstanceEventListeners = {}
---@type MaxCore?
InstanceBuilder.CoreLib = nil

--- ====================================================
---    Core Library Importer; Recursive Stuff Removed
--- ====================================================

---@param LibraryModule MaxCore
function InstanceBuilder.ImportCoreLibrary(LibraryModule)
    if LibraryModule and type(LibraryModule) == "table" then
        InstanceBuilder.CoreLib = LibraryModule
    end
end

---@class Instance
---@field Name string
---@field Parent Instance
---@field NameChanged Event
---@field ParentChanged Event
---@field GetName fun(selfObj: Instance): string
---@field GetParent fun(selfObj: Instance): Instance
---@field SetParent fun(selfObj: Instance, TargetParent: Instance)
---@field __max_type string

---@return Instance
function InstanceBuilder.new()
    local self = setmetatable(InstanceBuilder, InstanceBuilder)
    assert(not self.CoreLib, "CoreLib in instance builder wasn't loaded!")
    ---@type StorageService
    local StorageService = self.CoreLib:LoadService("StorageService")
    self.ParentChanged = self.CoreLib.Event.new("ParentChanged")
    self.NameChanged = self.CoreLib.Event.new("NameChanged")

    if self[InstanceTyper.TYPE_KEY] ~= "Instance" then
        InstanceTyper.SetType(self, "Instance")
    end

    ---@param SubjectParent Instance
    local ParentChangedConnection = self.ParentChanged:Connect(function(SubjectParent)
        
    end); table.insert(self.InstanceEventListeners, ParentChangedConnection); return self
end

---@param self Instance
---@param TargetParent Instance
function InstanceBuilder:SetParent(TargetParent)
    if not self or self.__max_type == nil or not InstanceTyper.IsA(self, "Instance") then return end
    if self.ParentChanged == nil or not InstanceTyper.IsA(self.ParentChanged, "Event") then return end

    if self.ParentChanged:IsConnected() then
        self.ParentChanged:Fire(TargetParent)
    end

    if self.Parent == nil or self.Parent ~= TargetParent then
        self.Parent = TargetParent and TargetParent or nil
    end
end

---@param self Instance
---@return Instance
function InstanceBuilder:GetParent() return self and self.Parent end
---@param self Instance
---@return string
function InstanceBuilder:GetName() return tostring(self.Name) or "Instance" end

---@generic InstanceCombiner(InstanceCombination)
---@alias InstanceCombination Instance

---@param self Instance
---@param SourceInherit InstanceCombination
function InstanceBuilder:SetInstanceInherent(SourceInherit)
    
end

return InstanceBuilder
