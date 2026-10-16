-- =========================================================================
-- services/native_paths.lua
-- =========================================================================
-- Makes Lua's package.cpath look for native modules (window_management,
-- sleep_native, scheduler_native) with the right file extension on every OS.
--
-- Why this exists: Lua only finds a native module if the file name matches a
-- pattern in package.cpath. The default patterns end in ".so" (Linux and,
-- usually, macOS) or ".dll" (Windows). A CMake/clang build on macOS that
-- produces "sleep_native.dylib" is never matched, require() fails, and code
-- that wraps it in pcall silently falls back as if the module didn't exist.
--
-- Safe to call any number of times from any file: a pattern is only added if
-- it is not already present, so cpath never grows duplicate entries.

---@class NativePaths
local NativePaths = {}

-- Folders searched for native modules, relative to the working directory.
-- Covers a plain build/, the multi-config folders IDE generators create, and
-- the project root.
local FOLDERS = {
    "./build",
    "./build/Release",
    "./build/Debug",
    "./build/RelWithDebInfo",
    "./build/bin",
    "./bin",
    ".",
}

-- Every extension a native Lua module can have. All are listed on every OS on
-- purpose: a .so built on macOS, a .dylib built on Linux cross-tooling, or a
-- .dll next to either are all found without anyone editing this file.
local EXTENSIONS = { "dylib", "so", "dll" }

---@alias NativePlatform '"windows"'|'"macos"'|'"linux"'

---Used only to decide search ORDER (the native extension first), never to
---exclude anything. Detected once, then cached.
---@type NativePlatform?
local cachedPlatform = nil

---@return NativePlatform
function NativePaths.Platform()
    if cachedPlatform then return cachedPlatform end
    cachedPlatform = NativePaths.DetectPlatform()
    return cachedPlatform
end

---Does the actual detection. Prefer Platform(), which caches this.
---@return NativePlatform
function NativePaths.DetectPlatform()
    local sep = package.config:sub(1, 1)
    if sep == "\\" then return "windows" end
    -- jit.os exists on LuaJIT; on plain Lua fall back to asking the shell once.
    ---@type any
    local jitGlobal = rawget(_G, "jit")
    if type(jitGlobal) == "table" and type(jitGlobal.os) == "string" then
        return jitGlobal.os == "OSX" and "macos" or "linux"
    end
    local ok, handle = pcall(io.popen, "uname -s 2>/dev/null")
    if ok and handle then
        local name = handle:read("*l") or ""
        handle:close()
        if name:lower():find("darwin", 1, true) then return "macos" end
    end
    return "linux"
end

---@param list string
---@param entry string
---@return boolean
local function contains(list, entry)
    -- Plain-text search with ';' boundaries so "a/?.so" doesn't match inside "xa/?.so".
    return (";" .. list .. ";"):find(";" .. entry .. ";", 1, true) ~= nil
end

local applied = false

---Adds the native-module search patterns to package.cpath. Idempotent.
---@param extraFolders string[]? Additional folders to search, e.g. your own build output directory
---@return string cpath The resulting package.cpath
function NativePaths.Apply(extraFolders)
    local platform = NativePaths.Platform()

    -- Native extension first so the right file wins when several exist.
    local order = { "so", "dylib", "dll" }
    if platform == "macos" then order = { "dylib", "so", "dll" }
    elseif platform == "windows" then order = { "dll", "so", "dylib" } end

    local folders = {}
    for _, f in ipairs(extraFolders or {}) do folders[#folders + 1] = f end
    for _, f in ipairs(FOLDERS) do folders[#folders + 1] = f end

    local cpath = package.cpath
    for _, folder in ipairs(folders) do
        for _, ext in ipairs(order) do
            local entry = ("%s/?.%s"):format(folder, ext)
            if not contains(cpath, entry) then
                cpath = cpath .. ";" .. entry
            end
        end
    end
    package.cpath = cpath
    applied = true
    return cpath
end

---True once Apply has run at least once in this process.
---@return boolean
function NativePaths.IsApplied()
    return applied
end

---Attempts require(name) and returns the module, or nil plus a message that
---lists every path that was tried. Use this instead of a bare pcall(require)
---so a missing library produces a useful error rather than silence.
---@param name string
---@return any? module
---@return string? err
function NativePaths.TryRequire(name)
    if not applied then NativePaths.Apply() end
    local ok, result = pcall(require, name)
    if ok then return result, nil end
    return nil, tostring(result)
end

return NativePaths
