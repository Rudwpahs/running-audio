# PR1 checkpoint bundle reconstruction

The exact checkpoint payload was stored as four Base64 text fragments because the GitHub connector writes UTF-8 text files.

## Files
- `pr1_checkpoint_20260823.tgz.b64.part01`
- `pr1_checkpoint_20260823.tgz.b64.part02`
- `pr1_checkpoint_20260823.tgz.b64.part03`
- `pr1_checkpoint_20260823.tgz.b64.part04`

## Expected archive SHA-256
`722cc774f8c01c545e2f6ff0b578c71114ed2891b480ded4a30b1fe8f9d5ccdc`

## Windows PowerShell reconstruction
```powershell
$parts = 1..4 | ForEach-Object { Get-Content ("pr1_checkpoint_20260823.tgz.b64.part{0:D2}" -f $_) -Raw }
$b64 = ($parts -join "")
[IO.File]::WriteAllBytes("pr1_checkpoint_20260823.tgz", [Convert]::FromBase64String($b64))
Get-FileHash .\pr1_checkpoint_20260823.tgz -Algorithm SHA256
```

## Linux/macOS reconstruction
```bash
cat pr1_checkpoint_20260823.tgz.b64.part01 \
    pr1_checkpoint_20260823.tgz.b64.part02 \
    pr1_checkpoint_20260823.tgz.b64.part03 \
    pr1_checkpoint_20260823.tgz.b64.part04 \
  | base64 -d > pr1_checkpoint_20260823.tgz
sha256sum pr1_checkpoint_20260823.tgz
tar -xzf pr1_checkpoint_20260823.tgz
```

## Bundle contents
- exact current A transmitter source
- exact current B receiver source
- High Sensitivity OFF log
- High Sensitivity ON short log
- High Sensitivity ON long log
- RF benchmark report
- 10,000-packet 225/200 us confirmation log

Do not treat this branch as the production/main branch. It is a recovery checkpoint before the reliability-layer experiments.
