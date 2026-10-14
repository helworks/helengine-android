<#
.SYNOPSIS
Checks Vulkan startup, surface replacement, resize, and teardown in the private Android host emulator.
.DESCRIPTION
Optionally injects an official Khronos validation-layer binary into the debug app.
Restores emulator display overrides and GPU-debug settings after the check, and leaves the host running.
No screen or window images are captured.
#>
[CmdletBinding()]
param(
    [string]$Serial = 'emulator-5580',
    [string]$SdkRoot = '',
    [string]$ApkPath = '',
    [string]$ValidationLayerPath = '',
    [string]$ExpectedStartupScene = '',
    [string]$StartupSceneOverride = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../builds/helengine-android/native-host'))
if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
    $SdkRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../builds/helengine-android/toolchain/sdk'))
}
if ([string]::IsNullOrWhiteSpace($ApkPath)) {
    $ApkPath = Join-Path $buildRoot 'output/helengine_android_host-debug.apk'
}
$adb = Join-Path ([IO.Path]::GetFullPath($SdkRoot)) 'platform-tools/adb.exe'
$applicationId = 'com.helengine.android.scaffold'
$component = "$applicationId/com.helengine.android.HelengineActivity"
$testRoot = Join-Path $buildRoot 'boot'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

function Invoke-HostAdb {
    <# .SYNOPSIS Runs one checked command against the explicitly selected emulator. #>
    param([string[]]$Arguments)
    # PowerShell 5 treats successful adb progress on stderr as an error record; the process exit code defines failure.
    $ErrorActionPreference = 'Continue'
    $output = & $adb -s $Serial @Arguments 2>&1
    $result = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($result -ne 0) { throw "adb command failed: $($Arguments -join ' ')`n$($output -join [Environment]::NewLine)" }
    return $output
}

function Get-VulkanHostLog {
    <# .SYNOPSIS Saves and returns the current emulator log for renderer and validation checks. #>
    $lines = Invoke-HostAdb @('logcat','-d')
    $lines | Set-Content -LiteralPath (Join-Path $testRoot 'vulkan-validation-full.log') -Encoding UTF8
    return ($lines -join [Environment]::NewLine)
}

function Assert-VulkanHostHealthy {
    <# .SYNOPSIS Requires a successful native Vulkan presentation and rejects native or validation errors. #>
    param([string]$Log)
    $presentationPattern = if ([string]::IsNullOrWhiteSpace($ExpectedStartupScene)) { 'Vulkan triangle frame presented:' } else { 'Vulkan scene frame presented:.*vertices=[1-9]' }
    if ($Log -notmatch $presentationPattern -or
        $Log -match 'VUID-|Validation Error|SYNC-HAZARD|FATAL EXCEPTION|Fatal signal|E HelengineAndroid') {
        throw "Vulkan host did not pass validation. See $testRoot/vulkan-validation-full.log"
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedStartupScene)) {
        if ($Log -notmatch ('startup=' + [regex]::Escape($ExpectedStartupScene) + ' ') -or
            $Log -notmatch 'Authored runtime update:.*meshDraws=[1-9]') { throw 'Expected authored startup scene and mesh draw were not observed.' }
    }
    $appProcessId = (Invoke-HostAdb @('shell','pidof',$applicationId)) -join ''
    if ($appProcessId.Trim() -notmatch '^\d+$') { throw 'The Vulkan host is not running.' }
}

$avdName = Invoke-HostAdb @('emu','avd','name')
if ($avdName -notcontains 'helengine_android_host') { throw 'This check requires the private helengine_android_host emulator.' }
$boot = (Invoke-HostAdb @('shell','getprop','sys.boot_completed')) -join ''
if ($boot.Trim() -ne '1') { throw 'The Android host emulator must finish booting before this check.' }
$display = Invoke-HostAdb @('shell','wm','size')
$displayOverride = @($display | Select-String -Pattern 'Override size: (\d+x\d+)')
$gpuKeys = @('enable_gpu_debug_layers','gpu_debug_app','gpu_debug_layers')
$savedGpuSettings = @{}
foreach ($key in $gpuKeys) { $savedGpuSettings[$key] = ((Invoke-HostAdb @('shell','settings','get','global',$key)) -join '').Trim() }

