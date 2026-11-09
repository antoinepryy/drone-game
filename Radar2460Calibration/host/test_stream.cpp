// Golden test bench: replaces radar_sim.cpp and feeds the sketch scripted byte
// sequences on a virtual clock, so every run prints exactly the same output.
// Compared against host/golden.txt by host/run_golden.sh.
//
// Each case prints "## <name>" before its bytes are delivered, then the sketch's
// own output follows. Chunks of a case are released at their time offset; while
// nothing is due, available() returns 0 and the clock advances 1 ms per call,
// like loop() spinning on an idle UART.
#include "Arduino.h"

#include <cstdlib>
#include <string>
#include <vector>

HardwareSerial Serial(0);

namespace {
using Bytes = std::vector<uint8_t>;

unsigned long nowMs = 0;

void put16(Bytes& f, int v) {
    f.push_back(static_cast<uint8_t>(v & 0xFF));
    f.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

// Targets frame; lenOverride != 0 writes a wrong length field.
Bytes frame(std::vector<std::pair<int, int>> targets, int lenOverride = 0) {
    Bytes f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0, 0};
    for (auto& t : targets) { put16(f, t.first); put16(f, t.second); }
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    int len = lenOverride ? lenOverride : static_cast<int>(f.size());
    f[5] = len & 0xFF;
    f[6] = (len >> 8) & 0xFF;
    return f;
}

Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

Bytes head(const Bytes& b, size_t n) { return Bytes(b.begin(), b.begin() + n); }

struct Chunk { unsigned long atMs; Bytes bytes; };
struct Case { std::string name; std::vector<Chunk> chunks; };

std::vector<Case> cases() {
    Bytes one = frame({{18, 37}});
    Bytes two = frame({{-6, 35}, {12, 50}});
    return {
        {"valid frame, two targets", {{0, two}}},
        {"valid frame, no target", {{0, frame({})}}},
        {"length field = 2, then valid frame", {{0, cat({{0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00}, one})}}},
        {"length field = 32767, then valid frame", {{0, cat({{0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0xFF, 0x7F}, one})}}},
        {"stray F4 before header", {{0, cat({{0xF4}, one})}}},
        {"garbage then valid frame", {{0, cat({{0x00, 0xF4, 0xF3, 0x11, 0xF4, 0xF3, 0xF2, 0x00}, one})}}},
        {"corrupted footer, then valid frame", {{0, [&] { Bytes b = one; b.back() = 0; return cat({b, one}); }()}}},
        // A frame cut short is immediately followed by the next one: the length read
        // from the truncated frame spans into the next frame's header.
        {"truncated frame, then valid frame", {{0, cat({head(frame({{1, 20}, {2, 30}, {3, 40}}), 15), one})}}},
        // Plausible but wrong length (40): reading it swallows the next two frames.
        {"wrong length 40, then two valid frames", {{0, cat({frame({{5, 25}}, 40), one, two})}}},
        {"target data not a multiple of 4 bytes", {{0, [] {
            Bytes f = frame({{18, 37}});
            f.insert(f.begin() + 11, {0xAA, 0xBB});  // 2 extra data bytes
            f[5] = static_cast<uint8_t>(f.size());
            return cat({f, frame({{18, 37}})});
        }()}}},
        // Radar stops in the middle of a frame, then restarts long after.
        {"frame cut, radar silent, then valid frame", {{0, head(two, 12)}, {500, one}}},
        // Slow line: bytes of a valid frame arrive 10 ms apart, must not time out.
        {"slow but continuous frame", [&] {
            std::vector<Chunk> c;
            for (size_t i = 0; i < one.size(); ++i) c.push_back({10 * i, {one[i]}});
            return c;
        }()},
    };
}

std::vector<Case> all = cases();
size_t caseIdx = 0, chunkIdx = 0, bytePos = 0;
unsigned long caseStartMs = 0;
bool caseAnnounced = false;

// Gap between cases, larger than any byte timeout so cases do not interact.
const unsigned long CASE_GAP_MS = 1000;
} // namespace

unsigned long millis() { return nowMs; }

void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}

int HardwareSerial::available() {
    if (uart_ == 0) return 0;
    for (;;) {
        if (caseIdx >= all.size()) {
            if (nowMs > caseStartMs + CASE_GAP_MS) { std::fflush(stdout); std::exit(0); }
            ++nowMs;
            return 0;
        }
        Case& c = all[caseIdx];
        if (!caseAnnounced) {
            std::fflush(stdout);
            std::printf("\n## %s\n", c.name.c_str());
            caseAnnounced = true;
        }
        if (chunkIdx >= c.chunks.size()) {
            if (nowMs < caseStartMs + c.chunks.back().atMs + CASE_GAP_MS) { ++nowMs; return 0; }
            ++caseIdx; chunkIdx = 0; bytePos = 0; caseAnnounced = false;
            caseStartMs = nowMs;
            continue;
        }
        Chunk& ch = c.chunks[chunkIdx];
        if (nowMs < caseStartMs + ch.atMs) { ++nowMs; return 0; }
        if (bytePos >= ch.bytes.size()) { ++chunkIdx; bytePos = 0; continue; }
        return static_cast<int>(ch.bytes.size() - bytePos);
    }
}

int HardwareSerial::read() {
    if (uart_ == 0 || !available()) return -1;
    return all[caseIdx].chunks[chunkIdx].bytes[bytePos++];
}
