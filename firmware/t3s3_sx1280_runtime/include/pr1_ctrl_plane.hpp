#pragma once

// Gate C1 (issue #52): bench control plane isolated from the radio core.
// The USB/serial side (core 0) parses host lines into In records and formats
// Out records into text; the radio loop (core 1) only exchanges fixed-size
// records through single-producer/single-consumer queues. No parsing, printf
// or Serial call happens on the radio core while a run is measured.
// The wire text is unchanged from Gate C, so the host relay and analysis stay the same.
//
// Parsing and formatting are self-contained (no snprintf/strtoull/strstr) and placed in
// IRAM (see pr1_placement.hpp): newlib's printf core alone is ~12 KB of flash code and
// running it evicts most of the shared 16 KB instruction cache.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../common/pr1_placement.hpp"

namespace pr1::runtime::ctrl {

enum class InType : std::uint8_t { None = 0, Ack, Map, Probe, Commit, Abort };

// Host -> board (two-phase). Fields not used by a type stay zero.
struct In {
  InType type = InType::None;
  std::uint32_t id = 0;       // same 32-bit widths as the Gate C reader
  std::uint32_t ok = 0;       // ACK
  std::uint32_t version = 0;  // MAP v / PRB v
  std::uint32_t old_version = 0;
  std::uint8_t channel = 0;   // PRB ch
  std::uint64_t bits = 0;
  std::uint64_t old_bits = 0;
  std::uint64_t at = 0;       // MAP act / PRB at
};

enum class OutType : std::uint8_t {
  PropMap = 0,
  PropProbe,
  Commit,          // PR1COMMIT id ok tx_ok in_time rx_logical
  CommitAbandon,   // PR1COMMIT ok=0 ... reason=<r> rx_logical
  CommitUnknown,   // PR1COMMIT ok=0 tx_ok in_time=0 reason=unknown_id
  Ack,             // PR1ACK id ok reason tx_logical
  Staged,          // PR1STAGED id ok reason tx_logical
  Aborted,         // PR1ABORTED id had
};

// Board -> host. Captured on the radio core at event time (logical included),
// formatted later on the control core.
struct Out {
  OutType type = OutType::PropMap;
  std::uint32_t id = 0;
  std::uint16_t version = 0;
  std::uint16_t old_version = 0;
  std::uint8_t channel = 0;
  std::uint8_t ok = 0;
  std::uint32_t tx_ok = 0;
  std::uint8_t in_time = 0;
  std::uint8_t active = 0;
  const char* reason = "";  // always a string literal
  std::uint64_t bits = 0;
  std::uint64_t old_bits = 0;
  std::uint64_t at = 0;
  std::uint64_t logical = 0;
};

// Lock-free SPSC ring (one producer core, one consumer core).
template <typename T, std::size_t N>
class Spsc {
 public:
  PR1_IRAM bool push(const T& v) {
    const std::uint32_t h = head_.load(std::memory_order_relaxed);
    if (h - tail_.load(std::memory_order_acquire) >= N) return false;
    items_[h % N] = v;
    head_.store(h + 1U, std::memory_order_release);
    return true;
  }
  PR1_IRAM bool empty() const {
    return tail_.load(std::memory_order_relaxed) == head_.load(std::memory_order_acquire);
  }
  PR1_IRAM bool pop(T* out) {
    const std::uint32_t t = tail_.load(std::memory_order_relaxed);
    if (t == head_.load(std::memory_order_acquire)) return false;
    *out = items_[t % N];
    tail_.store(t + 1U, std::memory_order_release);
    return true;
  }

