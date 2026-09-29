$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$env:PYTHONUNBUFFERED = "1"
Set-Location C:\Users\USER\Projects\running-audio
python -u runs\pr1-distance\distance_driver.py run 13.6 150 10000 2>&1 | ForEach-Object { "$_" } | Out-File -Encoding utf8 runs\pr1-distance\13.6m\gap-150us\driver.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-distance\13.6m\gap-150us\driver.log
