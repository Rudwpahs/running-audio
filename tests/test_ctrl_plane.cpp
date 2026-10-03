#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_ctrl_plane.hpp"

using namespace pr1::runtime::ctrl;

namespace {

std::string fmt(const Out& o) {
  char buf[224];
  const int n = format(o, buf, sizeof(buf));
  assert(n > 0 && n < static_cast<int>(sizeof(buf)));
  return buf;
}

void testParse() {
  In m{};
  assert(parse("ACK id=7 ok=1", &m) && m.type == InType::Ack && m.id == 7 && m.ok == 1);
  assert(!parse("ACK id=7", &m));
  assert(!parse("ACKX id=7 ok=1", &m));
  assert(parse("MAP id=3 v=5 old=4 oldbits=ffffffffff bits=fffffffffe act=12345", &m));
  assert(m.type == InType::Map && m.id == 3 && m.version == 5 && m.old_version == 4);
  assert(m.old_bits == 0xffffffffffULL && m.bits == 0xfffffffffeULL && m.at == 12345U);
  assert(!parse("MAP id=3 v=5 old=4 bits=fffffffffe act=12345", &m));  // oldbits missing
  assert(parse("PRB id=9 ch=17 at=999 v=2", &m) && m.type == InType::Probe && m.channel == 17 && m.at == 999 &&
         m.version == 2);
  assert(parse("PRB id=9 ch=300 at=999 v=2", &m) && m.channel == 255);  // still rejected by TX range check
  assert(parse("COMMIT id=4", &m) && m.type == InType::Commit && m.id == 4);
  assert(parse("ABORT id=4", &m) && m.type == InType::Abort);
  assert(!parse("COMMIT", &m));
  assert(!parse("HELLO id=1", &m));
}

void testFormatMatchesGateC() {
  Out o{};
  o.type = OutType::PropMap;
  o.id = 12; o.version = 6; o.old_version = 5; o.bits = 0xfffffffffeULL; o.old_bits = 0xffffffffffULL;
  o.at = 7000; o.logical = 6400; o.active = 39;
  assert(fmt(o) == "PR1PROP id=12 type=map v=6 old=5 bits=fffffffffe oldbits=ffffffffff act=7000 rx_logical=6400 active=39");
  o = Out{}; o.type = OutType::PropProbe; o.id = 13; o.channel = 4; o.at = 900; o.version = 6; o.logical = 600;
  assert(fmt(o) == "PR1PROP id=13 type=probe ch=4 at=900 v=6 rx_logical=600");
  o = Out{}; o.type = OutType::Commit; o.id = 13; o.ok = 1; o.tx_ok = 1; o.in_time = 1; o.logical = 610;
  assert(fmt(o) == "PR1COMMIT id=13 ok=1 tx_ok=1 in_time=1 rx_logical=610");
  o = Out{}; o.type = OutType::CommitAbandon; o.id = 2; o.reason = "expired"; o.logical = 5;
  assert(fmt(o) == "PR1COMMIT id=2 ok=0 tx_ok=0 in_time=0 reason=expired rx_logical=5");
  o = Out{}; o.type = OutType::CommitUnknown; o.id = 2; o.tx_ok = 1;
  assert(fmt(o) == "PR1COMMIT id=2 ok=0 tx_ok=1 in_time=0 reason=unknown_id");
  o = Out{}; o.type = OutType::Ack; o.id = 2; o.ok = 1; o.reason = "validated"; o.logical = 77;
  assert(fmt(o) == "PR1ACK id=2 ok=1 reason=validated tx_logical=77");
  o.type = OutType::Staged; o.reason = "staged";
  assert(fmt(o) == "PR1STAGED id=2 ok=1 reason=staged tx_logical=77");
  o = Out{}; o.type = OutType::Aborted; o.id = 2; o.ok = 0;
  assert(fmt(o) == "PR1ABORTED id=2 had=0");
}

// Reference: the Gate C snprintf formats (removed from the firmware).
std::string refFormat(const Out& o) {
  char b[256];
  const unsigned id = o.id;
  switch (o.type) {
    case OutType::PropMap:
      std::snprintf(b, sizeof(b), "PR1PROP id=%u type=map v=%u old=%u bits=%010" PRIx64 " oldbits=%010" PRIx64
                    " act=%" PRIu64 " rx_logical=%" PRIu64 " active=%u", id, unsigned(o.version), unsigned(o.old_version),
                    o.bits, o.old_bits, o.at, o.logical, unsigned(o.active));
      break;
    case OutType::PropProbe:
      std::snprintf(b, sizeof(b), "PR1PROP id=%u type=probe ch=%u at=%" PRIu64 " v=%u rx_logical=%" PRIu64, id,
                    unsigned(o.channel), o.at, unsigned(o.version), o.logical);
      break;
    case OutType::Commit:
      std::snprintf(b, sizeof(b), "PR1COMMIT id=%u ok=%u tx_ok=%u in_time=%u rx_logical=%" PRIu64, id, unsigned(o.ok),
                    unsigned(o.tx_ok), unsigned(o.in_time), o.logical);
      break;
    case OutType::CommitAbandon:
      std::snprintf(b, sizeof(b), "PR1COMMIT id=%u ok=0 tx_ok=0 in_time=0 reason=%s rx_logical=%" PRIu64, id, o.reason,
                    o.logical);
      break;
    case OutType::CommitUnknown:
      std::snprintf(b, sizeof(b), "PR1COMMIT id=%u ok=0 tx_ok=%u in_time=0 reason=unknown_id", id, unsigned(o.tx_ok));
      break;
    case OutType::Ack:
      std::snprintf(b, sizeof(b), "PR1ACK id=%u ok=%u reason=%s tx_logical=%" PRIu64, id, unsigned(o.ok), o.reason,
                    o.logical);
      break;
    case OutType::Staged:
      std::snprintf(b, sizeof(b), "PR1STAGED id=%u ok=%u reason=%s tx_logical=%" PRIu64, id, unsigned(o.ok), o.reason,
                    o.logical);
      break;
    case OutType::Aborted:
      std::snprintf(b, sizeof(b), "PR1ABORTED id=%u had=%u", id, unsigned(o.ok));
      break;
  }
  return b;
}

// Reference: the Gate C strstr/strtoull reader.
bool refKv(const char* line, const char* key, int base, unsigned long long* out) {
  const std::size_t klen = std::strlen(key);
  for (const char* p = line; (p = std::strstr(p, key)) != nullptr; p += klen) {
    if ((p == line || p[-1] == ' ') && p[klen] == '=') {
      char* end = nullptr;
      *out = std::strtoull(p + klen + 1, &end, base);
      return end != p + klen + 1;
    }
  }
  return false;
}

void testEquivalenceRandomized() {
  std::uint64_t s = 0x1234567ULL;
  auto rnd = [&]() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return s >> 11; };
  const char* reasons[] = {"validated", "late", "base_mismatch", "expired", "staged", "probe_reserved"};
  for (int i = 0; i < 20000; ++i) {
    Out o{};
    o.type = static_cast<OutType>(rnd() % 8U);
    o.id = static_cast<std::uint32_t>(rnd() % 70000U);
    o.version = static_cast<std::uint16_t>(rnd());
    o.old_version = static_cast<std::uint16_t>(rnd());
    o.channel = static_cast<std::uint8_t>(rnd() % 40U);
    o.ok = static_cast<std::uint8_t>(rnd() % 2U);
    o.tx_ok = static_cast<std::uint32_t>(rnd() % 3U);
    o.in_time = static_cast<std::uint8_t>(rnd() % 2U);
    o.active = static_cast<std::uint8_t>(rnd() % 41U);
    o.reason = reasons[rnd() % 6U];
    o.bits = rnd() & (i % 3 == 0 ? 0xFFULL : 0xFFFFFFFFFFULL);
    o.old_bits = rnd() & 0xFFFFFFFFFFULL;
    o.at = rnd() % (i % 2 ? 100000ULL : 10ULL);
    o.logical = rnd() % 5000000ULL;
    assert(fmt(o) == refFormat(o));
  }
  const char* lines[] = {"MAP id=3 v=5 old=4 oldbits=ffffffffff bits=00fffffffe act=12345",
                         "PRB id=19 ch=7 at=900 v=2", "ACK id=1 ok=1", "ACK id=1 ok=", "COMMIT id=9",
                         "MAP id=3 vv=5 v=6 old=4 oldbits=1 bits=2 act=3", "xid=4 id=5", "MAP id=1 bits=0x1f"};
  const char* keys[] = {"id", "v", "old", "oldbits", "bits", "act", "ch", "at", "ok"};
  for (const char* l : lines) {
    for (const char* k : keys) {
      for (int base : {10, 16}) {
        unsigned long long a = 0, b = 0;
        const bool ra = refKv(l, k, base, &a), rb = kv(l, k, base, &b);
        assert(ra == rb);
        if (ra) assert(a == b);
      }
    }
  }
}

void testSpsc() {
  Spsc<int, 4> q;
  int v = 0;
  assert(!q.pop(&v));
  for (int i = 0; i < 4; ++i) assert(q.push(i));
  assert(!q.push(9));
  for (int i = 0; i < 4; ++i) assert(q.pop(&v) && v == i);
  assert(!q.pop(&v));
}

}  // namespace

int main() {
  testParse();
  testFormatMatchesGateC();
  testEquivalenceRandomized();
  testSpsc();
  std::cout << "test_ctrl_plane OK\n";
  return 0;
}
