sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "AmneziaVPN-service.exe" /F
taskkill /IM "AmneziaVPN.exe" /F
exit /b 0
