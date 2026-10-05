# Controlled 2.4 GHz hotspot test — predictions recorded before the runs (2026-10-06)

Interferer: the operator's phone hotspot (2.4 GHz band, with traffic running on a device joined to it). No jammer.
The interferer is called "Wi-Fi-consistent" only. The hotspot channel is recorded if the operator can read it from the phone.
Placement, PHY, power, 150 us gap unchanged. 30000 frames per run (~92 s). Phases (arms alternate B1 / C3n2 in each phase):
  P0 hotspot OFF : B1, C3n2          (tag W0)
  P1 hotspot ON + traffic : B1, C3n2 (tag W1)
  P2 hotspot OFF again : B1, C3n2    (tag W2)
Predictions:
1. W1 raises B1 loss clearly above P0 (>= 2x P0 B1 loss), mostly as CRC errors on a contiguous channel block.
   If it does not, the hotspot is not interfering enough (wrong band/no traffic) and W1 is void, not a C3 result.
2. In W1, C3n2 loss <= 50 % of B1 loss (same target as before); C3n2 excludes mostly channels next to each other.
3. W2 (OFF again): C3n2 returns to its P0 loss within the run's tail; active channels may stay reduced for up to ~5 min
   (replay says recovery up to ~5 min). Falsified if C3n2 W2 loss stays > 2x P0 C3n2.
4. Safety: map equal TX/RX, no in-window mismatch, 0 slow RX post-reads, active >= 12, no reset.
Stop: any safety violation, reset, unexplained sequence jump.

Setup note (operator photos, 2026-10-06): a U+ Wi-Fi 6 home router (2.4 GHz + 5 GHz, always on) stands on the desk about 1 m behind the boards;
it is part of the P0 baseline and of every phase. The phone hotspot (2.4 GHz band requested; channel not read) and a second device streaming
video sit on the desk, left of the boards. Whether the second device was really streaming during W1 is operator-reported, not measured.
W1 restarted after operator started the video late (first attempt aborted, partial archived)
