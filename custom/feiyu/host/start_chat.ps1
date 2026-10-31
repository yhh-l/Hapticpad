param([switch]$PrepareOnly)
$ErrorActionPreference = 'Stop'
$feiyuRoot = Split-Path -Parent $PSScriptRoot
$feiyuWorkspace = Join-Path $feiyuRoot 'workspace'
$feiyuRuntime = Join-Path $feiyuRoot 'runtime'
$feiyuCli = Join-Path $feiyuRuntime 'node_modules\@deepseek-ai\dsh\lib\bin.js'
$feiyuNode = (Get-Command node.exe -ErrorAction Stop).Source
$env:DSH_HOME = Join-Path $env:LOCALAPPDATA 'FeiyuDSH'
$feiyuLogs = Join-Path $env:DSH_HOME 'logs'
[IO.Directory]::CreateDirectory($feiyuLogs) | Out-Null
$feiyuProcessFile = Join-Path $env:DSH_HOME 'feiyu-chat-process.json'
$feiyuRunningChat = $false
if (Test-Path -LiteralPath $feiyuProcessFile) {
    $feiyuPrevious = Get-Content -LiteralPath $feiyuProcessFile -Raw | ConvertFrom-Json
    $feiyuRunning = Get-CimInstance Win32_Process -Filter ('ProcessId = ' + [int]$feiyuPrevious.pid)
    if ($feiyuRunning -and $feiyuRunning.CommandLine.Contains($feiyuCli)) {
        $feiyuRunningChat = $true
    }
}
if (-not (Test-Path -LiteralPath $feiyuCli)) {
    Write-Output 'Installing the pinned DSH runtime and Bigfish plugin...'
    & npm.cmd install --prefix $feiyuRuntime --no-fund --no-audit --fetch-timeout=600000
    if ($LASTEXITCODE -ne 0) { throw 'Runtime installation failed.' }
}
$feiyuPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $feiyuPython)) { $feiyuPython = (Get-Command python.exe -ErrorAction Stop).Source }
$feiyuBridge = Join-Path $PSScriptRoot 'feiyu_bridge.py'
if (-not (Test-Path -LiteralPath (Join-Path $feiyuWorkspace 'identity.json'))) {
    & $feiyuPython -X utf8 $feiyuBridge pull --workspace $feiyuWorkspace
    if ($LASTEXITCODE -ne 0) { throw 'Cannot restore memory from keyboard.' }
}
if (-not $feiyuRunningChat) {
    & $feiyuNode (Join-Path $PSScriptRoot 'prepare_chat.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Chat profile preparation failed.' }
}
if ($PrepareOnly) { return }
$feiyuExistingBridge = @(Get-CimInstance Win32_Process -Filter "Name = 'python.exe'" | Where-Object {
    $_.CommandLine -and $_.CommandLine.Contains($feiyuBridge) -and $_.CommandLine.Contains($feiyuWorkspace) -and $_.CommandLine -match '\bwatch\b'
})
if ($feiyuExistingBridge.Count -eq 0) {
    $feiyuBridgeArgs = '-X utf8 "' + $feiyuBridge + '" watch --workspace "' + $feiyuWorkspace + '"'
    $feiyuBridgeProcess = Start-Process -FilePath $feiyuPython -ArgumentList $feiyuBridgeArgs -WorkingDirectory $feiyuRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $feiyuLogs 'bridge.stdout.log') -RedirectStandardError (Join-Path $feiyuLogs 'bridge.stderr.log') -PassThru
    @{pid=$feiyuBridgeProcess.Id;workspace=$feiyuWorkspace} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $env:DSH_HOME 'feiyu-bridge-process.json') -Encoding utf8
}
if ($feiyuRunningChat) {
    Start-Process -FilePath ('http://127.0.0.1:' + $feiyuPrevious.port + '/')
    Write-Output 'Bigfish chat is already running; keyboard bridge checked.'
    return
}
$feiyuPort = 3087
if (Get-NetTCPConnection -State Listen -LocalPort $feiyuPort -ErrorAction SilentlyContinue) { throw 'Chat port 3087 is already in use.' }
$feiyuArguments = '"' + $feiyuCli + '" web --patch "' + (Join-Path $PSScriptRoot 'feiyu.chat.patch.yml') + '" --host 127.0.0.1 --port ' + $feiyuPort
$feiyuChat = Start-Process -FilePath $feiyuNode -ArgumentList $feiyuArguments -WorkingDirectory $feiyuWorkspace -WindowStyle Hidden -RedirectStandardOutput (Join-Path $feiyuLogs 'chat.stdout.log') -RedirectStandardError (Join-Path $feiyuLogs 'chat.stderr.log') -PassThru
@{pid=$feiyuChat.Id;port=$feiyuPort;workspace=$feiyuWorkspace} | ConvertTo-Json | Set-Content -LiteralPath $feiyuProcessFile -Encoding utf8
Write-Output 'Bigfish chat is starting; the browser will open automatically.'
