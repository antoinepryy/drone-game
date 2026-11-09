// Minimal Arduino shim to build and run the sketch natively (Linux/macOS).
// Serial      -> stdout
// Serial port 1 (radar) -> simulated LD2460 frame stream (see radar_sim.cpp)
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstddef>

#define SERIAL_8N1 0x800001c
#define A0 0

class Print {
public:
    virtual ~Print() = default;
    void print(const char* s)            { std::fputs(s, stdout); }
    void print(float v, int digits = 2)  { std::printf("%.*f", digits, v); }
    void print(int v)                    { std::printf("%d", v); }
    void println()                       { std::fputs("\n", stdout); std::fflush(stdout); }
    template <typename... T> void println(T... args) { print(args...); println(); }
};

class HardwareSerial : public Print {
public:
    explicit HardwareSerial(int uart) : uart_(uart) {}
    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int rx = -1, int tx = -1);
    int available();
    int read();
    explicit operator bool() const { return true; }
private:
    int uart_;
};

extern HardwareSerial Serial;
