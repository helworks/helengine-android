<#
.SYNOPSIS
Builds and verifies the standalone Android native host debug APK.
.DESCRIPTION
Uses the checked-in Gradle wrapper and an installed SDK with accepted licenses.
Checks APK signing, application identity, both 64-bit libraries and NativeActivity exports.
Outputs and diagnostic logs stay in a visible workspace build directory.
#>
[CmdletBinding()]
param(
    [string]$SdkRoot = '',
    [string]$JavaHome = '',
    [string]$OutputDirectory = '',
    [string]$WorkingDirectory = '',
    [string]$GradleCacheRoot = '',
    [string]$GeneratedCoreRoot = '',
    [string]$ContentRoot = '',
    [ValidateSet('unspecified','portrait','landscape')]
    [string]$ScreenOrientation = 'unspecified'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$platformRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspaceBuildRoot = [IO.Path]::GetFullPath((Join-Path $platformRoot '../builds/helengine-android'))
if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
        $SdkRoot = $env:ANDROID_HOME
    } elseif (-not [string]::IsNullOrWhiteSpace($env:ANDROID_SDK_ROOT)) {
        $SdkRoot = $env:ANDROID_SDK_ROOT
    } else {
        $SdkRoot = Join-Path $workspaceBuildRoot 'toolchain/sdk'
    }
}
if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    if (-not [string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
        $JavaHome = $env:JAVA_HOME
    } else {
        $javaRoot = Join-Path $workspaceBuildRoot 'toolchain/java'
        if (Test-Path -LiteralPath $javaRoot -PathType Container) {
            $javaCandidates = @(Get-ChildItem -LiteralPath $javaRoot -Directory | Where-Object {
                $releasePath = Join-Path $_.FullName 'release'
                (Test-Path -LiteralPath $releasePath -PathType Leaf) -and
                    (Get-Content -LiteralPath $releasePath -Raw) -match 'JAVA_VERSION="17\.'
            })
            if ($javaCandidates.Count -eq 1) {
                $JavaHome = $javaCandidates[0].FullName
            }
        }
    }
}
if ([string]::IsNullOrWhiteSpace($JavaHome)) {
    throw 'Set -JavaHome or JAVA_HOME to a JDK 17 installation.'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $workspaceBuildRoot 'native-host/output'
}
if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) {
    $WorkingDirectory = Join-Path $workspaceBuildRoot 'native-host/working'
}
if ([string]::IsNullOrWhiteSpace($GradleCacheRoot)) {
    $GradleCacheRoot = Join-Path $workspaceBuildRoot 'gradle-cache'
}
$sdkPath = [IO.Path]::GetFullPath($SdkRoot)
$javaPath = [IO.Path]::GetFullPath($JavaHome)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$workingPath = [IO.Path]::GetFullPath($WorkingDirectory)
$cachePath = [IO.Path]::GetFullPath($GradleCacheRoot)
$javaReleasePath = Join-Path $javaPath 'release'
if (-not (Test-Path -LiteralPath (Join-Path $javaPath 'bin/java.exe') -PathType Leaf) -or
    -not (Test-Path -LiteralPath $javaReleasePath -PathType Leaf) -or
    (Get-Content -LiteralPath $javaReleasePath -Raw) -notmatch 'JAVA_VERSION="17\.') {
    throw "JDK 17 is required for the pinned build toolchain: $javaPath"
}

