# Builds the release archive: the DLL from the Release build, dist/ as the
# mod's data, LICENSE.txt, README.txt and THIRD-PARTY.txt. Nothing else -- no PDB,
# no local override files (only _Example.ini.txt ships from Overrides/).
#
#   powershell -File docs/package.ps1            # build/ModernCameraCollision-<version>.zip
#   powershell -File docs/package.ps1 -Version 1.0.1
param(
    [string]$Version = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $Version) {
    $Version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(ModernCameraCollision VERSION ([0-9.]+)').Matches[0].Groups[1].Value
}
$dll = Join-Path $root 'build\src\Release\ModernCameraCollision.dll'
if (-not (Test-Path $dll)) { throw "no Release DLL at $dll; build first" }

$stage = Join-Path $root "build\package\ModernCameraCollision"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage 'SKSE\Plugins') | Out-Null

# dist/ is the mod's data layout. Overrides/ ships the example only: any
# other .ini there is a local test file.
Get-ChildItem -Path (Join-Path $root 'dist') -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring((Join-Path $root 'dist').Length + 1)
    if ($rel -like 'SKSE\Plugins\ModernCameraCollision\Overrides\*' -and $_.Name -ne '_Example.ini.txt') { return }
    $dest = Join-Path $stage $rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $dest) | Out-Null
    Copy-Item $_.FullName $dest
}
Copy-Item $dll (Join-Path $stage 'SKSE\Plugins\')
# The GPL asks for the licence text, a copyright statement with the warranty
# disclaimer, and a way to the source; LICENSE.txt and README.txt carry them.
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'THIRD-PARTY.md') (Join-Path $stage 'THIRD-PARTY.txt')
@"
Modern Camera Collision $Version
Copyright (C) 2026 DisizMod

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program, in LICENSE.txt.  If not, see
<https://www.gnu.org/licenses/>.

Source code: https://github.com/DisizMod/ModernCameraCollision
Third-party software and its licences: THIRD-PARTY.txt
"@ | Out-File -FilePath (Join-Path $stage 'README.txt') -Encoding ascii

$zip = Join-Path $root "build\ModernCameraCollision-$Version.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

Write-Output "packaged $zip"
Get-ChildItem -Path $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($stage.Length + 1) }
