---@meta

---@generic T
---@param t T[]
---@param start_even? boolean
---@return fun(tbl: T[], i: integer): integer, T iterator
---@return T[] state
---@return integer initial_index
function kpairs(t, start_even) end

---@type MaxCore
_G.EngineCore = dofile("max_core.lua")

---Options accepted by `os.native_execute`.
---@class NativeExecuteOptions
---@field wait? boolean Wait for completion. Defaults to `true`.
---@field cwd? string Working directory for the new process. Supported on Windows and on macOS/glibc-based POSIX builds that provide the required spawn working-directory API.
---@field inherit_stdio? boolean Inherit the caller's standard input/output/error handles. Defaults to `false` on Windows and `true` on POSIX.
---@field detached? boolean Start in a separate process group. On Windows this also detaches the process from the parent's console; on POSIX it creates a new process group, not a new login session.
---@field return_pid? boolean With `wait = false`, return the new process ID instead of `true`.
---@field window? "hidden"|"inherit"|"normal"|"minimized"|"maximized"|"new_console" Windows only. Startup window/console behavior. Defaults to `"hidden"`.
---@field priority? "idle"|"below_normal"|"normal"|"above_normal"|"high" Windows only. Requested process priority class; defaults to `"normal"`.

---Launch a native process directly, without invoking a command shell.
---
---The command is a command-line string. Quote paths and arguments containing spaces.
---On POSIX, the command is tokenized into argv; shell expansion, pipes, redirection,
---globbing, and environment-variable expansion are not performed.
---
---Legacy form: `os.native_execute(command, false)` launches asynchronously.
---@param command string Executable and its arguments as a command-line string.
---@param options? NativeExecuteOptions|boolean Options table, or a legacy boolean controlling `wait`.
---@return integer|boolean|nil result Exit code when waiting; `true` when launched asynchronously; or PID when `return_pid = true`.
---@return integer? os_error On failure, the numeric operating-system error code.
function os.native_execute(command, options) end

---Execute a command using the legacy system command interface.
---Deprecated: use `os.native_execute()` instead for configurable native process execution.
---@deprecated Use os.native_execute() instead.
---@param command? string Command to execute using the system command interface.
---@return boolean|integer|nil success Exit status or success indicator, depending on the platform and Lua implementation.
function os.execute(command) end

---@type string
_G.VERSION = string.char(0)
