# Download Microsoft installers into deploy/prerequisites and dist/prerequisites.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
# <repo>/deploy/prerequisites -> <repo>
$RepoRoot = Split-Path (Split-Path $Here -Parent) -Parent
$DistPrereq = Join-Path $RepoRoot 'dist\prerequisites'
New-Item -ItemType Directory -Force -Path $Here, $DistPrereq | Out-Null

$Files = @(
    @{ Name = 'vc_redist.x64.exe'; Url = 'https://aka.ms/vs/17/release/vc_redist.x64.exe' }
    @{ Name = 'vs_BuildTools.exe'; Url = 'https://aka.ms/vs/17/release/vs_BuildTools.exe' }
)

foreach ($f in $Files) {
    $dest = Join-Path $Here $f.Name
    Write-Host "Downloading $($f.Name) ..."
    Invoke-WebRequest -Uri $f.Url -OutFile $dest -UseBasicParsing
    Copy-Item -Force $dest (Join-Path $DistPrereq $f.Name)
    $mb = [math]::Round((Get-Item $dest).Length / 1MB, 2)
    Write-Host "  OK ($mb MB)"
}

Copy-Item -Force (Join-Path $Here 'install_build_tools.bat') (Join-Path $DistPrereq 'install_build_tools.bat')
Copy-Item -Force (Join-Path $Here 'README.md') (Join-Path $DistPrereq 'README.md')
Copy-Item -Force (Join-Path $Here 'README.md') (Join-Path $RepoRoot 'dist\README.md')

Write-Host "Done."
Write-Host "  $Here"
Write-Host "  $DistPrereq"
Write-Host "  $(Join-Path $RepoRoot 'dist\README.md')"
