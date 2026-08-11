local InstanceTyper = require("instance_type")
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

---@class __InstancerRequiredServices
---@field ImportedStorageService StorageService

---@return __InstancerRequiredServices?
local function LoadRequiredServices()
    if InstanceBuilder.CoreLib ~= nil and type(InstanceBuilder.CoreLib) == "table" then
        local FetchedServices <const> = {
            ImportedStorageService = InstanceBuilder.CoreLib:LoadService("StorageService"),
        }; return FetchedServices
    end
end

---@class InstanceBuilder
---@field new fun(): Instance
---@field ImportCoreLibrary fun(src: MaxCore)
---@field InstanceEventListeners table<EventConnection>
---@field CoreLib MaxCore?

---@class Instance
---@field Name string
---@field Parent Instance
---@field ParentChanged Event
---@field __max_type string
---@field GetName fun(selfObj: Instance): string
---@field GetParent fun(selfObj: Instance): Instance
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

xpcall(LoadRequiredServices, error)
return InstanceBuilder