 private:
  std::array<T, N> items_{};
  std::atomic<std::uint32_t> head_{0};
  std::atomic<std::uint32_t> tail_{0};
};

PR1_IRAM inline bool startsAt(const char* p, const char* word) {
  while (*word != '\0') {
    if (*p++ != *word++) return false;
  }
  return true;
}

PR1_IRAM inline int digitValue(char c, int base) {
  int v = -1;
  if (c >= '0' && c <= '9') v = c - '0';
  else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
  else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
  return v >= 0 && v < base ? v : -1;
}

// Same "key=value" rules as the Gate C reader: the key must start the line or follow a
// space and be followed by '='; the first such occurrence wins. Value: optional leading
// blanks, optional 0x for base 16, at least one digit (as strtoull for host-made lines).
PR1_IRAM inline bool kv(const char* line, const char* key, int base, unsigned long long* out) {
  for (const char* p = line; *p != '\0'; ++p) {
    if ((p != line && p[-1] != ' ') || !startsAt(p, key)) continue;
    const char* q = p;
    for (const char* k = key; *k != '\0'; ++k) ++q;
    if (*q != '=') continue;
    ++q;
    while (*q == ' ' || *q == '\t') ++q;
    if (base == 16 && q[0] == '0' && (q[1] == 'x' || q[1] == 'X') && digitValue(q[2], 16) >= 0) q += 2;
    unsigned long long v = 0;
    int n = 0;
    for (int d; (d = digitValue(*q, base)) >= 0; ++q, ++n) v = v * static_cast<unsigned>(base) + static_cast<unsigned>(d);
    if (n == 0) return false;
    *out = v;
    return true;
  }
  return false;
}

PR1_IRAM inline bool starts(const char* line, const char* word) {
  const char* q = line;
  while (*word != '\0') {
    if (*q++ != *word++) return false;
  }
  return *q == ' ';
}

// Returns false for anything that Gate C ignored (unknown verb or missing field).
PR1_IRAM inline bool parse(const char* line, In* m) {
  unsigned long long id = 0, a = 0, b = 0, c = 0, d = 0, e = 0;
  *m = In{};
  if (!kv(line, "id", 10, &id)) return false;
  m->id = static_cast<std::uint32_t>(id);
  if (starts(line, "ACK")) {
    if (!kv(line, "ok", 10, &a)) return false;
    m->type = InType::Ack;
    m->ok = static_cast<std::uint32_t>(a);
  } else if (starts(line, "MAP")) {
    if (!kv(line, "v", 10, &a) || !kv(line, "old", 10, &b) || !kv(line, "oldbits", 16, &c) ||
        !kv(line, "bits", 16, &d) || !kv(line, "act", 10, &e)) {
      return false;
    }
    m->type = InType::Map;
    m->version = static_cast<std::uint32_t>(a);
    m->old_version = static_cast<std::uint32_t>(b);
    m->old_bits = c;
    m->bits = d;
    m->at = e;
  } else if (starts(line, "PRB")) {
    if (!kv(line, "ch", 10, &a) || !kv(line, "at", 10, &b) || !kv(line, "v", 10, &c)) return false;
    m->type = InType::Probe;
    // Out-of-range channels must still be rejected by the TX ("late"), not wrapped.
    m->channel = a > 255ULL ? 255U : static_cast<std::uint8_t>(a);
    m->at = b;
    m->version = static_cast<std::uint32_t>(c);
  } else if (starts(line, "COMMIT")) {
    m->type = InType::Commit;
  } else if (starts(line, "ABORT")) {
    m->type = InType::Abort;
  } else {
    return false;
  }
  return true;
}

// Minimal bounded text writer (replaces snprintf on the control core).
struct Writer {
  char* p;
  char* end;  // last usable byte is end - 1 (kept for the terminator)
  bool ok = true;
  PR1_IRAM void ch(char c) {
    if (p + 1 < end) *p++ = c;
    else ok = false;
  }
  PR1_IRAM void str(const char* s) {
    while (*s != '\0') ch(*s++);
  }
  PR1_IRAM void dec(unsigned long long v) {
    char tmp[20];
    int n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + v % 10U);
      v /= 10U;
    } while (v != 0U);
    while (n > 0) ch(tmp[--n]);
  }
  // Lower-case hex, zero-padded to at least `width` digits (as %0<width>llx).
  PR1_IRAM void hex(unsigned long long v, int width) {
    char tmp[16];
    int n = 0;
    do {
      const unsigned d = static_cast<unsigned>(v & 0xFU);
      tmp[n++] = static_cast<char>(d < 10U ? '0' + d : 'a' + d - 10U);
      v >>= 4U;
    } while (v != 0U);
    for (int i = n; i < width; ++i) ch('0');
    while (n > 0) ch(tmp[--n]);
  }
};

// Byte-identical to the Gate C lines. Returns the length, or -1 if it did not fit.
PR1_IRAM inline int format(const Out& o, char* buf, std::size_t n) {
  if (n == 0U) return -1;
  Writer w{buf, buf + n};
  switch (o.type) {
    case OutType::PropMap:
      w.str("PR1PROP id="); w.dec(o.id); w.str(" type=map v="); w.dec(o.version);
      w.str(" old="); w.dec(o.old_version); w.str(" bits="); w.hex(o.bits, 10);
      w.str(" oldbits="); w.hex(o.old_bits, 10); w.str(" act="); w.dec(o.at);
      w.str(" rx_logical="); w.dec(o.logical); w.str(" active="); w.dec(o.active);
      break;
    case OutType::PropProbe:
      w.str("PR1PROP id="); w.dec(o.id); w.str(" type=probe ch="); w.dec(o.channel);
      w.str(" at="); w.dec(o.at); w.str(" v="); w.dec(o.version); w.str(" rx_logical="); w.dec(o.logical);
      break;
    case OutType::Commit:
      w.str("PR1COMMIT id="); w.dec(o.id); w.str(" ok="); w.dec(o.ok); w.str(" tx_ok="); w.dec(o.tx_ok);
      w.str(" in_time="); w.dec(o.in_time); w.str(" rx_logical="); w.dec(o.logical);
      break;
    case OutType::CommitAbandon:
      w.str("PR1COMMIT id="); w.dec(o.id); w.str(" ok=0 tx_ok=0 in_time=0 reason="); w.str(o.reason);
      w.str(" rx_logical="); w.dec(o.logical);
      break;
    case OutType::CommitUnknown:
      w.str("PR1COMMIT id="); w.dec(o.id); w.str(" ok=0 tx_ok="); w.dec(o.tx_ok);
      w.str(" in_time=0 reason=unknown_id");
      break;
    case OutType::Ack:
    case OutType::Staged:
      w.str(o.type == OutType::Ack ? "PR1ACK id=" : "PR1STAGED id="); w.dec(o.id); w.str(" ok="); w.dec(o.ok);
      w.str(" reason="); w.str(o.reason); w.str(" tx_logical="); w.dec(o.logical);
      break;
    case OutType::Aborted:
      w.str("PR1ABORTED id="); w.dec(o.id); w.str(" had="); w.dec(o.ok);
      break;
  }
  *w.p = '\0';
  return w.ok ? static_cast<int>(w.p - buf) : -1;
}

}  // namespace pr1::runtime::ctrl
