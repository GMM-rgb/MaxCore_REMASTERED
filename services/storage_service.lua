-- #service
local InstanceTyping = require("instance_type")
--- =============================================
---           Storage Service Type Stubs
--- =============================================

---@class StorageService
---@field new fun(): StorageService
---@field GetFile fun(selfObj: StorageService)
---@field _CachedFiles StorageCache

---@class FileObject
---@field _name string
---@field contents table

---@class StorageCache
---@field _cache table<FileObject>

---@type StorageService
local StorageService = setmetatable({}, nil)

---@return StorageService
function StorageService.new()
    ---@type StorageService
    local self = setmetatable(StorageService, {})
    InstanceTyping.SetType(self, "StorageService")
    ---@type StorageCache
    self._CachedFiles = {
        _cache = {},
    }; return self
end

function StorageService:GetFile()

end

return StorageService
