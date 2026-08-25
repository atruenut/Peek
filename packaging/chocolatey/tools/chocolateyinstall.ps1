$ErrorActionPreference = 'Stop'

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'Peek requires 64-bit Windows.'
}

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$exePath = Join-Path $toolsDir 'Peek.exe'

$packageArgs = @{
    packageName    = $env:ChocolateyPackageName
    fileFullPath  = $exePath
    url64bit      = 'https://github.com/HaBaDEDE/Peek/releases/download/v0.1.0/Peek.exe'
    checksum64    = '9A93D452897B121307490FD905BCDBCE842096041A74C49B449E2AB1754BF783'
    checksumType64 = 'sha256'
}

Get-ChocolateyWebFile @packageArgs

$shortcutPath = Join-Path ([Environment]::GetFolderPath('CommonStartMenu')) 'Programs\Peek.lnk'
Install-ChocolateyShortcut `
    -ShortcutFilePath $shortcutPath `
    -TargetPath $exePath `
    -WorkingDirectory $toolsDir `
    -IconLocation $exePath
