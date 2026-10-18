param([string]$BuildRoot='.',[Parameter(Mandatory=$true)][string]$ComponentId)
$ErrorActionPreference='Stop'
if($env:GITHUB_SHA -notmatch '^[0-9a-f]{40}$'){throw 'Exact GitHub commit required'}
$stage=Join-Path $PWD 'candidate-stage'
if(Test-Path $stage){throw 'Fresh candidate staging directory required'}
New-Item -ItemType Directory -Path "$stage/x64" | Out-Null
Copy-Item "$BuildRoot/build/$ComponentId.dll" "$stage/$ComponentId.dll"
Copy-Item "$BuildRoot/build/x64/$ComponentId.dll" "$stage/x64/$ComponentId.dll"
New-Item -ItemType Directory -Path candidate | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$package=Join-Path $PWD "candidate/$ComponentId.fb2k-component"
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$package)
$proof=& "$PSScriptRoot/verify-fb2k-component.ps1" -ComponentPath $package -DllName "$ComponentId.dll"
$hash=(Get-FileHash $package -Algorithm SHA256).Hash.ToLowerInvariant()
@{commit=$env:GITHUB_SHA;run_id=$env:GITHUB_RUN_ID;component="$ComponentId.fb2k-component";sha256=$hash;architectures=@('Win32','x64');runtime_acceptance='PENDING';release_authorized=$false;proof=$proof} | ConvertTo-Json -Depth 8 | Set-Content candidate/manifest.json
"$hash  $ComponentId.fb2k-component" | Set-Content candidate/SHA256SUMS.txt
