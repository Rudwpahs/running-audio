# PR1 board sweep summary

Completed runs: 2/11
Parsed runs: 2/11
Missing sweep gaps: 5000, 1000, 500, 300, 250, 225, 200, 175, 150

## Loss transitions
- 125 -> 0 us: loss delta 48.963 percentage points

## 10,000-packet revalidation
- 125 us, 0 us

## Bottleneck candidates
- primary: spi
- candidates: spi
- confidence: medium

## Evidence boundary
These labels are diagnostic hypotheses from measured timing/loss correlation, not causal proof. The frozen RF baseline is unchanged.
Packet count is the RX-observed sequence span (crc_good + missing), not an exact TX-attempt counter.
