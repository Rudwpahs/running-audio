Set-Location C:\Users\USER\Projects\running-audio
$env:PYTHONUNBUFFERED = "1"
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -u runs\pr1-auto-20260930\suite.py step3 3 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-auto-20260930\step3.log
"exit=$LASTEXITCODE $(Get-Date -Format o)" | Out-File -Append -Encoding utf8 runs\pr1-auto-20260930\step3.log
