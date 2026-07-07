# Claude Review Scope — M0 Draft

Review branch: `agent/m0-firmware-draft`

Focus files:

- `components/pr1_protocol/pr1_protocol.c`
- `m0_tx/main/m0_tx.c`
- `m0_rx/main/m0_rx.c`
- `m0_rx/main/rx_udp.c`

Check only:

1. ESP-IDF v6.0.2 API and compile compatibility
2. Wi-Fi and esp-netif initialization order
3. static IPv4 configuration and reconnect behavior
4. UDP socket lifecycle and errors
5. packet serialization and 32-bit sequence wrap
6. loss, duplicate, and out-of-order accounting
7. FreeRTOS race conditions and memory issues
8. absolute 10 ms transmitter pacing

Do not change the locked hardware, transport, packet size, audio format, or pins. Return findings and minimal diffs only. No ESP-IDF build or hardware test has been run yet.
