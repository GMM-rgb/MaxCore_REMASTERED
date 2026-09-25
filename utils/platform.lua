-- #service
package.cpath = package.cpath
    .. ";./build/?.dylib"
    .. ";./build/Release/?.dylib"
    .. ";./build/Debug/?.dylib"
    .. ";./build/bin/?.dylib"
    .. ";./build/?.dll"
    .. ";./build/Release/?.dll"
    .. ";./build/Debug/?.dll"
    .. ";./build/bin/?.dll"
    .. ";./build/?.so"
    .. ";./build/Release/?.so"
    .. ";./build/Debug/?.so"
    .. ";./build/bin/?.so"

---@alias PlatformMemoryInfo { ["total_mb"]: integer, ["available_mb"]: integer }

---@class PlatformInfo
---@field os string
---@field arch string
---@field cpu_cores integer
---@field ram_total_mb integer
---@field ram_available_mb integer
---@field is_64bit boolean
---@field endianness "little" | "big"

---@class NativePlatformInterface
---@field get_os fun(): string
---@field get_arch fun(): string
---@field get_cpu_cores fun(): integer
---@field get_memory_info fun(): PlatformMemoryInfo
---@field get_info fun(): PlatformInfo

---@type NativePlatformInterface
local native_platform = require("platform_module")

local PlatformService = {}
PlatformService.__index = PlatformService

function PlatformService.new()
    local self = setmetatable({}, PlatformService)
    return self
end

function PlatformService:get_os()
    return native_platform.get_os()
end

function PlatformService:get_arch()
    return native_platform.get_arch()
end

function PlatformService:get_cpu_cores()
    return native_platform.get_cpu_cores()
end

function PlatformService:get_memory_info()
    return native_platform.get_memory_info()
end

function PlatformService:get_info()
    return native_platform.get_info()
end

return PlatformService
