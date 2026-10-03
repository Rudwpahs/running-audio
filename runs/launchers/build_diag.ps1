Set-Location C:\Users\USER\Projects\running-audio
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" runs\pr1-dart-layered-20261003\gate.py build B-tx-300us Bm-rx *> runs\pr1-dart-layered-20261003\build_diag.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dart-layered-20261003\build_diag.log
