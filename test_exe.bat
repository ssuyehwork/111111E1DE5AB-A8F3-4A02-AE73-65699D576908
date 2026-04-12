@echo off
REM Kill old processes
taskkill /IM ArcMeta.exe /F 2>nul
timeout /t 1 /nobreak

REM Start the program
echo Starting ArcMeta.exe...
cd /d "g:\C++\ArcMeta\ArcMeta\ArcMeta\Release"
ArcMeta.exe
