param(
    [double] $bpm = 100.0,
    [double] $seconds = 25.0,
    [int]    $port = 53711
)

$udp = New-Object System.Net.Sockets.UdpClient(0)
$ep = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, $port)

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$step = $bpm / 40.0   # advance one beat per 25 ms tick
$ppq = 100.0

while ($sw.Elapsed.TotalSeconds -lt $seconds) {
    $ppq += $step
    $t = ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) % 2147483647
    $msg = 'DTB1;1;1;' + $bpm.ToString('F6') + ';' + $ppq.ToString('F6') + ';' + (100.0).ToString('F6') + ';' + $t
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($msg)
    [void] $udp.Send($bytes, $bytes.Length, $ep)
    Start-Sleep -Milliseconds 25
}

$udp.Close()
Write-Output ('sent ' + $seconds + 's of beats at ' + $bpm + ' bpm')
