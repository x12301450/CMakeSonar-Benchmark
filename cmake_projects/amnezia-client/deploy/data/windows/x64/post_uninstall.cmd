set AmneziaPath=%~dp0
echo %AmneziaPath%

"%AmneziaPath%\AmneziaVPN.exe" -c
timeout /t 1
sc stop AmneziaVPN-service
sc delete AmneziaVPN-service
sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "AmneziaVPN-service.exe" /F
taskkill /IM "AmneziaVPN.exe" /F
exit /b 0