$apkSigner = Join-Path $sdkPath 'build-tools/35.0.0/apksigner.bat'
$aapt = Join-Path $sdkPath 'build-tools/35.0.0/aapt.exe'
$zipAlign = Join-Path $sdkPath 'build-tools/35.0.0/zipalign.exe'
$readElf = Join-Path $sdkPath 'ndk/27.0.12077973/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe'
foreach ($requiredPath in @(
    (Join-Path $sdkPath 'platforms/android-35/android.jar'),
    (Join-Path $sdkPath 'cmake/3.22.1/bin/cmake.exe'),
    $apkSigner, $aapt, $zipAlign, $readElf
)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required Android build tool is missing: $requiredPath. Install the SDK packages listed in README.md and accept their licenses."
    }
}
New-Item -ItemType Directory -Force -Path $outputPath,$workingPath,$cachePath | Out-Null
# Keep Ninja/CMake paths below Windows' legacy length limit even when the editor cache root is deeply nested.
$nativeCacheIdentity = "$platformRoot|$sdkPath|$GeneratedCoreRoot|$ContentRoot"
$pathHasher = [Security.Cryptography.SHA256]::Create()
try { $nativeCacheHash = [BitConverter]::ToString($pathHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($nativeCacheIdentity))).Replace('-','').Substring(0,16).ToLowerInvariant() }
finally { $pathHasher.Dispose() }
$shortBuildRoot = Join-Path $workspaceBuildRoot "apk/$nativeCacheHash"
$gradleAppRoot = Join-Path $shortBuildRoot 'app'
$nativeBuildRoot = Join-Path $shortBuildRoot 'cxx'
New-Item -ItemType Directory -Force -Path $shortBuildRoot | Out-Null
$buildLog = Join-Path $workingPath 'gradle.log'
$savedJavaHome = $env:JAVA_HOME
$savedAndroidHome = $env:ANDROID_HOME
$savedAndroidSdkRoot = $env:ANDROID_SDK_ROOT
$savedGradleHome = $env:GRADLE_USER_HOME

