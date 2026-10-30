param([string]$Port = '', [string]$DeviceSerial = 'E46468C09732292D')
$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$connection = $null
try {
    while ($null -ne ($line = [Console]::ReadLine())) {
        try {
            $request = $line | ConvertFrom-Json
            if ($request.command -eq 'close') { break }
            if (-not $connection -or -not $connection.IsOpen) {
                $connected = @(Get-CimInstance Win32_PnPEntity -Filter "DeviceID LIKE '%VID_239A&PID_CAFE%'" | Where-Object { $_.Present -and $_.ConfigManagerErrorCode -eq 0 })
                $board = @($connected | Where-Object DeviceID -EQ ('USB\VID_239A&PID_CAFE\' + $DeviceSerial))
                $ports = @($connected | Where-Object PNPClass -EQ 'Ports')
                if ($board.Count -ne 1 -or $ports.Count -ne 1 -or $ports[0].Name -notmatch '\((COM\d+)\)') { throw 'Expected unique keyboard not found' }
                $foundPort = $Matches[1]
                if ($Port -and $Port -ne $foundPort) { throw 'Requested port does not belong to keyboard' }
                $connection = [IO.Ports.SerialPort]::new($foundPort,115200)
                $connection.DtrEnable = $true
                $connection.Encoding = [Text.UTF8Encoding]::new($false)
                $connection.ReadTimeout = 30000
                $connection.WriteTimeout = 5000
                $connection.NewLine = "`n"
                $connection.Open()
                $connection.DiscardInBuffer()
            }
            $command = [string]$request.command
            if ($command.Contains("`n") -or $command.Contains("`r") -or $command.Length -gt 380) { throw 'Invalid serial command' }
            $connection.WriteLine($command)
            $answer = $connection.ReadLine().TrimEnd()
            [Console]::WriteLine((@{ok=$true;response=$answer} | ConvertTo-Json -Compress))
        } catch {
            if ($connection) { $connection.Dispose(); $connection = $null }
            [Console]::WriteLine((@{ok=$false;error=$_.Exception.Message} | ConvertTo-Json -Compress))
        }
        [Console]::Out.Flush()
    }
} finally { if ($connection) { $connection.Dispose() } }
