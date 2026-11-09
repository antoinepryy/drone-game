// Simulated LD2460 radar: emits "targets" frames in the format the sketch parses.
//   F4 F3 F2 F1 | type 0x04 | len (u16 LE, whole frame) | N x (x_dm i16, y_dm i16) | F8 F7 F6 F5
// Env: SIM_FRAMES (default 10, 0 = endless), SIM_PERIOD_MS (default 300),
//      SIM_TARGETS (default 2, max 3), SIM_FAULTS=1 to interleave malformed frames
//      (see makeFaultyFrame), SIM_SCENARIO=walk for people walking in and out of
//      the field (see walkTargets) instead of the default oscillating targets.
#include "Arduino.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

HardwareSerial Serial(0);

namespace {
std::vector<uint8_t> pending;
size_t pos = 0;
int framesSent = 0;

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return v ? std::atoi(v) : fallback;
}

void put16(std::vector<uint8_t>& f, int16_t v) {
    f.push_back(static_cast<uint8_t>(v & 0xFF));
    f.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

struct Target { int16_t x_dm, y_dm; };

// Default: targets oscillating in front of the radar; X can be negative (left of radar).
std::vector<Target> oscillatingTargets(int n, int targets) {
    std::vector<Target> out;
    for (int t = 0; t < targets; ++t) {
        double phase = n * 0.4 + t * 2.1;
        out.push_back({static_cast<int16_t>(std::lround(25.0 * std::sin(phase))),                    // X: -2.5 .. 2.5 m
                       static_cast<int16_t>(std::lround(30.0 + 15.0 * t + 10.0 * std::cos(phase)))}); // Y: 2 .. 7 m
    }
    return out;
}

// SIM_SCENARIO=walk: a 300-frame cycle (30 s at SIM_PERIOD_MS=100) where people walk
// in straight lines, enter and leave the field. Target count varies, including empty
// frames, and the area close to the radar stays empty for more than 10 s per cycle.
//   #0 crosses left to right at 3 m (frames 0-80)
//   #1 walks slowly right to left at about 6 m (frames 40-260)
//   #2 walks straight towards the radar, then leaves (frames 150-210)
std::vector<Target> walkTargets(int n, int targets) {
    struct Path { int start, end; double x0, y0, x1, y1; };
    static const Path paths[3] = {
        {0, 80, -3.0, 3.0, 3.0, 3.0},
        {40, 260, 2.5, 6.0, -2.5, 6.5},
        {150, 210, 0.5, 7.0, 0.3, 1.5},
    };
    std::vector<Target> out;
    int frame = n % 300;
    for (int t = 0; t < targets; ++t) {
        const Path& p = paths[t];
        if (frame < p.start || frame >= p.end) continue;
        double k = double(frame - p.start) / (p.end - p.start);
        double wobble = 0.05 * std::sin(frame * 0.7 + t);  // measurement noise
        out.push_back({static_cast<int16_t>(std::lround((p.x0 + k * (p.x1 - p.x0) + wobble) * 10)),
                       static_cast<int16_t>(std::lround((p.y0 + k * (p.y1 - p.y0) + wobble) * 10))});
    }
    return out;
}

std::vector<uint8_t> makeFrame(int n, int targets) {
    std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0, 0};
    const char* scenario = std::getenv("SIM_SCENARIO");
    bool walk = scenario && std::strcmp(scenario, "walk") == 0;
    for (const Target& t : walk ? walkTargets(n, targets) : oscillatingTargets(n, targets)) {
        put16(f, t.x_dm);
        put16(f, t.y_dm);
    }
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    uint16_t len = static_cast<uint16_t>(f.size());
    f[5] = len & 0xFF;
    f[6] = len >> 8;
    return f;
}

std::vector<uint8_t> makeValidFrame(int n) {
    int targets = envInt("SIM_TARGETS", 2);
    if (targets < 1) targets = 1;
    if (targets > 3) targets = 3;
    return makeFrame(n, targets);
}

// Cycles through the faults a real UART line produces, each followed by a valid frame.
std::vector<uint8_t> makeFaultyFrame(int n) {
    std::vector<uint8_t> f;
    switch ((n / 2) % 5) {
    case 0:  // length field too small (noise): header + type + len=2
        f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00};
        std::fprintf(stderr, "[SIM] fault: length field = 2\n");
        break;
    case 1:  // length field larger than any real frame
        f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0xFF, 0x7F};
        std::fprintf(stderr, "[SIM] fault: length field = 32767\n");
        break;
    case 2:  // frame with no target
        f = makeFrame(n, 0);
        std::fprintf(stderr, "[SIM] fault: frame without target\n");
        break;
    case 3:  // corrupted footer
        f = makeFrame(n, 1);
        f.back() = 0x00;
        std::fprintf(stderr, "[SIM] fault: corrupted footer\n");
        break;
    default:  // stray 0xF4 right before a valid header
        f = makeFrame(n, 1);
        f.insert(f.begin(), 0xF4);
        std::fprintf(stderr, "[SIM] fault: stray F4 before header (frame is valid)\n");
        break;
    }
    return f;
}
} // namespace

unsigned long millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}

void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}

int HardwareSerial::available() {
    if (uart_ == 0) return 0;
    if (pos >= pending.size()) {
        int maxFrames = envInt("SIM_FRAMES", 10);
        if (maxFrames > 0 && framesSent >= maxFrames) {
            std::fflush(stdout);
            std::exit(0);  // simulation finished
        }
        if (framesSent > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(envInt("SIM_PERIOD_MS", 300)));
        bool faulty = envInt("SIM_FAULTS", 0) && framesSent % 2 == 1;
        pending = faulty ? makeFaultyFrame(framesSent) : makeValidFrame(framesSent);
        ++framesSent;
        pos = 0;
    }
    return static_cast<int>(pending.size() - pos);
}

int HardwareSerial::read() {
    if (uart_ == 0 || !available()) return -1;
    return pending[pos++];
}
