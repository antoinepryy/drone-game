#include <Arduino.h>
#include "Config.h"   // pour PIN_RADAR_RX / TX et USE_RADAR
#include <stdio.h>
#include <string.h>

// ===============================
// FORMAT TRAME LD2460
// F4 F3 F2 F1 | type | len (u16 LE, trame complète) | données | F8 F7 F6 F5
// ===============================
static const uint8_t FRAME_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t FRAME_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
static const uint16_t FRAME_HEADER_LEN = 7;   // header + type + len
static const uint16_t FRAME_MIN_LEN    = FRAME_HEADER_LEN + sizeof(FRAME_FOOTER);
static const uint8_t  FRAME_TYPE_TARGETS = 0x04;

// Attente max du port série USB au démarrage (la carte doit tourner sans PC)
static const unsigned long SERIAL_WAIT_TIMEOUT_MS = 2000UL;

// Silence max entre deux octets d'une même trame. Une trame de 128 octets passe en
// ~11 ms à 115200 bauds, les octets se suivent donc à moins de 1 ms.
static const unsigned long RADAR_BYTE_TIMEOUT_MS = 50UL;

// Déclaré avant toute fonction : arduino-cli insère ses prototypes générés
// avant la première fonction, ils doivent déjà connaître ce type.
enum ParseResult { PARSE_MORE, PARSE_DONE, PARSE_DROPPED };

void processFrame(uint8_t* buf, uint16_t len);

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
// SORTIE SÉRIE
// ===============================
// Pire cas : (128 - 11) / 4 = 29 cibles de ~40 octets, plus en-tête et min/max
char outBuf[1536];

// Tampon de réception radar : absorbe les trames reçues pendant l'affichage
static const size_t RADAR_RX_BUFFER_SIZE = 512;

// Décimètres -> texte en mètres, au même format que print(float, 2) : -1 -> "-0.10", 25 -> "2.50"
static const size_t DM_TEXT_LEN = 12;
void formatDm(int16_t dm, char* out) {
    int v = dm;
    const char* sign = v < 0 ? "-" : "";
    if (v < 0) v = -v;
    snprintf(out, DM_TEXT_LEN, "%s%d.%d0", sign, v / 10, v % 10);
}

// ===============================
// SETUP
// ===============================
void setup() {

    Serial.begin(115200);
    unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < SERIAL_WAIT_TIMEOUT_MS) {}

    Serial.println("\n=== LD2460 X / Y CALIBRATION ===");

#if USE_RADAR
    radarSerial.setRxBufferSize(RADAR_RX_BUFFER_SIZE);   // avant begin()
    radarSerial.begin(115200, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
    Serial.println("[RADAR] UART started");
#else
    Serial.println("[RADAR] USE_RADAR disabled");
#endif
}

// ===============================
// RÉCEPTION TRAME
// ===============================
// Ajoute un octet à la trame en cours. Sur PARSE_DROPPED, frame[0..frameIndex)
// contient encore les octets rejetés : l'appelant les rejoue avant de remettre à zéro.
ParseResult parseByte(uint8_t b) {

    // Synchronisation header F4 F3 F2 F1.
    // Sur erreur, un F4 peut être le début du vrai header : on repart de lui.
    if (frameIndex < sizeof(FRAME_HEADER)) {
        if (b == FRAME_HEADER[frameIndex]) {
            frame[frameIndex++] = b;
        } else if (b == FRAME_HEADER[0]) {
            frame[0] = b;
            frameIndex = 1;
        } else {
            frameIndex = 0;
        }
        return PARSE_MORE;
    }

    frame[frameIndex++] = b;

    // Longueur connue à partir des octets 5/6
    if (frameIndex < FRAME_HEADER_LEN) return PARSE_MORE;

    uint16_t len = frame[5] | (frame[6] << 8);

    // Longueur impossible : bruit sur la ligne
    if (len < FRAME_MIN_LEN || len > sizeof(frame)) {
#if DEBUG_SERIAL
        Serial.print("[RADAR] invalid length dropped: ");
        Serial.println((int)len);
#endif
        return PARSE_DROPPED;
    }

    // Trame de cibles : 4 octets par cible. Rejet dès la longueur connue, sans
    // attendre la fin de trame qui avalerait les octets de la trame suivante.
    if (frame[4] == FRAME_TYPE_TARGETS && (len - FRAME_MIN_LEN) % 4 != 0) {
#if DEBUG_SERIAL
        Serial.print("[RADAR] invalid data size dropped: ");
        Serial.println((int)(len - FRAME_MIN_LEN));
#endif
        return PARSE_DROPPED;
    }

    if (frameIndex < len) return PARSE_MORE;

    // Trame complète : on ne la traite que si le footer est intact
    if (memcmp(&frame[len - sizeof(FRAME_FOOTER)], FRAME_FOOTER, sizeof(FRAME_FOOTER)) != 0) {
#if DEBUG_SERIAL
        Serial.println("[RADAR] bad footer, frame dropped");
#endif
        return PARSE_DROPPED;
    }

    processFrame(frame, len);
    frameIndex = 0;
    return PARSE_DONE;
}

