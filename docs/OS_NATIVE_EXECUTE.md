# `os.native_execute` API

`os.native_execute` starts an executable directly. It does not send the command through `cmd.exe`, PowerShell, `/bin/sh`, or another shell. This means shell features such as `|`, `>`, `&&`, wildcard expansion, and `$VARIABLE`/`%VARIABLE%` expansion are not interpreted. Pass an actual executable and its arguments, quoting paths and arguments with spaces.

## Signature

```lua
os.native_execute(command [, options])
```

The old second-argument boolean remains supported: `os.native_execute(command, false)` means `wait = false`.

| Option | Type | Default | Purpose |
|---|---|---|---|
| `wait` | boolean | `true` | Wait for process exit and return its exit code. `false` launches asynchronously. |
| `cwd` | string | inherited working directory | Set the child's working directory. Windows supports this. POSIX support requires a macOS or glibc build with `posix_spawn_file_actions_addchdir_np`. |
| `inherit_stdio` | boolean | Windows: `false`; POSIX: `true` | Inherit stdin/stdout/stderr. If false on POSIX, all three are redirected to `/dev/null`. On Windows, inherited standard handles are duplicated as inheritable handles. This is handle inheritance, not a Lua-managed pipe API. |
| `detached` | boolean | `false` | Windows: detach from the parent console and use a new process group. POSIX: make a new process group; this does not create a fully detached session. |
| `return_pid` | boolean | `false` | With `wait = false`, return the process ID instead of `true`. It is an identifier, not a managed process handle. |
| `window` | string (Windows only) | `"hidden"` | `"hidden"`, `"inherit"`, `"normal"`, `"minimized"`, `"maximized"`, or `"new_console"`. A GUI application may still control its own window after startup. |
| `priority` | string (Windows only) | `"normal"` | Request `"idle"`, `"below_normal"`, `"normal"`, `"above_normal"`, or `"high"` priority class. This is only a scheduling hint, not a performance guarantee. |

## Examples

Launch a hidden process and keep the Lua runtime moving without waiting:

```lua
local ok, err = os.native_execute(
    'powershell.exe -NoProfile -File "C:\\tools\\worker.ps1"',
    { wait = false }
)
if not ok then
    io.stderr:write("Launch failed: ", tostring(err), "\\n")
end
```

Inherit standard handles so a console child can use the caller's standard streams:

```lua
os.native_execute('python.exe "C:\\tools\\worker.py"', {
    wait = true,
    inherit_stdio = true,
    window = "inherit",
    cwd = "C:\\tools"
})
```

Start an asynchronous process and receive its OS process ID:

```lua
local pid, err = os.native_execute('powershell.exe -NoProfile -Command "Start-Sleep -Seconds 10"', {
    wait = false,
    return_pid = true,
    detached = true
})
```

`detached = true` and `wait = false` affect process launch/lifetime semantics; they do not install a permanent supervisor. The existing runtime keep-alive code remains responsible for keeping the Lua host alive, and a PID alone does not provide wait/terminate/pipe operations.

## Return values

- With `wait = true`: `exit_code` on completion.
- With `wait = false`: `true` after a successful launch, or the numeric PID when `return_pid = true`.
- On launch/wait failure: `nil, os_error_code`.

The implementation was source-reviewed but not compiled against your exact Lua/toolchain setup here; check your platform SDK and compiler settings before replacing the copy used by your runtime.
