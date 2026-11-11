# Requires the isolated PRODUCT=BitSkimInstallerTest / APP_MUTEX test build.
param([Parameter(Mandatory=$true)][string]$Setup)
$ErrorActionPreference = 'Stop'
$key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\BitSkimInstallerTest'
if (Test-Path $key) { throw 'An installer test registration already exists; leave it untouched.' }
$dir = Join-Path ([IO.Path]::GetTempPath()) ('BitSkimInstaller-' + [guid]::NewGuid().ToString('N'))
$p = Start-Process -FilePath $Setup -ArgumentList ('/S /D=' + $dir) -PassThru -Wait
if ($p.ExitCode -ne 0) { throw "Installation failed: $($p.ExitCode)" }
if (!(Test-Path "$dir\BitSkim.exe") -or !(Test-Path "$dir\Uninstall.exe")) { throw 'Missing installed files' }
if ((Get-ItemProperty $key).DisplayVersion -ne '0.1') { throw 'Wrong registered version' }
$sentinel = Join-Path $dir 'Keep-me.txt'
Set-Content $sentinel 'Preserve unknown files'
$p = Start-Process -FilePath $Setup -ArgumentList ('/S /D=' + $dir) -PassThru -Wait
if ($p.ExitCode -ne 0 -or !(Test-Path $sentinel)) { throw 'Update failed to preserve unknown file' }
$p = Start-Process -FilePath "$dir\Uninstall.exe" -ArgumentList '/S' -PassThru -Wait
for ($i=0; $i -lt 50 -and (Test-Path $key); $i++) { Start-Sleep -Milliseconds 100 }
if ((Test-Path $key) -or (Test-Path "$dir\BitSkim.exe")) { throw 'Uninstall left registration or program behind' }
if (!(Test-Path $sentinel)) { throw 'Uninstaller removed an unknown file' }
$menu = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::Programs,[Environment+SpecialFolderOption]::DoNotVerify)) 'BitSkimInstallerTest'
if (Test-Path $menu) { throw 'Uninstaller left the test Start Menu folder' }
# Only this generated, disposable test directory is cleaned up.
Remove-Item -LiteralPath $dir -Recurse -Force
Write-Output 'PASS: per-user install, 0.1 registration, update, uninstall, shortcuts and unknown-file preservation; live BitSkim untouched.'
