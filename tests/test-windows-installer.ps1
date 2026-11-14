# Only isolated PRODUCT=BitSkimInstallerTest builds are accepted.
param(
 [Parameter(Mandatory=$true)][string]$OldSetup,
 [Parameter(Mandatory=$true)][string]$Setup,
 [string]$Report = (Join-Path $env:TEMP 'BitSkimInstallerTest.log')
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
 # Elevated sessions do not inherit the user's mapped shared drives.
 $local = Join-Path $env:TEMP ('BitSkimInstallerStage-' + [guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $local | Out-Null
 $localScript = Join-Path $local 'review.ps1'; $localOld = Join-Path $local 'old.exe'; $localSetup = Join-Path $local 'new.exe'; $localReport = Join-Path $local 'report.log'
 Copy-Item -LiteralPath $PSCommandPath -Destination $localScript
 Copy-Item -LiteralPath $OldSetup -Destination $localOld
 Copy-Item -LiteralPath $Setup -Destination $localSetup
 Set-Content -LiteralPath $Report -Value 'Preparing isolated local Windows installer review.' -Encoding UTF8
 try {
  $arguments = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -OldSetup "{1}" -Setup "{2}" -Report "{3}"' -f $localScript,$localOld,$localSetup,$localReport
  $process = Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -Wait -PassThru
  $process.Refresh()
  if (Test-Path $localReport) { Copy-Item -LiteralPath $localReport -Destination $Report -Force }
  else { Add-Content -LiteralPath $Report -Value 'FAIL: elevated runner did not produce a report.' }
  $code = $process.ExitCode
  if ($code -eq 0) { Remove-Item -LiteralPath $local -Recurse -Force -ErrorAction SilentlyContinue }
  exit $code
 } catch { Add-Content -LiteralPath $Report -Value ('FAIL: ' + $_.Exception.Message); exit 1 }
}
$product = 'BitSkimInstallerTest'
$subkey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + $product
function Log([string]$text) { Add-Content -LiteralPath $Report -Value $text -Encoding UTF8; Write-Output $text }
function Check([bool]$ok,[string]$label) { if (!$ok) { throw $label }; Log ('PASS ' + $label) }
function Registration {
 $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
 try {
  $key = $base.OpenSubKey($subkey)
  if (!$key) { return $null }
  try { return @{ Version=$key.GetValue('DisplayVersion'); Location=$key.GetValue('InstallLocation') } } finally { $key.Dispose() }
 } finally { $base.Dispose() }
}
function Run([string]$file,[string]$arguments) {
 $process = Start-Process -FilePath $file -ArgumentList $arguments -PassThru -Wait
 return $process.ExitCode
}
function LiveSettings {
 $result = @{}; $root = Join-Path $env:APPDATA 'BitSkim'
 if (Test-Path $root) { Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object { $result[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash } }
 return $result
}
try {
 Set-Content -LiteralPath $Report -Value 'BitSkim isolated all-users installer lifecycle' -Encoding UTF8
 foreach ($file in @($OldSetup,$Setup)) { Check ((Get-Item -LiteralPath $file).VersionInfo.ProductName -eq $product) 'installer belongs to isolated test product' }
 foreach ($hive in @([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryHive]::CurrentUser)) {
  foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32,[Microsoft.Win32.RegistryView]::Registry64)) {
   $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive,$view)
   try { $key = $base.OpenSubKey($subkey); if ($key) { $key.Dispose(); throw 'Existing test registration found; leaving it untouched.' } } finally { $base.Dispose() }
  }
 }
 $before = LiveSettings
 $work = Join-Path ([IO.Path]::GetTempPath()) ('BitSkimInstaller-' + [guid]::NewGuid().ToString('N'))
 $dir = Join-Path $work 'Install'; New-Item -ItemType Directory -Path $work | Out-Null
 $settings = Join-Path $work 'Profile\preferences.ini'; $received = Join-Path $work 'Downloads\received.txt'
 New-Item -ItemType Directory -Path (Split-Path $settings),(Split-Path $received) | Out-Null
 Set-Content -LiteralPath $settings -Value 'Preserve test preferences'; Set-Content -LiteralPath $received -Value 'Preserve received data'
 $settingsHash = (Get-FileHash $settings).Hash; $receivedHash = (Get-FileHash $received).Hash
 Check ((Run $OldSetup ('/S /D=' + $dir)) -eq 0) 'fresh 0.1 installation'
 $state = Registration; Check ($state -and $state.Version -eq '0.1' -and $state.Location -eq $dir) '0.1 all-users 64-bit registration'
 Check ((Get-Item "$dir\BitSkim.exe").VersionInfo.FileVersion -eq '0.1') 'actual 0.1 application payload'
 $sentinel = Join-Path $dir 'Keep-me.txt'; Set-Content -LiteralPath $sentinel -Value 'Preserve unknown file'
 foreach ($name in @('Local\BitSkimInstallerTest-0.1','Local\BitSkimInstallerTest-0.2')) {
  $mutex = New-Object Threading.Mutex($false,$name)
  try { Check ((Run $Setup ('/S /D=' + $dir)) -ne 0) 'upgrade refuses a running old or current client' } finally { $mutex.Dispose() }
  Check ((Registration).Version -eq '0.1') 'blocked upgrade leaves registration unchanged'
 }
 Check ((Run $Setup ('/S /D=' + $dir)) -eq 0) 'upgrade from 0.1 to 0.2'
 Check ((Registration).Version -eq '0.2') '0.2 all-users registration'
 Check ((Get-Item "$dir\BitSkim.exe").VersionInfo.FileVersion -eq '0.2') 'actual 0.2 application payload'
 Check ((Test-Path $sentinel) -and (Get-FileHash $settings).Hash -eq $settingsHash -and (Get-FileHash $received).Hash -eq $receivedHash) 'upgrade preserves unknown files, settings and received data'
 $menu = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) $product
 $desktop = Join-Path ([Environment]::GetFolderPath('CommonDesktopDirectory')) ($product + '.lnk')
 Check ((Test-Path "$menu\$product.lnk") -and (Test-Path $desktop)) 'all-users Start Menu and selected desktop shortcuts'
 Check ((Run $Setup ('/S /D=' + $dir)) -eq 0) 'repeat 0.2 installation'
 Check ((Run "$dir\Uninstall.exe" '/S') -eq 0) 'uninstall completes'
 for ($i=0; $i -lt 150 -and ((Registration) -or (Test-Path "$dir\BitSkim.exe")); $i++) { Start-Sleep -Milliseconds 100 }
 Check (!(Registration) -and !(Test-Path "$dir\BitSkim.exe")) 'uninstall removes program and registration'
 Check (!(Test-Path $menu) -and !(Test-Path $desktop)) 'uninstall removes only test shortcuts'
 Check ((Test-Path $sentinel) -and (Get-FileHash $settings).Hash -eq $settingsHash -and (Get-FileHash $received).Hash -eq $receivedHash) 'uninstall preserves unknown files, settings and received data'
 $fresh = Join-Path $work 'Fresh02'
 Check ((Run $Setup ('/S /D=' + $fresh)) -eq 0) 'fresh 0.2 installation after uninstall'
 Check ((Registration).Version -eq '0.2' -and (Get-Item "$fresh\BitSkim.exe").VersionInfo.FileVersion -eq '0.2') 'fresh 0.2 payload and registration'
 Check ((Run "$fresh\Uninstall.exe" '/S') -eq 0) 'fresh 0.2 uninstall'
 for ($i=0; $i -lt 150 -and (Registration); $i++) { Start-Sleep -Milliseconds 100 }
 Check (!(Registration) -and !(Test-Path "$fresh\BitSkim.exe")) 'fresh 0.2 uninstall removes registration and executable'
 $after = LiveSettings
 Check ($before.Count -eq $after.Count) 'live BitSkim settings file count unchanged'
 foreach ($name in $before.Keys) { Check ($after.ContainsKey($name) -and $before[$name] -eq $after[$name]) 'live BitSkim settings unchanged' }
 Remove-Item -LiteralPath $work -Recurse -Force
 Log 'PASSED: isolated 0.1 install, 0.2 upgrade, running-client refusal, uninstall and preserved user data.'
 exit 0
} catch { Log ('FAIL ' + $_.Exception.Message); exit 1 }