try {
    $env:JAVA_HOME = $javaPath
    $env:ANDROID_HOME = $sdkPath
    $env:ANDROID_SDK_ROOT = $sdkPath
    $env:GRADLE_USER_HOME = $cachePath
    $ErrorActionPreference = 'Continue'
    & (Join-Path $platformRoot 'android/gradlew.bat') -p (Join-Path $platformRoot 'android') `
        ':app:assembleDebug' --no-daemon --console=plain `
        "-PhelengineBuildRoot=$gradleAppRoot" "-PhelengineNativeBuildRoot=$nativeBuildRoot" `
        "-PhelengineCoreRoot=$GeneratedCoreRoot" "-PhelengineContentRoot=$ContentRoot" "-PhelengineOrientation=$ScreenOrientation" *> $buildLog
    $buildExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($buildExitCode -ne 0) {
        throw "Android host build failed. See $buildLog"
    }
    $builtApk = Join-Path $gradleAppRoot 'outputs/apk/debug/app-debug.apk'
    if (-not (Test-Path -LiteralPath $builtApk -PathType Leaf) -or (Get-Item -LiteralPath $builtApk).Length -eq 0) {
        throw "Gradle did not produce a nonempty APK: $builtApk"
    }
    & $apkSigner verify --verbose $builtApk *> (Join-Path $workingPath 'apk-signature.log')
    if ($LASTEXITCODE -ne 0) {
        throw 'APK signature verification failed.'
    }
    & $zipAlign -c -P 16 -v 4 $builtApk *> (Join-Path $workingPath 'apk-alignment.log')
    if ($LASTEXITCODE -ne 0) {
        throw 'APK native library alignment verification failed.'
    }
    $badgingPath = Join-Path $workingPath 'apk-badging.log'
    & $aapt dump badging $builtApk *> $badgingPath
    if ($LASTEXITCODE -ne 0) {
        throw 'APK manifest inspection failed.'
    }
    $badging = Get-Content -LiteralPath $badgingPath -Raw
    if ($badging -notmatch "package: name='com\.helengine\.android\.scaffold'" -or
        $badging -notmatch "launchable-activity: name='android\.app\.NativeActivity'") {
        throw 'APK does not expose the expected Android native host application.'
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($builtApk)
    try {
        if (-not [string]::IsNullOrWhiteSpace($GeneratedCoreRoot)) {
            foreach ($asset in @('android-content-files.txt','android-content.json','shaders/scene.vert.spv','shaders/scene.frag.spv')) {
                $packagedEntry = $archive.GetEntry("assets/$asset")
                if ($null -eq $packagedEntry -or $packagedEntry.Length -eq 0) {
                    throw "Game APK is missing its authored runtime asset: $asset"
                }
            }
            $inventoryReader = [IO.StreamReader]::new($archive.GetEntry('assets/android-content-files.txt').Open())
            try { $inventory = $inventoryReader.ReadToEnd() -split '\r?\n' } finally { $inventoryReader.Dispose() }
            foreach ($asset in $inventory) {
                if ([string]::IsNullOrWhiteSpace($asset)) { continue }
                if ($null -eq $archive.GetEntry("assets/$asset")) { throw "Cooked asset is absent from the APK: $asset" }
            }
        }
        foreach ($shader in @('boot.vert.spv','boot.frag.spv')) {
            $shaderEntry = $archive.GetEntry("assets/shaders/$shader")
            if ($null -eq $shaderEntry -or $shaderEntry.Length -lt 20 -or $shaderEntry.Length % 4 -ne 0) {
                throw "APK is missing a compiled Vulkan shader: $shader"
            }
        }
        foreach ($abi in @('arm64-v8a','x86_64')) {
            $entry = $archive.GetEntry("lib/$abi/libhelengine_android.so")
            if ($null -eq $entry -or $entry.Length -eq 0) {
                throw "APK is missing the native host for $abi"
            }
            $libraryPath = Join-Path $workingPath "$abi-libhelengine_android.so"
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $libraryPath, $true)
            $elfLog = Join-Path $workingPath "$abi-elf.log"
            & $readElf --file-header --program-headers --dyn-syms --wide $libraryPath *> $elfLog
            if ($LASTEXITCODE -ne 0) {
                throw "Native library inspection failed for $abi"
            }
            $elf = Get-Content -LiteralPath $elfLog -Raw
            if ($elf -notmatch 'Class:\s+ELF64' -or
                $elf -notmatch 'GLOBAL\s+DEFAULT\s+\d+\s+ANativeActivity_onCreate') {
                throw "NativeActivity entry point is missing from the 64-bit $abi library."
            }
            $expectedMachine = if ($abi -eq 'arm64-v8a') { 'AArch64' } else { 'Advanced Micro Devices X86-64' }
            if ($elf -notmatch ('Machine:\s+' + [regex]::Escape($expectedMachine))) {
                throw "Native library machine type does not match $abi"
            }
            $loadSegments = [regex]::Matches($elf, '(?m)^\s*LOAD\s+.*\s+(0x[0-9a-fA-F]+)\s*$')
            if ($loadSegments.Count -eq 0) {
                throw "Native library has no loadable segments for $abi"
            }
            foreach ($segment in $loadSegments) {
                $alignment = [Convert]::ToInt64($segment.Groups[1].Value.Substring(2), 16)
                if ($alignment -lt 16384) {
                    throw "Native library segment alignment is below 16 KB for $abi"
                }
            }
        }
    } finally {
        $archive.Dispose()
    }

    $destinationApk = Join-Path $outputPath 'helengine_android_host-debug.apk'
    Copy-Item -LiteralPath $builtApk -Destination $destinationApk -Force
    [pscustomobject][ordered]@{
        applicationId = 'com.helengine.android.scaffold'
        packageKind = $(if ([string]::IsNullOrWhiteSpace($GeneratedCoreRoot)) { 'native-host-probe' } else { 'generated-native-game' })
        generatedCoreRoot = $GeneratedCoreRoot
        contentRoot = $ContentRoot
        screenOrientation = $ScreenOrientation
        apkPath = $destinationApk
        sha256 = (Get-FileHash -LiteralPath $destinationApk -Algorithm SHA256).Hash.ToLowerInvariant()
        verifiedAbis = @('arm64-v8a','x86_64')
        signatureVerified = $true
        nativeActivityExportsVerified = $true
        nativeLibraryAlignmentVerified = $true
        graphicsBackend = 'vulkan'
        packagedSpirvShadersVerified = $true
        authoredContentInventoryVerified = (-not [string]::IsNullOrWhiteSpace($GeneratedCoreRoot))
        builtAtUtc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputPath 'host-build-report.json') -Encoding UTF8
    Write-Output "Verified Android native host APK: $destinationApk"
} finally {
    [Environment]::SetEnvironmentVariable('JAVA_HOME', $savedJavaHome, 'Process')
    [Environment]::SetEnvironmentVariable('ANDROID_HOME', $savedAndroidHome, 'Process')
    [Environment]::SetEnvironmentVariable('ANDROID_SDK_ROOT', $savedAndroidSdkRoot, 'Process')
    [Environment]::SetEnvironmentVariable('GRADLE_USER_HOME', $savedGradleHome, 'Process')
}
