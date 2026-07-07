# Host Protocol Test

From the repository root, compile the shared protocol test with a C11 compiler using warnings as errors. Include `components/pr1_protocol/include`, and compile `components/pr1_protocol/pr1_protocol.c` together with `tests/protocol_host_test.c`.

The expected output is `protocol tests passed`.

This host test checks header encoding and decoding, payload validation, corruption detection, and 32-bit sequence wrap comparison. It does not validate ESP-IDF APIs or hardware behavior.