// Octets d'une trame rejetée, à analyser de nouveau
static uint8_t replayBuf[sizeof(frame)];

// Une trame rejetée peut contenir le header de la suivante (trame tronquée,
// longueur fausse mais plausible) : on rejoue ses octets à partir du deuxième.
// Itératif et borné : à chaque rejet, l'octet de tête est abandonné, donc la file
// raccourcit et ne dépasse jamais sizeof(frame) - 1 octets.
void receiveRadarByte(uint8_t b) {
    uint16_t replayLen = 0, replayPos = 0;
    for (;;) {
        if (parseByte(b) == PARSE_DROPPED) {
            uint16_t keep = frameIndex - 1;
            uint16_t left = replayLen - replayPos;
            memmove(replayBuf + keep, replayBuf + replayPos, left);
            memcpy(replayBuf, frame + 1, keep);
            replayLen = keep + left;
            replayPos = 0;
            frameIndex = 0;
        }
        if (replayPos >= replayLen) return;
        b = replayBuf[replayPos++];
    }
}

// ===============================
// LOOP
// ===============================
void loop() {

#if USE_RADAR
    static unsigned long lastByteMs = 0;

    // Trame commencée puis plus rien : le radar s'est arrêté au milieu d'une trame.
    // Contrôlé seulement quand le tampon UART est vide, sinon un affichage lent
    // ferait expirer une trame dont les octets attendent déjà dans le tampon.
    if (frameIndex > 0 && !radarSerial.available()
            && millis() - lastByteMs > RADAR_BYTE_TIMEOUT_MS) {
#if DEBUG_SERIAL
        Serial.print("[RADAR] frame timeout, dropped after ");
        Serial.print((int)frameIndex);
        Serial.println(" bytes");
#endif
        frameIndex = 0;
    }

    while (radarSerial.available()) {
        receiveRadarByte(radarSerial.read());
        lastByteMs = millis();
    }
#endif
}

// ===============================
// TRAITEMENT TRAME
// ===============================
void processFrame(uint8_t* buf, uint16_t len) {

    // 0x04 = targets
    if (buf[4] != FRAME_TYPE_TARGETS) return;

    // len est garanti >= FRAME_MIN_LEN et <= sizeof(frame) par parseByte()
    uint16_t dataStart = FRAME_HEADER_LEN;
    uint16_t dataEnd   = len - sizeof(FRAME_FOOTER);

    // 4 octets par cible : un reste signifie une trame corrompue. Déjà rejeté par
    // parseByte() en réception ; garde-fou pour un appel direct.
    if ((dataEnd - dataStart) % 4 != 0) {
#if DEBUG_SERIAL
        Serial.print("[RADAR] invalid data size dropped: ");
        Serial.println((int)(dataEnd - dataStart));
#endif
        return;
    }
    uint16_t targetCount = 0;

    // Tout en décimètres entiers : l'ESP32-C3 n'a pas d'unité flottante
    int16_t yMin = INT16_MAX;
    int16_t yMax = INT16_MIN;

    // Sortie de la trame construite en mémoire puis envoyée en une écriture
    size_t n = 0;
    n += snprintf(outBuf + n, sizeof(outBuf) - n, "\n--- TARGETS ---\n");

    for (uint16_t i = dataStart; i + 3 < dataEnd; i += 4) {

        // ⚠️ IMPORTANT : signed 16 bits
        int16_t x_dm = buf[i]     | (buf[i + 1] << 8);
        int16_t y_dm = buf[i + 2] | (buf[i + 3] << 8);

        char x_txt[DM_TEXT_LEN], y_txt[DM_TEXT_LEN];
        formatDm(x_dm, x_txt);
        formatDm(y_dm, y_txt);
        n += snprintf(outBuf + n, sizeof(outBuf) - n, "Target → X=%s m | Y=%s m\n", x_txt, y_txt);

        if (y_dm < yMin) yMin = y_dm;
        if (y_dm > yMax) yMax = y_dm;
        targetCount++;
    }

    if (targetCount == 0) {
        n += snprintf(outBuf + n, sizeof(outBuf) - n, "No target\n");
    } else {
        char min_txt[DM_TEXT_LEN], max_txt[DM_TEXT_LEN];
        formatDm(yMin, min_txt);
        formatDm(yMax, max_txt);
        n += snprintf(outBuf + n, sizeof(outBuf) - n, "Y min = %s m | Y max = %s m\n", min_txt, max_txt);
    }

    Serial.write((const uint8_t*)outBuf, n);
}
