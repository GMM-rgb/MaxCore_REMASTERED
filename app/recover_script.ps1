# ==============================================================================
# SYSTEM RUNTIME RECOVERY SCRIPT (NON-ADMINISTRATOR SAFE + OVERLAY EXCLUSION)
# ==============================================================================

# 1. Force-restore default user environment shell pointers in the registry hive
$RegPath = "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Winlogon"
if (-not (Test-Path $RegPath)) { New-Item -Path $RegPath -Force | Out-Null }
Set-ItemProperty -Path $RegPath -Name "Shell" -Value "explorer.exe" -Force

# 2. Define native User32.dll window manager interfaces via C# compilation mapping
$Signature = @'
    using System;
    using System.Text;
    using System.Runtime.InteropServices;

    public static class Win32WindowRecovery {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        public static extern bool ReleaseCapture();

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    }
'@ 

# Compile the types into active PowerShell memory
Add-Type -TypeDefinition $Signature

# 3. Forcibly snap low-level mouse and keyboard input captures away from glitched application hooks
[Win32WindowRecovery]::ReleaseCapture() | Out-Null

# 4. Enumerate every active top-level user space window on the display topology
$Callback = {
    param([IntPtr]$hWnd, [IntPtr]$lParam)

    # Filter out invisible or hidden system thread workers to protect core OS assets
    if ([Win32WindowRecovery]::IsWindowVisible($hWnd)) {
        
        # Pull the internal Win32 class identifier to detect specific software layers
        $ClassNameBuffer = New-Object System.Text.StringBuilder(256)
        [Win32WindowRecovery]::GetClassName($hWnd, $ClassNameBuffer, 256) | Out-Null
        $CurrentClassName = $ClassNameBuffer.ToString()
        
        # SOLUTION EXCLUSION RULE: Skip NVIDIA Overlay widgets entirely so they never surface or harden
        if ($CurrentClassName -eq "CEF-OSC-WIDGET") {
            return $true # Instantly skip to the next window loop iteration
        }

        # --- APPLY WINDOW WINDOWING TEMPLATE DEFAULTS ---
        # WS_OVERLAPPEDWINDOW (0x00CF0000) reconstructs default caption bars, frames, and borders.
        [Win32WindowRecovery]::SetWindowLong($hWnd, -16, 0x00CF0000) | Out-Null

        # Strip extended exclusivity parameters (Removes WS_EX_TOPMOST rendering states)
        [Win32WindowRecovery]::SetWindowLong($hWnd, -20, 0x00000100) | Out-Null

        # Command the OS window station manager to render the container inside standard frames (SW_SHOWNORMAL = 1)
        [Win32WindowRecovery]::ShowWindow($hWnd, 1) | Out-Null

        # Push frame update notification boundaries (SWP_FRAMECHANGED | SWP_NOACTIVATE = 0x0027)
        # Demote z-axis hierarchy underneath baseline system layers using HWND_NOTOPMOST (-2)
        $HWND_NOTOPMOST = [IntPtr](-2)
        [Win32WindowRecovery]::SetWindowPos($hWnd, $HWND_NOTOPMOST, 100, 100, 1280, 720, 0x0027) | Out-Null
    }
    return $true
}

# Invoke the global desktop application loop iteration
[Win32WindowRecovery]::EnumWindows($Callback, [IntPtr]::Zero) | Out-Null

# 5. Flush existing dead or corrupted interface loops
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue

# 6. Execute fresh Windows User shell instance completely detached from the Lua application tree
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = "$env:windir\explorer.exe" } | Out-Null
