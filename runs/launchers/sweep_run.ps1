$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$env:PYTHONUNBUFFERED = "1"
Set-Location C:\Users\USER\Projects\running-audio
$R = "C:\Users\USER\Projects\running-audio"
"start $(Get-Date -Format o)" | Out-File -Encoding utf8 sweep_run.log
python -u tools/pr1_experiment_controller.py run "$R\runs\pr1-board-test" "$R\firmware\t3s3_sx1280_runtime" --rx-port COM3 --tx-port COM4 --safe-first 2>&1 | ForEach-Object { "$_" } | Out-File -Append -Encoding utf8 sweep_run.log
"exit=$LASTEXITCODE $(Get-Date -Format o)" | Out-File -Append -Encoding utf8 sweep_run.log
