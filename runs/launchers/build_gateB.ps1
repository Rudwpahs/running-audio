Set-Location C:\Users\USER\Projects\running-audio
$env:PATH = "$env:USERPROFILE\.platformio\penv\Scripts;$env:PATH"
$R = "C:\Users\USER\Projects\running-audio"
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" runs\pr1-dart-layered-20261003\gate.py build *> runs\pr1-dart-layered-20261003\build_B.log
"gateB_exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dart-layered-20261003\build_B.log
$env:PLATFORMIO_BUILD_DIR = "$R\runs\pr1-dart-layered-20261003\build\afh0-compile"
pio run -d "$R\firmware\t3s3_sx1280_runtime" -e rf_rx_compile -e rf_tx_compile *> runs\pr1-dart-layered-20261003\build_afh0.log
"afh0_exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dart-layered-20261003\build_B.log
