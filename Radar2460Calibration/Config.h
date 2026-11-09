#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// CONFIGURATION GLOBALE DU SYSTÈME
// Carte : ESP32-C3 XIAO
// ============================================================================

// =======================
// DEBUG / SERIAL
// =======================
#define SERIAL_BAUDRATE            115200

// Active ou non les traces série détaillées
#define DEBUG_SERIAL               1

// =======================
// PINS ESP32-C3 XIAO
// =======================

// --- Relais bistable ---
#define PIN_RELAY_A                3
#define PIN_RELAY_B                4

// --- Radar LD2460 (UART) ---
#define PIN_RADAR_RX               20
#define PIN_RADAR_TX               21

// --- RTC (I2C) ---
#define PIN_SDA                    6
#define PIN_SCL                    7

// --- Mesure tension alimentation ---
#define PIN_TENSION                A0

// ---  LEDS         ---
#define PIN_LAT  9
// =======================
// SEUILS TENSION (VOLTS)
// =======================

// Perte d’alimentation détectée
#define POWER_LOSS_THRESHOLD_V     4.5f

// Rétablissement d’alimentation détecté
#define POWER_RESTORE_THRESHOLD_V  4.75f

// Hystérésis minimum de sécurité
static_assert(POWER_RESTORE_THRESHOLD_V > POWER_LOSS_THRESHOLD_V,
              "POWER_RESTORE_THRESHOLD_V must be > POWER_LOSS_THRESHOLD_V");

// =======================
// LAMPE UV — LIMITES DE VIE
// =======================

// Durée de vie maximale (heures)
#define LAMP_MAX_HOURS             9000UL

// Nombre maximal de cycles ON/OFF
#define LAMP_MAX_CYCLES            10000UL

// =======================
// SAUVEGARDES & PERSISTENCE
// =======================

// Sauvegarde NVS tous les X cycles de relais
#define SAVE_CYCLE_INTERVAL        10

// Sauvegarde périodique du temps d’usage (ms)
#define SAVE_TIME_INTERVAL_MS      60000UL   // 1 minute

// =======================
// RADAR — PARAMÈTRES GLOBAUX
// =======================

// Active complètement le radar (compile-time)
#define USE_RADAR                  1

// =======================
// ACQUITTEMENT ERREUR
// =======================

// Temps minimal sans détection radar (ZONE_0) avant sortie MODE_ERROR
#define RADAR_SAFE_DELAY_MS        10000UL   // 10 secondes

// =======================
// SÉCURITÉ / SYSTÈME
// =======================

// Interdit tout rallumage automatique après erreur
#define SAFETY_NO_AUTO_RESTART     1

// Oblige radar ZONE_0 pour sortir de MODE_ERROR
#define SAFETY_RADAR_CLEAR_REQUIRED 1

#endif // CONFIG_H
