<#
.SYNOPSIS
Cold-starts every packaged scene on the private emulator and checks native presentation and Vulkan errors.
.DESCRIPTION
Reads the scene catalog from the APK build report, saves per-scene logs, restores GPU debug settings,
and leaves the normal splash/menu flow running. The disabled loading overlay is allowed to have no visible vertices.
No screenshots are captured. This verifies scene startup; gameplay outcomes require separate input checks.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ApkPath,
    [string]$Serial = 'emulator-5580',
    [string]$SdkRoot = '',
    [string]$ValidationLayerPath = '',
    [int]$SettleSeconds = 6,
    [string]$SceneFilter = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($SettleSeconds -lt 2) { throw 'Scene startup needs at least two seconds to settle.' }
if (-not $SdkRoot) { $SdkRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../builds/helengine-android/toolchain/sdk')) }
$adb = Join-Path $SdkRoot 'platform-tools/adb.exe'
$ApkPath = [IO.Path]::GetFullPath($ApkPath)
$buildReport = Get-Content -LiteralPath (Join-Path (Split-Path $ApkPath -Parent) 'host-build-report.json') -Raw | ConvertFrom-Json
if ($buildReport.packageKind -ne 'generated-native-game') { throw 'The scene check requires an exported game APK.' }
if ((Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $buildReport.sha256) { throw 'APK does not match its verified build report.' }
$catalog = Join-Path $buildReport.generatedCoreRoot 'runtime/runtime_scene_catalog_manifest.cpp'
$sceneIds = @([regex]::Matches((Get-Content -LiteralPath $catalog -Raw), '\{ "([^"]+)", "cooked/scenes/') | ForEach-Object { $_.Groups[1].Value })
if ($SceneFilter) { $sceneIds = @($sceneIds | Where-Object { $_ -match $SceneFilter }) }
if ($sceneIds.Count -eq 0) { throw 'The selected scene catalog is empty.' }
$package = $buildReport.applicationId
$component = "$package/android.app.NativeActivity"
$logRoot = Join-Path (Split-Path $ApkPath -Parent) 'scene-smoke'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

function Invoke-SceneAdb {
    <# .SYNOPSIS Runs a checked adb command against the selected private emulator. #>
    param([string[]]$Arguments)
    $ErrorActionPreference = 'Continue'
    $lines = & $adb -s $Serial @Arguments 2>&1
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw "adb failed: $($Arguments -join ' '): $($lines -join ' ')" }
    return $lines
}
if ((Invoke-SceneAdb @('emu','avd','name')) -notcontains 'helengine_android_host') { throw 'This check requires the private helengine_android_host emulator.' }
$gpuKeys = @('enable_gpu_debug_layers','gpu_debug_app','gpu_debug_layers')
$savedGpuSettings = @{}
foreach ($key in $gpuKeys) { $savedGpuSettings[$key] = ((Invoke-SceneAdb @('shell','settings','get','global',$key)) -join '').Trim() }
$results = @()
try {
    Invoke-SceneAdb @('install','-r',$ApkPath) | Out-Null
    if ($ValidationLayerPath) {
        Invoke-SceneAdb @('push',([IO.Path]::GetFullPath($ValidationLayerPath)),'/data/local/tmp/libVkLayer_khronos_validation.so') | Out-Null
        Invoke-SceneAdb @('shell','run-as',$package,'cp','/data/local/tmp/libVkLayer_khronos_validation.so','.') | Out-Null
        Invoke-SceneAdb @('shell','settings','put','global','enable_gpu_debug_layers','1') | Out-Null
        Invoke-SceneAdb @('shell','settings','put','global','gpu_debug_app',$package) | Out-Null
        Invoke-SceneAdb @('shell','settings','put','global','gpu_debug_layers','VK_LAYER_KHRONOS_validation') | Out-Null
    }
    foreach ($sceneId in $sceneIds) {
        Invoke-SceneAdb @('shell','am','force-stop',$package) | Out-Null
        Invoke-SceneAdb @('logcat','-c') | Out-Null
        Invoke-SceneAdb @('shell','am','start','-W','-n',$component,'--es','helengine.startup_scene',$sceneId) | Out-Null
        Start-Sleep -Seconds $SettleSeconds
        $log = (Invoke-SceneAdb @('logcat','-d')) -join [Environment]::NewLine
        $log | Set-Content -LiteralPath (Join-Path $logRoot "$sceneId.log") -Encoding UTF8
        $problems = @([regex]::Matches($log,'(?m)^.*(?:VUID-|Validation Error|SYNC-HAZARD|FATAL EXCEPTION|Fatal signal|E HelengineAndroid).*$') | ForEach-Object { $_.Value })
        $presentation = if ($sceneId -eq 'SceneLoadingScreen') { 'Vulkan scene frame presented:' } else { 'Vulkan scene frame presented:.*vertices=[1-9]' }
        $passed = $log.Contains("startup=$sceneId ") -and $log -match $presentation -and $problems.Count -eq 0
        if ($ValidationLayerPath -and $log -notmatch 'Loaded layer VK_LAYER_KHRONOS_validation') { $passed = $false }
        $result = [pscustomobject]@{ SceneId=$sceneId; Passed=$passed; Errors=$problems; MeshDraws=@([regex]::Matches($log,'meshDraws=(\d+)') | ForEach-Object { [int]$_.Groups[1].Value }) }
        $results += $result
        $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logRoot "$sceneId.json") -Encoding UTF8
        Write-Output "$sceneId : $passed"
        if ($problems.Count -gt 0) { $problems | Select-Object -First 3 | Write-Output }
    }
    [pscustomobject]@{ ApkSha256=$buildReport.sha256; ValidationLayerEnabled=[bool]$ValidationLayerPath; Scenes=$results; VerifiedAtUtc=[DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $logRoot 'report.json') -Encoding UTF8
    if (@($results | Where-Object { -not $_.Passed }).Count -gt 0) { throw "Scene startup checks failed. See $logRoot/report.json" }
} finally {
    foreach ($key in $gpuKeys) {
        if ($savedGpuSettings[$key] -eq 'null') { Invoke-SceneAdb @('shell','settings','delete','global',$key) | Out-Null }
        else { Invoke-SceneAdb @('shell','settings','put','global',$key,$savedGpuSettings[$key]) | Out-Null }
    }
    Invoke-SceneAdb @('shell','am','force-stop',$package) | Out-Null
    Invoke-SceneAdb @('shell','am','start','-W','-n',$component) | Out-Null
}
