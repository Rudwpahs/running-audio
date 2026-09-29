$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$env:PYTHONUNBUFFERED = "1"
Set-Location C:\Users\USER\Projects\running-audio
$env:PR1_TARGET = "10000"
python -u runs\pr1-dist-1m\diag_driver.py 150 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-dist-1m\driver.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dist-1m\driver.log
