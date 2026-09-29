$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$env:PYTHONUNBUFFERED = "1"
Set-Location C:\Users\USER\Projects\running-audio
$env:PR1_TARGET = "10000"
python -u runs\pr1-diag-175-10k\diag_driver.py 175 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-diag-175-10k\driver.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-diag-175-10k\driver.log
$env:PR1_TARGET = "100000"
python -u runs\pr1-diag-100k\diag_driver.py 150 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-diag-100k\driver.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-diag-100k\driver.log
