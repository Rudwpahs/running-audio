$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$env:PYTHONUNBUFFERED = "1"
Set-Location C:\Users\USER\Projects\running-audio
python -u runs\pr1-diag-10k\diag_driver.py 150 5000 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-diag-10k\driver.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-diag-10k\driver.log
