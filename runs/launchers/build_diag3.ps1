Set-Location C:\Users\USER\Projects\running-audio
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" runs\pr1-dart-layered-20261003\gate.py build Bt-tx-150us *> runs\pr1-dart-layered-20261003\build_diag3.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dart-layered-20261003\build_diag3.log
