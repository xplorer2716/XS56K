# Drives a running desktop application through Windows UI Automation (the accessibility interface screen readers use), without the
# mouse: lists the elements of its windows with their names and tooltips, invokes one by its name, types keys into one (only when the
# application's window is in the foreground, so that no key lands in another program), and captures its main window.
# Windows only (Windows PowerShell 5.1 or PowerShell 7). Generic: any application whose components are exposed to UI Automation, JUCE
# applications included (XS56K by default). See .github/skills/ui-automation/SKILL.md.
#
# Usage:
#   powershell -NoProfile -File juce/tools/uia.ps1 -Action list       [-Process XS56K] [-Filter <regex>]
#   powershell -NoProfile -File juce/tools/uia.ps1 -Action invoke     -Name <exact name> [-Process XS56K]
#   powershell -NoProfile -File juce/tools/uia.ps1 -Action type       (-Name <exact name> | -Edit <index>) -Keys <SendKeys text> [-Process XS56K]
#   powershell -NoProfile -File juce/tools/uia.ps1 -Action screenshot -Out <file.png> [-Process XS56K]
#
# Exit codes: 0 done; 1 process or element not found; 2 keys not sent because the foreground window is another program's;
#             3 the element has no pattern for the action.
param(
    [Parameter(Mandatory = $true)][ValidateSet('list', 'invoke', 'type', 'screenshot')][string]$Action,
    [string]$Process = 'XS56K',
    [string]$Name = '',
    [int]$Edit = -1,
    [string]$Keys = '',
    [string]$Filter = '',
    [string]$Out = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class UiaWin32 {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$PRINT_FULL_CONTENT = 2       # PW_RENDERFULLCONTENT: captures a window that is not in the foreground
$FOCUS_SETTLE_MS = 500

$target = Get-Process -Name $Process -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $target) { "process not found: $Process"; exit 1 }

$auto = [System.Windows.Automation.AutomationElement]
$scope = [System.Windows.Automation.TreeScope]
$byProcess = New-Object System.Windows.Automation.PropertyCondition($auto::ProcessIdProperty, $target.Id)
$windows = @($auto::RootElement.FindAll($scope::Children, $byProcess))

function All-Elements {
    foreach ($window in $windows) {
        $window
        foreach ($element in $window.FindAll($scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) { $element }
    }
}

function Find-Target {
    if ($Edit -ge 0) {
        $edits = @(All-Elements | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Edit })
        if ($edits.Count -gt $Edit) { return $edits[$Edit] }
        return $null
    }
    return All-Elements | Where-Object { $_.Current.Name -eq $Name } | Select-Object -First 1
}

switch ($Action) {
    'list' {
        foreach ($window in $windows) {
            $lines = @()
            foreach ($element in $window.FindAll($scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
                $line = "  [" + $element.Current.ControlType.ProgrammaticName.Replace('ControlType.', '') + "] '" + $element.Current.Name + "'"
                if ($element.Current.HelpText) { $line += " tip='" + $element.Current.HelpText + "'" }
                if (-not $Filter -or $line -match $Filter) { $lines += $line }
            }
            # A window is named only when it has something to show (a popup menu or a dialog is a window of its own).
            if ($lines.Count -gt 0 -or -not $Filter) { "WINDOW '" + $window.Current.Name + "'"; $lines }
        }
        exit 0
    }
    'invoke' {
        $element = Find-Target
        if (-not $element) { "not found: $Name"; exit 1 }
        $pattern = $null
        if ($element.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pattern)) { $pattern.Invoke(); "invoked '$Name'"; exit 0 }
        if ($element.TryGetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern, [ref]$pattern)) { $pattern.Expand(); "expanded '$Name'"; exit 0 }
        if ($element.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern, [ref]$pattern)) { $pattern.Select(); "selected '$Name'"; exit 0 }
        "no pattern to invoke '$Name'"
        exit 3
    }
    'type' {
        $element = Find-Target
        if (-not $element) { "not found: " + $(if ($Edit -ge 0) { "edit $Edit" } else { $Name }); exit 1 }
        $element.SetFocus()
        Start-Sleep -Milliseconds $FOCUS_SETTLE_MS
        $foregroundProcess = 0
        [UiaWin32]::GetWindowThreadProcessId([UiaWin32]::GetForegroundWindow(), [ref]$foregroundProcess) | Out-Null
        if ($foregroundProcess -ne $target.Id) { "not sent: the foreground window belongs to process $foregroundProcess, not $Process ($($target.Id))"; exit 2 }
        [System.Windows.Forms.SendKeys]::SendWait($Keys)
        "sent '$Keys'"
        exit 0
    }
    'screenshot' {
        if (-not $Out) { "-Out is needed"; exit 1 }
        $rect = New-Object UiaWin32+RECT
        [UiaWin32]::GetWindowRect($target.MainWindowHandle, [ref]$rect) | Out-Null
        $bitmap = New-Object System.Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $hdc = $graphics.GetHdc()
        [UiaWin32]::PrintWindow($target.MainWindowHandle, $hdc, $PRINT_FULL_CONTENT) | Out-Null
        $graphics.ReleaseHdc($hdc)
        $bitmap.Save($Out)
        "saved $Out"
        exit 0
    }
}
