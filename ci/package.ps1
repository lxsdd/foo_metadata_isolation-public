param([string]$BuildRoot='.',[Parameter(Mandatory=$true)][string]$ComponentId)
$ErrorActionPreference='Stop'
if($env:GITHUB_SHA -notmatch '^[0-9a-f]{40}$'){throw 'Exact GitHub commit required'}
# The GitHub pull_request checkout normally builds a *synthetic merge commit*.
# Record both that built tree and the true PR-head parent, never present a
# green merge ref as proof that the source branch itself was directly tested.
$checkoutSha=(& git rev-parse --verify HEAD).Trim().ToLowerInvariant()
if($LASTEXITCODE -ne 0 -or $checkoutSha -cne $env:GITHUB_SHA.ToLowerInvariant()) {
  throw 'Checked-out source SHA differs from trusted GitHub event SHA'
}
$prHead=$null
$checkoutKind='direct'
if($env:GITHUB_EVENT_NAME -ceq 'pull_request') {
  $prHead=$env:EXPECTED_PR_HEAD_SHA
  $prBase=$env:EXPECTED_PR_BASE_SHA
  if($prHead -notmatch '^[0-9a-f]{40}$' -or $prBase -notmatch '^[0-9a-f]{40}$') {
    throw 'Pull request head/base SHA unavailable'
  }
  $parents=((& git log -1 --pretty=%P) -split ' ') | Where-Object { $_ }
  if($LASTEXITCODE -ne 0 -or @($parents).Count -ne 2 -or
     $parents[0] -cne $prBase -or $parents[1] -cne $prHead) {
    throw 'GitHub checked-out merge must have the exact PR base and head parents'
  }
  $checkoutKind='pull_request_test_merge'
} else {
  $prHead=$checkoutSha
}

$stage=Join-Path $PWD 'candidate-stage'
if(Test-Path $stage){throw 'Fresh candidate staging directory required'}
New-Item -ItemType Directory -Path "$stage/x64" | Out-Null
Copy-Item "$BuildRoot/build/$ComponentId.dll" "$stage/$ComponentId.dll"
Copy-Item "$BuildRoot/build/x64/$ComponentId.dll" "$stage/x64/$ComponentId.dll"
New-Item -ItemType Directory -Path candidate | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$package=Join-Path $PWD "candidate/$ComponentId.fb2k-component"
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$package)
$proof=& "$PSScriptRoot/verify-fb2k-component.ps1" -ComponentPath $package -DllName "$ComponentId.dll" -ExpectedWin32DllPath "$BuildRoot/build/$ComponentId.dll" -ExpectedX64DllPath "$BuildRoot/build/x64/$ComponentId.dll"
if(-not $proof.PayloadMatchesBuild) {throw 'Built DLL archive digest comparison is mandatory'}
$hash=(Get-FileHash $package -Algorithm SHA256).Hash.ToLowerInvariant()
@{commit=$checkoutSha;built_checkout_sha=$checkoutSha;source_head_sha=$prHead;checkout_kind=$checkoutKind;run_id=$env:GITHUB_RUN_ID;component="$ComponentId.fb2k-component";sha256=$hash;architectures=@('Win32','x64');runtime_acceptance='PENDING';release_authorized=$false;proof=$proof} | ConvertTo-Json -Depth 8 | Set-Content candidate/manifest.json
"$hash  $ComponentId.fb2k-component" | Set-Content candidate/SHA256SUMS.txt
