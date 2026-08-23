-- #service
local InstanceTyping = require("instance_type")

package.cpath = package.cpath 
    .. ";./build/?.dylib" 
    .. ";./build/Release/?.dylib" 
    .. ";./build/Debug/?.dylib" 
    .. ";./build/bin/?.dylib" 
    .. ";./build/bin/Release/?.dylib"
    .. ";./build/?/?.dylib"
    .. ";./build/?.so"
    .. ";./build/?.dll"

local native_ok, storage_interface = pcall(require, "storage_interface")

if not native_ok then
    print("\27[33m[StorageService Warning]\27[0m Failed to load storage_interface: " .. tostring(storage_interface))
end

-- =========================================================================
-- TYPE DEFINITIONS & SCHEMA STRUCTS
-- =========================================================================

---@class StorageCache
---@field _cache table<string, FileObject>

---@class StorageService
---@field _CachedFiles StorageCache
---@field _debug boolean
local StorageService = {}
StorageService.__index = StorageService
InstanceTyping.SetType(StorageService, "StorageService")

---@class CreateFileOptions
---@field path? string
---@field name? string
---@field extension? string
---@field directory? string
---@field contents? string
---@field debug? boolean

---@class FileObject
---@field _path string
---@field _name string
---@field _extension string
---@field _size number
---@field _modifiedTime number
---@field _isDirectory boolean
---@field _contents string|nil
---@field _service StorageService | nil
local FileObject = {}
FileObject.__index = FileObject
InstanceTyping.SetType(FileObject, "FileObject")

-- =========================================================================
-- PATH RESOLUTION
-- =========================================================================

local cached_base_dir = nil

---Gets the absolute base directory of the executing script
---@return string
local function get_base_dir()
    if cached_base_dir then
        return cached_base_dir
    end

    local scriptPath = "."
    if arg and arg[0] then
        local cleanArg = arg[0]:gsub("\\", "/")
        local dir = cleanArg:match("^(.*[/\\])")
        if dir then
            scriptPath = dir:gsub("[/\\]$", "")
        end
    end

    -- Convert scriptPath into an absolute path via C++ native layer
    if native_ok and type(storage_interface.get_file_info) == "function" then
        local info = storage_interface.get_file_info(scriptPath, false)
        if info and info.path and info.path ~= "" then
            local absPath = info.path:gsub("\\", "/")
            if not info.is_directory then
                absPath = absPath:match("^(.*/)") or absPath
                absPath = absPath:gsub("/$", "")
            end
            cached_base_dir = absPath
            return cached_base_dir
        end
    end

    cached_base_dir = scriptPath
    return cached_base_dir
end

