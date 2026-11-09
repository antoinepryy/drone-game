// Shared harness for the native test binaries: replaces the simulated radar with a byte
// stream controlled by the test, and captures what the sketch writes on Serial.
// Include this header, then the sketch: #include "../../Radar2460Calibration.ino"
#pragma once

#include "Arduino.h"

#include <cstdio>
#include <string>
#include <vector>

HardwareSerial Serial(0);

namespace test {
std::vector<uint8_t> stream;   // bytes the radar UART will deliver
size_t streamPos = 0;

void feed(const std::vector<uint8_t>& bytes) {
    stream = bytes;
    streamPos = 0;
}

bool drained() { return streamPos >= stream.size(); }

// Runs fn() with stdout redirected, returns everything written to it.
template <typename Fn>
std::string capture(Fn fn) {
    std::fflush(stdout);
    FILE* cap = std::tmpfile();
    FILE* saved = stdout;
    stdout = cap;
    fn();
    std::fflush(stdout);
    stdout = saved;

    std::rewind(cap);
    std::string out;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), cap)) > 0) out.append(buf, n);
    std::fclose(cap);
    return out;
}

int count(const std::string& haystack, const std::string& needle) {
    int c = 0;
    for (size_t p = haystack.find(needle); p != std::string::npos; p = haystack.find(needle, p + 1)) ++c;
    return c;
}

// Targets frame: F4 F3 F2 F1 | 04 | len | N x (x_dm, y_dm) | F8 F7 F6 F5
// forcedLen >= 0 overrides the length field (the frame keeps its real size).
std::vector<uint8_t> targetsFrame(const std::vector<std::pair<int16_t, int16_t>>& targets, int forcedLen = -1,
                                  uint8_t type = 0x04) {
    std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1, type, 0, 0};
    for (auto [x, y] : targets) {
        f.push_back(static_cast<uint8_t>(x & 0xFF));
        f.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
        f.push_back(static_cast<uint8_t>(y & 0xFF));
        f.push_back(static_cast<uint8_t>((y >> 8) & 0xFF));
    }
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    uint16_t len = forcedLen >= 0 ? static_cast<uint16_t>(forcedLen) : static_cast<uint16_t>(f.size());
    f[5] = len & 0xFF;
    f[6] = len >> 8;
    return f;
}
} // namespace test

unsigned long millis() { return 0; }
void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}
int HardwareSerial::available() {
    return uart_ == 0 ? 0 : static_cast<int>(test::stream.size() - test::streamPos);
}
int HardwareSerial::read() {
    return test::streamPos < test::stream.size() ? test::stream[test::streamPos++] : -1;
}
