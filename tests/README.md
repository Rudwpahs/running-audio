# Host Protocol Test

From the repository root, compile the shared test with a C11 compiler using warnings as errors. Include `components/pr1_protocol/include`, and compile both `components/pr1_protocol/pr1_protocol.c` and `components/pr1_protocol/pr1_sequence.c` together with `tests/protocol_host_test.c`.

The expected output is `protocol tests passed`.

The test covers header encoding and decoding, payload validation, corruption detection, serial-number comparison, packet gaps, duplicates, out-of-order delivery, and 32-bit sequence wraparound. It does not validate ESP-IDF networking APIs or physical hardware behavior.
