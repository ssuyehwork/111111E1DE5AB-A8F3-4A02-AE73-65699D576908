# 杀死旧进程
taskkill /IM ArcMeta.exe /F 2>$null
Start-Sleep -Milliseconds 500

# 启动程序
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = "g:\C++\ArcMeta\ArcMeta\ArcMeta\Release\ArcMeta.exe"
$startInfo.WorkingDirectory = "g:\C++\ArcMeta\ArcMeta\ArcMeta\Release"
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

$process = [System.Diagnostics.Process]::Start($startInfo)
Write-Host "程序已启动，PID: $($process.Id)"
Write-Host "现在请查看屏幕上是否出现了加载窗口..."
Write-Host "（等待 60 秒...）"

# 等待 60 秒
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Seconds 1
    if ($process.HasExited) {
        Write-Host "程序已退出，ExitCode: $($process.ExitCode)"
        break
    }
    Write-Host -NoNewline "."
}

if (-not $process.HasExited) {
    Write-Host "`n程序仍在运行..."
    taskkill /PID $process.Id /F 2>$null
}
