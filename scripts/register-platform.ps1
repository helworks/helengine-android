<#
.SYNOPSIS
Publishes the Android metadata builder and registers it with a source editor checkout.
.DESCRIPTION
Preserves other platform entries and updates Android entries for the selected engine version.
When EngineVersion is omitted, registers every engine version already present in the registry.
Registration does not require an Android SDK; game export requires the configured native toolchain.
#>
[CmdletBinding()]
param(
    [string]$EngineUserSettingsRoot = '',
    [string]$EngineVersion = '',
    [string]$HelengineRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$platformRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($EngineUserSettingsRoot)) {
    $EngineUserSettingsRoot = Join-Path $platformRoot '../helengine/user_settings'
}
if ([string]::IsNullOrWhiteSpace($HelengineRoot)) {
    $HelengineRoot = Join-Path $platformRoot '../helengine'
}
$engineRoot = [IO.Path]::GetFullPath($HelengineRoot)
$settingsRoot = [IO.Path]::GetFullPath($EngineUserSettingsRoot)
$manifestPath = Join-Path $settingsRoot 'platforms.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Existing engine platform registry required: $manifestPath"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($null -eq $manifest.PSObject.Properties['platforms'] -or $null -eq $manifest.platforms) {
    throw "The registry must contain a platforms array: $manifestPath"
}
if (-not [string]::IsNullOrWhiteSpace($EngineVersion)) {
    $engineVersions = @($EngineVersion)
} else {
    $engineVersions = @($manifest.platforms | ForEach-Object { $_.engineVersion } | Sort-Object -Unique)
}
if ($engineVersions.Count -eq 0 -or @($engineVersions | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -gt 0) {
    throw 'Provide -EngineVersion or use a registry containing valid engine versions.'
}

$builderOutput = Join-Path $platformRoot 'artifacts/builder'
& dotnet publish (Join-Path $platformRoot 'builder/helengine.android.builder.csproj') `
    -c Debug -o $builderOutput "-p:HelengineRoot=$engineRoot" --nologo -v minimal
if ($LASTEXITCODE -ne 0) {
    throw "Android builder publish failed with exit code $LASTEXITCODE. The registry was not changed."
}
$builderAssembly = Join-Path $builderOutput 'helengine.android.builder.dll'
if (-not (Test-Path -LiteralPath $builderAssembly -PathType Leaf)) {
    throw "Published Android builder missing: $builderAssembly"
}

# Replace only the Android entries for the selected versions and retain every other entry.
$entries = @($manifest.platforms | Where-Object {
    $_.platformId -ne 'android' -or $engineVersions -notcontains $_.engineVersion
})
foreach ($version in $engineVersions) {
    $entries += [pscustomobject][ordered]@{
        engineVersion = $version
        platformId = 'android'
        displayName = 'Android'
        builderAssemblyPath = $builderAssembly
        playerSourceRootPath = $platformRoot
        generatedCoreCppRootPath = ''
        pluginManifestPath = (Join-Path $platformRoot 'platform-plugin.json')
    }
}
$manifest.platforms = $entries
$json = ($manifest | ConvertTo-Json -Depth 32) + [Environment]::NewLine
if ($json -ne [IO.File]::ReadAllText($manifestPath)) {
    [IO.File]::WriteAllText($manifestPath, $json, [Text.UTF8Encoding]::new($false))
}
Write-Output "Registered Android for $($engineVersions -join ', ') in $manifestPath"
