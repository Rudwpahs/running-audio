# PR1 M0 UDP Packet Protocol

Each UDP datagram is 656 bytes: a 16-byte header followed by a 640-byte payload.

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `PR1A` |
| 4 | 1 | Version | `0x01` |
| 5 | 1 | Flags | M0 `0x01` |
| 6 | 2 | Payload length | big-endian, fixed at 640 |
| 8 | 4 | Sequence | big-endian unsigned integer |
| 12 | 4 | Sample index | big-endian unsigned integer |
| 16 | 640 | Payload | deterministic M0 pattern |

The M0 payload byte at index `i` is `(sequence + i) & 0xff`.

The first sequence and sample index are zero. Sequence increases by one and sample index increases by 320 for every packet. The packet period is 10 ms, giving a nominal rate of 100 packets per second.

Sequence comparison uses 32-bit serial-number arithmetic. The receiver keeps a 64-packet history bitmap so it can distinguish duplicates from late packets that were not previously seen. A late packet does not reduce the accumulated loss counter.
