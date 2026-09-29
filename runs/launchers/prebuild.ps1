$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
Set-Location C:\Users\USER\Projects\running-audio
$R = "C:\Users\USER\Projects\running-audio"
python tools/pr1_experiment_controller.py prepare "$R\runs\pr1-board-test" "$R\firmware\t3s3_sx1280_runtime" --rx-port COM3 --tx-port COM4 --target-packets 1000 --prebuild *> runs_prepare2.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs_prepare2.log
