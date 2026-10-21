# ==============================================================================
# MaxCore - Desktop Window Recovery
# User-level execution | Per-window error handling | Win32 via Add-Type
# ==============================================================================

$ErrorActionPreference = 'Continue'

# 1. Compile Win32 declarations once in this PowerShell process.
#    This does not persist across separate powershell.exe invocations.

if (-not ('SafeWin32' -as [type])) {
    $Signature = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class SafeWin32
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool EnumWindows(
        EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr GetWindowLongPtrW(
        IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongW",
        SetLastError = true)]
    public static extern int GetWindowLong32(
        IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SetWindowLongPtrW(
        IntPtr hWnd, int nIndex, IntPtr value);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongW",
        SetLastError = true)]
    public static extern int SetWindowLong32(
        IntPtr hWnd, int nIndex, int value);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool GetWindowRect(
        IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(
        IntPtr hWnd, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(
        IntPtr hWnd, int command);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(
        IntPtr hWnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool ReleaseCapture();

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(
        IntPtr hWnd, out uint processId);
}
'@

    Add-Type -TypeDefinition $Signature -ErrorAction Stop
}

# 2. Win32 constants.
$GWL_STYLE       = -16
$GWL_EXSTYLE     = -20

$WS_CAPTION      = 0x00C00000
$WS_THICKFRAME   = 0x00040000
$WS_MINIMIZEBOX  = 0x00020000
$WS_MAXIMIZEBOX  = 0x00010000
$WS_SYSMENU      = 0x00080000
$WS_POPUP        = [long]0x80000000

$WS_EX_TOPMOST   = 0x00000008

$SW_RESTORE      = 9

$SWP_NOZORDER    = 0x0004
$SWP_FRAMECHANGED = 0x0020
$SWP_NOOWNERZORDER = 0x0200

$TargetX = 100
$TargetY = 100
$TargetWidth = 1280
$TargetHeight = 720

$Exclusions = @(
    'CEF-OSC-WIDGET',
    'Shell_TrayWnd',
    'Progman',
    'WorkerW',
    'ApplicationFrameWindow',
    'Cortana'
)

# 3. Architecture-safe window-style wrappers.

function Get-WindowStyle {
    param(
        [IntPtr]$Handle,
        [int]$Index
    )

    if ([IntPtr]::Size -eq 8) {
        return [SafeWin32]::GetWindowLongPtrW($Handle, $Index).ToInt64()
    }

    return [long][SafeWin32]::GetWindowLong32($Handle, $Index)
}

function Set-WindowStyle {
    param(
        [IntPtr]$Handle,
        [int]$Index,
        [long]$Style
    )

    if ([IntPtr]::Size -eq 8) {
        $Value = [IntPtr]::new($Style)
        [void][SafeWin32]::SetWindowLongPtrW($Handle, $Index, $Value)
    }
    else {
        [void][SafeWin32]::SetWindowLong32(
            $Handle, $Index, [int]$Style)
    }
}

# 4. Release the current mouse capture if present.
#    This does not disable system-wide input hooks.

[void][SafeWin32]::ReleaseCapture()

# 5. Track results without stopping on individual window failures.

$script:Processed = 0
$script:Skipped = 0
$script:Failed = 0

$Callback = [SafeWin32+EnumWindowsProc]{
    param(
        [IntPtr]$hWnd,
        [IntPtr]$lParam
    )

    try {
        if (-not [SafeWin32]::IsWindow($hWnd)) {
            $script:Skipped++
            return $true
        }

        if (-not [SafeWin32]::IsWindowVisible($hWnd)) {
            $script:Skipped++
            return $true
        }

        $Buffer = [System.Text.StringBuilder]::new(256)
        [void][SafeWin32]::GetClassNameW($hWnd, $Buffer, 256)
        $Class = $Buffer.ToString()

        if (
            $Exclusions -contains $Class -or
            $Class -like 'Windows.UI.*'
        ) {
            $script:Skipped++
            return $true
        }

        # Avoid rewriting window styles belonging to another process
        # when the window is already in a protected or unsupported state.
        # Cross-process window operations can fail due to permissions.

        $Style = Get-WindowStyle -Handle $hWnd -Index $GWL_STYLE

        # Convert popup-style windows into standard overlapped windows.
        # Preserve unrelated style bits.
        $Style = $Style -band (-bnot $WS_POPUP)
        $Style = $Style -bor (
            $WS_CAPTION -bor
            $WS_THICKFRAME -bor
            $WS_SYSMENU -bor
            $WS_MINIMIZEBOX -bor
            $WS_MAXIMIZEBOX
        )

        Set-WindowStyle -Handle $hWnd -Index $GWL_STYLE -Style $Style

        # Remove topmost status without changing unrelated extended styles.
        $ExStyle = Get-WindowStyle -Handle $hWnd -Index $GWL_EXSTYLE
        $ExStyle = $ExStyle -band (-bnot $WS_EX_TOPMOST)

        Set-WindowStyle -Handle $hWnd -Index $GWL_EXSTYLE -Style $ExStyle

        # Restore minimized windows before applying the requested geometry.
        [void][SafeWin32]::ShowWindow($hWnd, $SW_RESTORE)

        # SWP_NOZORDER preserves the existing Z-order.
        $Flags = $SWP_NOZORDER -bor $SWP_NOOWNERZORDER -bor $SWP_FRAMECHANGED

        $Success = [SafeWin32]::SetWindowPos(
            $hWnd,
            [IntPtr]::Zero,
            $TargetX,
            $TargetY,
            $TargetWidth,
            $TargetHeight,
            $Flags
        )

        if (-not $Success) {
            $script:Failed++
            return $true
        }

        $script:Processed++
    }
    catch {
        $script:Failed++
    }

    return $true
}

try {
    [void][SafeWin32]::EnumWindows($Callback, [IntPtr]::Zero)
}
finally {
    $Callback = $null
}

Write-Output (
    'Recovery finished. Processed={0}; Skipped={1}; Failed={2}' -f
    $script:Processed, $script:Skipped, $script:Failed
)
