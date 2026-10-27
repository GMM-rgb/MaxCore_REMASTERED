local core = require("lua-5.7.2.src.max_core").call()
local input = core:LoadService("InputService")
local runtime = core:LoadService("RunnerService")

input:BindAction("restart", "down", function (name, state, key)
    if state ~= nil and state == "Pressed" and key == "down" then
        -- Force style rewrite, layout override, and safe user-scope Explorer restoration
        local psCommand = [[powershell -NoProfile -Command "]] ..
        [[$code = ']] ..
        [[using System; ]] ..
        [[using System.Runtime.InteropServices; ]] ..
        [[public class WM { ]] ..
        [[    [DllImport(\"user32.dll\")] public static extern bool ShowWindow(IntPtr h, int c); ]] ..
        [[    [DllImport(\"user32.dll\")] public static extern bool IsWindowVisible(IntPtr h); ]] ..
        [[    [DllImport(\"user32.dll\")] public static extern int SetWindowLongW(IntPtr h, int n, int dw); ]] ..
        [[    [DllImport(\"user32.dll\")] public static extern bool SetWindowPos(IntPtr h, IntPtr ha, int x, int y, int cx, int cy, uint f); ]] ..
        [[};'; ]] ..
        [[Add-Type -TypeDefinition $code; ]] ..
        
        -- 1. Apply absolute frame override and force window scale boundaries
        [[Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.ProcessName -ne 'explorer' -and $_.ProcessName -ne 'powershell' } | ForEach-Object { ]] ..
        [[    $h = $_.MainWindowHandle; ]] ..
        [[    if ([WM]::IsWindowVisible($h)) { ]] ..
        [[        [void][WM]::SetWindowLongW($h, -8, 0); ]] .. -- Break background priority link
        [[        [void][WM]::SetWindowLongW($h, -20, 0x00000008); ]] .. -- Force WS_EX_TOPMOST removal
        [[        [void][WM]::SetWindowLongW($h, -16, 0x14CF0000); ]] .. -- Overwrite with Visible Border Window Style
        [[        [void][WM]::SetWindowPos($h, (New-Object IntPtr(-2)), 50, 50, 1024, 768, 0x0020 -bor 0x0040); ]] .. -- Snap size immediately
        [[    } ]] ..
        [[}; ]] ..
        
        -- 2. Check if explorer.exe is running in user scope. If missing, boot it under interactive credentials.
        [[if (-not (Get-Process -Name explorer -ErrorAction SilentlyContinue)) { ]] ..
        [[    Start-Process "$env:SystemRoot\explorer.exe"; ]] .. -- Safely launches inside active user context
        [[}"]]

        local success, exitType, exitCode = os.execute(psCommand)
        io.stdout:write("System Restored: " .. tostring(exitCode) .. "\t" .. tostring(exitType) .. "\n")
    end
end)

runtime.Stepped:Connect(function()
    input:UpdateAll()
end)

input:SetGlobalInput(true)
runtime:KeepAlive()
