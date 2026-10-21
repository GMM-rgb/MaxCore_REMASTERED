
# ==============================================================================
# MaxCore - Desktop Window Recovery
# Non-elevated | Window relationship aware | Per-window error handling
# ==============================================================================

$ErrorActionPreference = 'Continue'

# 1. Win32 declarations.
#    Add-Type compiles these once per PowerShell process.

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
    public static extern bool IsWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr GetParent(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindow(IntPtr hWnd, uint command);

    [DllImport("user32.dll")]
    public static extern bool IsChild(
        IntPtr parent, IntPtr child);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW",
        SetLastError = true)]
    public static extern IntPtr GetWindowLongPtr64(
        IntPtr hWnd, int index);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongW",
        SetLastError = true)]
    public static extern int GetWindowLong32(
        IntPtr hWnd, int index);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW",
        SetLastError = true)]
    public static extern IntPtr SetWindowLongPtr64(
        IntPtr hWnd, int index, IntPtr value);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongW",
        SetLastError = true)]
    public static extern int SetWindowLong32(
        IntPtr hWnd, int index, int value);

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
}
'@

    Add-Type -TypeDefinition $Signature -ErrorAction Stop
}

# 2. Constants.

$GWL_STYLE          = -16
$GWL_EXSTYLE        = -20

$WS_CHILD           = 0x40000000
$WS_POPUP           = [long]0x80000000
$WS_CAPTION         = 0x00C00000
$WS_THICKFRAME      = 0x00040000
$WS_MINIMIZEBOX     = 0x00020000
$WS_MAXIMIZEBOX     = 0x00010000
$WS_SYSMENU         = 0x00080000
$WS_EX_TOPMOST      = 0x00000008

$GW_OWNER           = 4
$SW_RESTORE         = 9

$SWP_NOZORDER       = 0x0004
$SWP_FRAMECHANGED   = 0x0020
$SWP_NOOWNERZORDER  = 0x0200

$TargetX            = 100
$TargetY            = 100
$TargetWidth        = 1280
$TargetHeight       = 720

$Exclusions = @(
    'CEF-OSC-WIDGET',
    'Shell_TrayWnd',
    'Progman',
    'WorkerW',
    'ApplicationFrameWindow',
    'Cortana'
)

# 3. Architecture-compatible style wrappers.

function Get-WindowStyle {
    param(
        [IntPtr]$Handle,
        [int]$Index
    )

    if ([IntPtr]::Size -eq 8) {
        return [SafeWin32]::GetWindowLongPtr64(
            $Handle, $Index
        ).ToInt64()
    }

    return [long][SafeWin32]::GetWindowLong32(
        $Handle, $Index
    )
}

function Set-WindowStyle {
    param(
        [IntPtr]$Handle,
        [int]$Index,
        [long]$Style
    )

    if ([IntPtr]::Size -eq 8) {
        $Value = [IntPtr]::new($Style)

        [void][SafeWin32]::SetWindowLongPtr64(
            $Handle, $Index, $Value
        )
    }
    else {
        [void][SafeWin32]::SetWindowLong32(
            $Handle, $Index, [int]$Style
        )
    }
}

# 4. Reset counters.

$script:Processed   = 0
$script:Owned       = 0
$script:Skipped     = 0
$script:Failed      = 0

# Release current mouse capture if applicable.
[void][SafeWin32]::ReleaseCapture()

# 5. Enumerate visible top-level windows.

$Callback = [SafeWin32+EnumWindowsProc]{
    param(
        [IntPtr]$hWnd,
        [IntPtr]$lParam
    )

    try {
        if (
            -not [SafeWin32]::IsWindow($hWnd) -or
            -not [SafeWin32]::IsWindowVisible($hWnd)
        ) {
            $script:Skipped++
            return $true
        }

        $Buffer = [System.Text.StringBuilder]::new(256)

        [void][SafeWin32]::GetClassNameW(
            $hWnd, $Buffer, 256
        )

        $Class = $Buffer.ToString()

        if (
            $Exclusions -contains $Class -or
            $Class -like 'Windows.UI.*'
        ) {
            $script:Skipped++
            return $true
        }

        $Style = Get-WindowStyle -Handle $hWnd -Index $GWL_STYLE

        # EnumWindows returns top-level windows, not every child control.
        # Never remove WS_CHILD or independently reposition embedded controls.
        if (($Style -band $WS_CHILD) -ne 0) {
            $script:Skipped++
            return $true
        }

        # Determine whether this top-level window has an owner.
        $Owner = [SafeWin32]::GetWindow($hWnd, $GW_OWNER)

        if ($Owner -ne [IntPtr]::Zero) {
            # Keep owned dialogs attached to their application.
            # Do not force their styles or geometry independently.
            $script:Owned++
            return $true
        }

        # Independent top-level window: convert to a conventional
        # resizable window while preserving unrelated style bits.
        $Style = $Style -band (-bnot $WS_POPUP)

        $Style = $Style -bor (
            $WS_CAPTION -bor
            $WS_THICKFRAME -bor
            $WS_SYSMENU -bor
            $WS_MINIMIZEBOX -bor
            $WS_MAXIMIZEBOX
        )

        Set-WindowStyle -Handle $hWnd `
            -Index $GWL_STYLE -Style $Style

        # Remove topmost status while preserving other extended styles.
        $ExStyle = Get-WindowStyle `
            -Handle $hWnd -Index $GWL_EXSTYLE

        $ExStyle = $ExStyle -band (-bnot $WS_EX_TOPMOST)

        Set-WindowStyle -Handle $hWnd `
            -Index $GWL_EXSTYLE -Style $ExStyle

        [void][SafeWin32]::ShowWindow($hWnd, $SW_RESTORE)

        $Flags = $SWP_NOZORDER -bor
                 $SWP_NOOWNERZORDER -bor
                 $SWP_FRAMECHANGED

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
        # One failing window must not stop the enumeration.
        $script:Failed++
    }

    return $true
}

try {
    [void][SafeWin32]::EnumWindows(
        $Callback, [IntPtr]::Zero
    )
}
finally {
    $Callback = $null
}

# 6. Report results.

Write-Output (
    'Recovery complete: Independent={0}; OwnedSkipped={1}; ' +
    'Skipped={2}; Failed={3}' -f
    $script:Processed,
    $script:Owned,
    $script:Skipped,
    $script:Failed
)
