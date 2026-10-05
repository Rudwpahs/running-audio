# TX manual start (commit 5796e13), operator-verified 2026-10-06

- TX image `audio_tx150` (sha256 d0cab04f...a604), boot banner `manual_start=1`, `PR1_RUNTIME_LIVE_READY`.
- RX image `audio_rx` (sha256 2905da5d...6df). RX was silent for ~6 s with the TX running (no autoplay).
- USB char `p` to TX -> clip played twice, then stopped. Operator heard it.
- TX power-cycle (USB unplug/replug) -> no sound. Operator confirmed.
- TX BOOT button (GPIO0), one short press -> clip played twice, then stopped. Operator heard it.
Not measured: RF loss during these plays (audible check only).
