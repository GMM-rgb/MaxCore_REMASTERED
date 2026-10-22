
# ==============================================================================
# MaxCore - Desktop Recovery
# Window recovery | Explorer shell reconstruction | User-level execution
# ==============================================================================

$ErrorActionPreference = 'Continue'

# 1. Restore the per-user shell setting.

$RegPath = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Winlogon'

try {
    if (-not (Test-Path -LiteralPath $RegPath)) {
        New-Item -Path $RegPath -Force -ErrorAction Stop | Out-Null
    }

    Set-ItemProperty -Path $RegPath -Name 'Shell' `
        -Value 'explorer.exe' -ErrorAction Stop
}
catch {
    Write-Warning "Shell registry restoration failed: $_"
}

# 2. Compile Win32 declarations.

if (-not ('SafeWin32' -as [type])) {
    $Signature = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class SafeWin32
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool EnumWindows(
        EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindow(IntPtr hWnd, uint command);

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
    public static extern bool ShowWindow(IntPtr hWnd, int command);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(
        IntPtr hWnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(
        IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(
        string className, string windowName);

    [DllImport("user32.dll")]
    public static extern bool ReleaseCapture();
}
'@

    Add-Type -TypeDefinition $Signature -ErrorAction Stop
}

# 3. Constants.

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
$WS_EX_APPWINDOW    = 0x00040000

$GW_OWNER            = 4
$SW_RESTORE          = 9
$SWP_NOZORDER        = 0x0004
$SWP_FRAMECHANGED    = 0x0020
$SWP_NOOWNERZORDER   = 0x0200

$TargetX      = 100
$TargetY      = 100
$TargetWidth  = 1280
$TargetHeight = 720

# 4. Configurable exclusions.

$ExcludedProcesses = @(
    'dwm',
    'csrss',
    'winlogon',
    'services',
    'lsass',
    'smss',
    'wininit',
    'sihost',
    'StartMenuExperienceHost',
    'ShellExperienceHost',
    'SearchHost',
    'SearchIndexer',
    'TextInputHost',
    'LockApp',
    'RuntimeBroker',
    'ApplicationFrameHost',
    'System',
    'Registry'
)

$ExcludedClasses = @(
    'Shell_TrayWnd',
    'Shell_SecondaryTrayWnd',
    'Progman',
    'WorkerW',
    'CEF-OSC-WIDGET'
)

# 5. Architecture-compatible window-style access.

function Get-WindowStyle {
    param([IntPtr]$Handle, [int]$Index)

    if ([IntPtr]::Size -eq 8) {
        return [SafeWin32]::GetWindowLongPtr64(
            $Handle, $Index).ToInt64()
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
        [void][SafeWin32]::SetWindowLongPtr64(
            $Handle, $Index, [IntPtr]::new($Style))
    }
    else {
        [void][SafeWin32]::SetWindowLong32(
            $Handle, $Index, [int]$Style)
    }
}

# 6. Window recovery.

$script:Processed = 0
$script:OwnedProcessed = 0
$script:Skipped = 0
$script:Failed = 0

[void][SafeWin32]::ReleaseCapture()

$Callback = [SafeWin32+EnumWindowsProc]{
    param([IntPtr]$hWnd, [IntPtr]$lParam)

    try {
        if (
            -not [SafeWin32]::IsWindow($hWnd) -or
            -not [SafeWin32]::IsWindowVisible($hWnd)
        ) {
            $script:Skipped++
            return $true
        }

        $Buffer = [System.Text.StringBuilder]::new(256)
        [void][SafeWin32]::GetClassNameW($hWnd, $Buffer, 256)
        $Class = $Buffer.ToString()

        if (
            $ExcludedClasses -contains $Class -or
            $Class -like 'Windows.UI.*'
        ) {
            $script:Skipped++
            return $true
        }

        $ProcessId = [uint32]0

        [void][SafeWin32]::GetWindowThreadProcessId(
            $hWnd, [ref]$ProcessId)

        if ($ProcessId -eq 0) {
            $script:Skipped++
            return $true
        }

        try {
            $Process = [System.Diagnostics.Process]::GetProcessById(
                [int]$ProcessId)

            if ($ExcludedProcesses -contains $Process.ProcessName) {
                $script:Skipped++
                return $true
            }
        }
        catch {
            $script:Skipped++
            return $true
        }

        $Style = Get-WindowStyle -Handle $hWnd -Index $GWL_STYLE

        # Do not detach embedded child controls.
        if (($Style -band $WS_CHILD) -ne 0) {
            $script:Skipped++
            return $true
        }

        # Process owned top-level windows as well as independent windows.
        # Preserve their ownership relationship.
        $Owner = [SafeWin32]::GetWindow($hWnd, $GW_OWNER)

        if ($Owner -ne [IntPtr]::Zero) {
            $script:OwnedProcessed++
        }

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

        # Remove topmost status while retaining unrelated extended styles.
        $ExStyle = Get-WindowStyle -Handle $hWnd -Index $GWL_EXSTYLE
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

        if ($Success) {
            $script:Processed++
        }
        else {
            $script:Failed++
        }
    }
    catch {
        $script:Failed++
    }

    return $true
}

try {
    [void][SafeWin32]::EnumWindows($Callback, [IntPtr]::Zero)
}
catch {
    Write-Warning "Window enumeration failed: $_"
}
finally {
    $Callback = $null
}

# 7. Explorer shell reconstruction.

$ExplorerPath = Join-Path $env:windir 'explorer.exe'

function Get-ShellWindowState {
    $Desktop = [SafeWin32]::FindWindowW('Progman', $null)
    $Taskbar = [SafeWin32]::FindWindowW('Shell_TrayWnd', $null)

    [PSCustomObject]@{
        Desktop = ($Desktop -ne [IntPtr]::Zero)
        Taskbar = ($Taskbar -ne [IntPtr]::Zero)
    }
}

try {
    $Before = Get-ShellWindowState

    Write-Output (
        'Shell before recovery: Desktop={0}; Taskbar={1}' -f
        $Before.Desktop, $Before.Taskbar
    )

    $Explorer = Get-Process -Name explorer -ErrorAction SilentlyContinue

    if (-not $Explorer) {
        Start-Process -FilePath $ExplorerPath -ErrorAction Stop
        Start-Sleep -Milliseconds 1500
    }

    $AfterStart = Get-ShellWindowState

    # Retry shell reconstruction if the desktop or taskbar is missing.
    if (-not $AfterStart.Desktop -or -not $AfterStart.Taskbar) {
        $Explorer = Get-Process -Name explorer -ErrorAction SilentlyContinue

        if ($Explorer) {
            $Explorer | Stop-Process -Force -ErrorAction Stop
            Start-Sleep -Milliseconds 500
        }

        Start-Process -FilePath $ExplorerPath -ErrorAction Stop
        Start-Sleep -Milliseconds 2000
    }

    $FinalState = Get-ShellWindowState

    Write-Output (
        'Shell after recovery: Desktop={0}; Taskbar={1}' -f
        $FinalState.Desktop, $FinalState.Taskbar
    )

    if (-not $FinalState.Desktop -or -not $FinalState.Taskbar) {
        Write-Warning (
            'Some shell components remain unavailable. ' +
            'The current session may require further repair.'
        )
    }
}
catch {
    Write-Warning "Explorer reconstruction failed: $_"
}

# 8. Summary.

Write-Output (
    'Recovery complete: Processed={0}; Owned={1}; Skipped={2}; Failed={3}' -f
    $script:Processed,
    $script:OwnedProcessed,
    $script:Skipped,
    $script:Failed
)
