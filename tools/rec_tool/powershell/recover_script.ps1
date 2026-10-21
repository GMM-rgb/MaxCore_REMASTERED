# ==============================================================================
# LEAK-FREE RECOVERY SCRIPT (NON-ADMINISTRATOR SAFE + RELIABLE INVOCATION)
# ==============================================================================

# 1. Force-restore default user environment shell pointers in the registry hive
$RegPath = "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Winlogon"
if (-not (Test-Path $RegPath)) { New-Item -Path $RegPath -Force | Out-Null }
Set-ItemProperty -Path $RegPath -Name "Shell" -Value "explorer.exe" -Force

# 2. Map Win32 API signatures inside a structured C# string block
$Signature = @'
    using System;
    using System.Text;
    using System.Runtime.InteropServices;

    public static class SafeWin32 {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
        [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);
        [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] public static extern bool ReleaseCapture();
        [DllImport("user32.dll", CharSet = CharSet.Auto)] public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    }
'@

# 3. Only compile once per shell context window session to permanently fix the memory leak
if (-not ([Ref].Assembly.GetType("SafeWin32"))) {
    Add-Type -TypeDefinition $Signature -ErrorAction SilentlyContinue
}

# 4. Break mouse and keyboard hooks
[SafeWin32]::ReleaseCapture() | Out-Null

# 5. Enumerate active windows using strict system core filtering exclusions
$Callback = {
    param([IntPtr]$hWnd, [IntPtr]$lParam)

    if ([SafeWin32]::IsWindowVisible($hWnd)) {
        
        # Pull internal window class string to isolate background assets
        $Buf = New-Object System.Text.StringBuilder(256)
        [SafeWin32]::GetClassName($hWnd, $Buf, 256) | Out-Null
        $Class = $Buf.ToString()
        
        # SYSTEM EXCLUSION FILTER: Clean ASCII class tracking variables
        $Exclusions = @(
            "CEF-OSC-WIDGET",          # NVIDIA Overlay
            "Shell_TrayWnd",           # Windows Taskbar
            "Progman",                 # Desktop Wallpaper Background Manager
            "WorkerW",                 # DWM Display Sub-layers
            "ApplicationFrameWindow",  # Universal App Frame Wrappers
            "Cortana"                  # System Search Window Container
        )
        
        if ($Exclusions -contains $Class -or $Class -like "Windows.UI.*") {
            return $true # Instantly skip system assets safely
        }

        # --- APPLY WINDOW DEFAULTS TO USER APPS ONLY ---
        [SafeWin32]::SetWindowLong($hWnd, -16, 0x00CF0000) | Out-Null
        [SafeWin32]::SetWindowLong($hWnd, -20, 0x00000100) | Out-Null
        [SafeWin32]::ShowWindow($hWnd, 1) | Out-Null
        
        $HWND_NOTOPMOST = [IntPtr](-2)
        [SafeWin32]::SetWindowPos($hWnd, $HWND_NOTOPMOST, 100, 100, 1280, 720, 0x0027) | Out-Null
    }
    return $true
}

# Run the clean non-leaking loop
[SafeWin32]::EnumWindows($Callback, [IntPtr]::Zero) | Out-Null

# 6. Reset Shell Core
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue

# STUCK INITIALIZATION FIX: Hand execution off to Userinit
# This forces Windows to execute the shell initialization protocol natively outside of WMI bounds
# Start-Process -FilePath "$env:windir\system32\userinit.exe"
