#include <Arduino.h>
#include "Config.h"   // pour PIN_RADAR_RX / TX et USE_RADAR
#include <float.h>

// ===============================
// UART RADAR
// ===============================
HardwareSerial radarSerial(1);

// ===============================
// BUFFER TRAME
// ===============================
uint8_t frame[128];
uint16_t frameIndex = 0;

// ===============================
// SETUP
// ===============================
void setup() {

    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("\n=== LD2460 X / Y CALIBRATION ===");

#if USE_RADAR
    radarSerial.begin(115200, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
    Serial.println("[RADAR] UART started");
#else
    Serial.println("[RADAR] USE_RADAR disabled");
#endif
}

// ===============================
// LOOP
// ===============================
void loop() {

#if USE_RADAR
    while (radarSerial.available()) {

        uint8_t b = radarSerial.read();

        // Synchronisation header F4 F3 F2 F1
        if (frameIndex == 0 && b != 0xF4) continue;
        if (frameIndex == 1 && b != 0xF3) { frameIndex = 0; continue; }
        if (frameIndex == 2 && b != 0xF2) { frameIndex = 0; continue; }
        if (frameIndex == 3 && b != 0xF1) { frameIndex = 0; continue; }

        frame[frameIndex++] = b;

        // Longueur connue à partir de l’octet 5/6
        if (frameIndex >= 7) {
            uint16_t len = frame[5] | (frame[6] << 8);

            if (frameIndex >= len) {
                processFrame(frame, len);
                frameIndex = 0;
            }
        }

        if (frameIndex >= sizeof(frame)) {
            frameIndex = 0;
        }
    }
#endif
}

// ===============================
// TRAITEMENT TRAME
// ===============================
void processFrame(uint8_t* buf, uint16_t len) {

    // 0x04 = targets
    if (buf[4] != 0x04) return;

    uint16_t dataStart = 7;
    uint16_t dataEnd   = len - 4;

    float yMin = FLT_MAX;
    float yMax = -FLT_MAX;

    Serial.println("\n--- TARGETS ---");

    for (uint16_t i = dataStart; i + 3 < dataEnd; i += 4) {

        // ⚠️ IMPORTANT : signed 16 bits
        int16_t x_dm = buf[i]     | (buf[i + 1] << 8);
        int16_t y_dm = buf[i + 2] | (buf[i + 3] << 8);

        float x_m = x_dm / 10.0f;
        float y_m = y_dm / 10.0f;

        Serial.print("Target → X=");
        Serial.print(x_m, 2);
        Serial.print(" m | Y=");
        Serial.print(y_m, 2);
        Serial.println(" m");

        if (y_m < yMin) yMin = y_m;
        if (y_m > yMax) yMax = y_m;
    }

    Serial.print("Y min = ");
    Serial.print(yMin, 2);
    Serial.print(" m | Y max = ");
    Serial.print(yMax, 2);
    Serial.println(" m");
}