---Resolves a given path to an absolute or script-relative path
---@param filePath string|nil
---@return string
local function resolve_path(filePath)
    local baseDir = get_base_dir()

    if not filePath or filePath == "" or filePath == "." then
        return baseDir
    end

    filePath = filePath:gsub("\\", "/")

    -- Check if path is absolute (Windows drive letter C:/ or Unix /)
    if filePath:match("^%a:/") or filePath:match("^/") then
        return filePath
    end

    local cleanBase = baseDir:gsub("/$", "")

    -- Prevent path duplication if filePath already starts with baseDir
    if filePath == cleanBase then
        return filePath
    end

    if filePath:sub(1, #cleanBase + 1) == (cleanBase .. "/") then
        return filePath
    end

    filePath = filePath:gsub("^%./", "")
    return cleanBase .. "/" .. filePath
end

-- =========================================================================
-- FILE OBJECT CLASS IMPLEMENTATION
-- =========================================================================

---Creates a new FileObject instance
---@param filePath string
---@param serviceOwner StorageService|nil
---@return FileObject
function FileObject.new(filePath, serviceOwner)
    local self = setmetatable({}, FileObject)
    self._path = resolve_path(filePath)
    self._service = serviceOwner
    self._name = ""
    self._extension = ""
    self._size = 0
    self._modifiedTime = 0
    self._isDirectory = false
    self._contents = nil

    self:FetchMetaData(false)
    return self
end

---Fetches and updates internal metadata for the file
---@param includeContents? boolean
function FileObject:FetchMetaData(includeContents)
    if native_ok and type(storage_interface.get_file_info) == "function" then
        local info = storage_interface.get_file_info(self._path, includeContents or false)
        if info then
            self._name = info.name or ""
            self._extension = info.extension or ""
            self._size = info.size or 0
            self._modifiedTime = info.modified_time or 0
            self._isDirectory = info.is_directory or false
            if includeContents then
                self._contents = info.contents or ""
            end
        end
    end
end

---Reads the file contents from disk
---@return string|nil contents
---@return string|nil err
function FileObject:Read()
    if self._isDirectory then
        return nil, "Cannot read contents of a directory."
    end
    if native_ok and type(storage_interface.read_file) == "function" then
        local content, err = storage_interface.read_file(self._path)
        if content then
            self._contents = content
            self:FetchMetaData(false)
            return content, nil
        end
        return nil, err
    end
    return nil, "Native storage module is unavailable."
end

---Writes content to the file on disk
---@param content string
---@return boolean success
---@return string|nil err
function FileObject:Write(content)
    if native_ok and type(storage_interface.write_file) == "function" then
        local ok, err = storage_interface.write_file(self._path, content)
        if ok then
            self._contents = content
            self:FetchMetaData(false)
            return true, nil
        end
        return false, err
    end
    return false, "Native storage module is unavailable."
end

---Deletes the file or directory from disk
---@return boolean success
---@return string|nil err
function FileObject:Delete()
    if native_ok then
        local ok, err
        if self._isDirectory and type(storage_interface.remove_dir) == "function" then
            ok, err = storage_interface.remove_dir(self._path)
        elseif type(storage_interface.remove_file) == "function" then
            ok, err = storage_interface.remove_file(self._path)
        end

        if ok and self._service then
            self._service._CachedFiles._cache[self._path] = nil
        end
        return ok or false, err
    end
    return false, "Native storage module is unavailable."
end

---@return string
function FileObject:GetPath() return self._path end

---@return string
function FileObject:GetName() return self._name end

---@return string
function FileObject:GetExtension() return self._extension end

---@return number
function FileObject:GetSize() return self._size end

---@return number
function FileObject:GetModifiedTime() return self._modifiedTime end

---@return boolean
function FileObject:IsDirectory() return self._isDirectory end

---@return string|nil
function FileObject:GetContents() return self._contents or self:Read() end

-- =========================================================================
-- MAIN SERVICE CLASS IMPLEMENTATION
-- =========================================================================

---Creates a new StorageService instance
---@return StorageService
function StorageService.new()
    local self = setmetatable({}, StorageService)
    self._CachedFiles = {
        _cache = {}
    }
    self._debug = false
    return self
end

---Enables or disables global debug logging for the service
---@param enabled boolean
function StorageService:SetDebug(enabled)
    self._debug = enabled == true
end

---Gets the absolute base directory of the executing script
---@return string
function StorageService:GetBaseDirectory()
    return get_base_dir()
end

---Gets or caches a FileObject from the given path
---@param filePath string
---@return FileObject
function StorageService:GetFile(filePath)
    local resolvedPath = resolve_path(filePath)
    if not self._CachedFiles._cache[resolvedPath] then
        self._CachedFiles._cache[resolvedPath] = FileObject.new(resolvedPath, self)
    end
    return self._CachedFiles._cache[resolvedPath]
end

---Creates a file using explicit schema keys from the options table
---@param options CreateFileOptions|string
---@return FileObject|nil file
---@return string|nil err
function StorageService:CreateFile(options)
    if type(options) == "string" then
        options = { path = options }
    end
    
    options = type(options) == "table" and options or {}
    local targetPath = options.path

    if not targetPath or targetPath == "" then
        local dir = options.directory and resolve_path(options.directory) or get_base_dir()
        local name = options.name or "untitled"
        local ext = options.extension or ""
        
        if ext ~= "" and not ext:find("^%.") then
            ext = "." .. ext
        end

        targetPath = dir .. "/" .. name .. ext
    else
        targetPath = resolve_path(targetPath)
    end

    local isDebug = (options.debug == true) or (self._debug == true)
    if isDebug then
        print(string.format("\27[36m[StorageService Debug]\27[0m Creating file at resolved path: '%s'", targetPath))
    end

    local fileObj = self:GetFile(targetPath)
    local initialContents = options.contents or ""
    local ok, err = fileObj:Write(initialContents)

    if ok then
        return fileObj, nil
    end
    return nil, err
end

---Deletes a file at the given path
---@param filePath string
---@return boolean success
---@return string|nil err
function StorageService:DeleteFile(filePath)
    local fileObj = self:GetFile(filePath)
    return fileObj:Delete()
end

---Deletes a directory at the given path
---@param dirPath string
---@return boolean success
---@return string|nil err
function StorageService:DeleteDirectory(dirPath)
    local resolvedDir = resolve_path(dirPath)
    if native_ok and type(storage_interface.remove_dir) == "function" then
        return storage_interface.remove_dir(resolvedDir)
    end
    return false, "Native storage module is unavailable."
end

---Creates a directory at the given path
---@param dirPath string
---@return boolean success
---@return string|nil err
function StorageService:CreateDirectory(dirPath)
    local resolvedDir = resolve_path(dirPath)
    if native_ok and type(storage_interface.create_dir) == "function" then
        return storage_interface.create_dir(resolvedDir)
    end
    return false, "Native storage module is unavailable."
end

---Lists the contents of a directory and returns an array of FileObjects
---@param dirPath? string
---@return FileObject[]
function StorageService:ListDirectory(dirPath)
    local targetDir = resolve_path(dirPath or ".")
    local items = {}
    if native_ok and type(storage_interface.list_dir) == "function" then
        local rawList = storage_interface.list_dir(targetDir)
        for _, info in ipairs(rawList) do
            local fileObj = self:GetFile(info.path)
            fileObj._name = info.name or ""
            fileObj._extension = info.extension or ""
            fileObj._size = info.size or 0
            fileObj._modifiedTime = info.modified_time or 0
            fileObj._isDirectory = info.is_directory or false
            table.insert(items, fileObj)
        end
    end
    return items
end

---@param SourceName string
---@return string, string
function StorageService.SplitFileName(SourceName)
    if not SourceName or type(SourceName) ~= "string" then
        return "<unknown>", "<unknown>"
    end
    return SourceName:match("^(.+)%.([^.]+)$")
end

return StorageService
