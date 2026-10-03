Set-Location C:\Users\USER\Projects\running-audio
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" runs\pr1-dart-layered-20261003\gate.py build B-rx B-tx-150us Bf29-rx Bf29-tx-150us Bf20-rx Bf20-tx-150us *> runs\pr1-dart-layered-20261003\build_diag4.log
"exit=$LASTEXITCODE" | Out-File -Append -Encoding utf8 runs\pr1-dart-layered-20261003\build_diag4.log