try {
    Invoke-HostAdb @('install','-r',([IO.Path]::GetFullPath($ApkPath))) | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($ValidationLayerPath)) {
        $layer = [IO.Path]::GetFullPath($ValidationLayerPath)
        if (-not (Test-Path -LiteralPath $layer -PathType Leaf)) { throw "Validation layer missing: $layer" }
        Invoke-HostAdb @('push',$layer,'/data/local/tmp/libVkLayer_khronos_validation.so') | Out-Null
        Invoke-HostAdb @('shell','run-as',$applicationId,'cp','/data/local/tmp/libVkLayer_khronos_validation.so','.') | Out-Null
        Invoke-HostAdb @('shell','settings','put','global','enable_gpu_debug_layers','1') | Out-Null
        Invoke-HostAdb @('shell','settings','put','global','gpu_debug_app',$applicationId) | Out-Null
        Invoke-HostAdb @('shell','settings','put','global','gpu_debug_layers','VK_LAYER_KHRONOS_validation') | Out-Null
    }
    Invoke-HostAdb @('shell','am','force-stop',$applicationId) | Out-Null
    Invoke-HostAdb @('logcat','-c') | Out-Null
    $startupArguments = @('shell','am','start','-W','-n',$component)
    if (-not [string]::IsNullOrWhiteSpace($StartupSceneOverride)) {
        $startupArguments += @('--es','helengine.startup_scene',$StartupSceneOverride)
    }
    Invoke-HostAdb $startupArguments | Out-Null
    Start-Sleep -Seconds $(if ([string]::IsNullOrWhiteSpace($ExpectedStartupScene)) { 2 } else { 8 })
    $startupLog = Get-VulkanHostLog
    Assert-VulkanHostHealthy $startupLog
    if (-not [string]::IsNullOrWhiteSpace($ExpectedStartupScene)) {
        $rotations = @([regex]::Matches($startupLog, 'Authored mesh transform:.*quaternion=\(([^\r\n]+)\)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        if ($rotations.Count -lt 2) { throw 'Authored mesh rotation did not advance across sampled runtime frames.' }
    }
    if (-not [string]::IsNullOrWhiteSpace($ValidationLayerPath) -and $startupLog -notmatch 'Loaded layer VK_LAYER_KHRONOS_validation') {
        throw 'The requested Khronos validation layer was not loaded.'
    }

    Invoke-HostAdb @('shell','input','keyevent','KEYCODE_HOME') | Out-Null
    Start-Sleep -Seconds 2
    $backgroundLog = Get-VulkanHostLog
    if ($backgroundLog -notmatch 'Vulkan surface released\.') { throw 'Backgrounding did not release the Vulkan surface.' }
    Invoke-HostAdb @('shell','am','start','-W','-n',$component) | Out-Null
    Start-Sleep -Seconds 2
    $resumeLog = Get-VulkanHostLog
    Assert-VulkanHostHealthy $resumeLog
    if ([regex]::Matches($resumeLog, 'Vulkan swapchain created:').Count -le [regex]::Matches($backgroundLog, 'Vulkan swapchain created:').Count) {
        throw 'Resuming did not create a replacement Vulkan swapchain.'
    }

    Invoke-HostAdb @('shell','wm','size','900x1600') | Out-Null
    Start-Sleep -Seconds 2
    $resizeLog = Get-VulkanHostLog
    Assert-VulkanHostHealthy $resizeLog
    if ([string]::IsNullOrWhiteSpace($ExpectedStartupScene)) {
        if ($resizeLog -notmatch 'Vulkan triangle frame presented: 900x1600\.') { throw 'The Vulkan probe did not render at the resized extent.' }
    } elseif ($resizeLog -notmatch 'Vulkan swapchain created: (900x1600|1600x900)') { throw 'The Vulkan scene did not recreate its resized render surface.' }
    if ($displayOverride.Count -gt 0) {
        Invoke-HostAdb @('shell','wm','size',$displayOverride[0].Matches[0].Groups[1].Value) | Out-Null
    } else {
        Invoke-HostAdb @('shell','wm','size','reset') | Out-Null
    }
    Start-Sleep -Seconds 2

    $tasks = (Invoke-HostAdb @('shell','dumpsys','activity','activities')) -join [Environment]::NewLine
    $taskMatches = [regex]::Matches($tasks, '(?m)^\s*\* Task\{[^\r\n]+ #(\d+) type=standard A=\d+:com\.helengine\.android\.scaffold')
    $taskIds = @($taskMatches | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($taskIds.Count -ne 1) { throw 'Expected exactly one native host task.' }
    Invoke-HostAdb @('shell','am','stack','remove',$taskIds[0]) | Out-Null
    Start-Sleep -Seconds 2
    $teardownLog = Get-VulkanHostLog
    $destroyPattern = if ([string]::IsNullOrWhiteSpace($ExpectedStartupScene)) { 'Native host destroyed\.' } else { 'Android authored game host destroyed\.' }
    if ($teardownLog -notmatch $destroyPattern -or $teardownLog -match 'VUID-|Validation Error|SYNC-HAZARD|FATAL EXCEPTION|Fatal signal|E HelengineAndroid') {
        throw 'Vulkan teardown did not pass validation.'
    }
    [pscustomobject][ordered]@{
        apkSha256 = (Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash.ToLowerInvariant()
        graphicsBackend = 'vulkan'
        deviceSerial = $Serial
        nativeTrianglePresented = [string]::IsNullOrWhiteSpace($ExpectedStartupScene)
        authoredStartupScene = $ExpectedStartupScene
        authoredRotationVerified = -not [string]::IsNullOrWhiteSpace($ExpectedStartupScene)
        backgroundAndResumeVerified = $true
        resizeVerified = $true
        teardownVerified = $true
        validationLayerEnabled = -not [string]::IsNullOrWhiteSpace($ValidationLayerPath)
        validationErrorsDetected = $false
        verifiedAtUtc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $buildRoot 'output/vulkan-host-test-report.json') -Encoding UTF8
    Write-Output "Vulkan startup, surface replacement, resize, and teardown passed. Logs: $testRoot"
} finally {
    if ($displayOverride.Count -gt 0) {
        Invoke-HostAdb @('shell','wm','size',$displayOverride[0].Matches[0].Groups[1].Value) | Out-Null
    } else {
        Invoke-HostAdb @('shell','wm','size','reset') | Out-Null
    }
    foreach ($key in $gpuKeys) {
        if ($savedGpuSettings[$key] -eq 'null') {
            Invoke-HostAdb @('shell','settings','delete','global',$key) | Out-Null
        } else {
            Invoke-HostAdb @('shell','settings','put','global',$key,$savedGpuSettings[$key]) | Out-Null
        }
    }
    Invoke-HostAdb @('shell','am','start','-W','-n',$component) | Out-Null
}
