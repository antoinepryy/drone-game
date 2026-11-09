// Tests de réception des trames LD2460.
// Chaque scénario envoie un flux d'octets au croquis (inchangé), puis compte les trames
// effectivement décodées. Lancement : docker build --target tests .
//
// known=true : défaut connu, pas encore corrigé. Le scénario est affiché « ÉCHEC CONNU »
// sans faire échouer la suite. Le jour où il passe, la suite échoue pour qu'on retire le
// drapeau : un défaut corrigé doit devenir un test qui protège.
#include "Arduino.h"

#include <cstdio>
#include <string>
#include <vector>

HardwareSerial Serial(0);

namespace {
std::vector<uint8_t> stream;
size_t streamPos = 0;
}

unsigned long millis() { return 0; }
void HardwareSerial::begin(unsigned long, uint32_t, int, int) {}
int HardwareSerial::available() { return uart_ == 0 ? 0 : static_cast<int>(stream.size() - streamPos); }
int HardwareSerial::read() { return streamPos < stream.size() ? stream[streamPos++] : -1; }

#include "../../Radar2460Calibration.ino"

namespace {

// Trame de cibles : F4 F3 F2 F1 | 04 | len | N x (x, y) | F8 F7 F6 F5
// forcedLen >= 0 remplace le champ longueur (la trame garde sa vraie taille).
std::vector<uint8_t> targetsFrame(int targets, int forcedLen = -1) {
    std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0, 0};
    for (int t = 0; t < targets; ++t) f.insert(f.end(), {static_cast<uint8_t>(10 + t), 0, 30, 0});
    f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
    uint16_t len = forcedLen >= 0 ? static_cast<uint16_t>(forcedLen) : static_cast<uint16_t>(f.size());
    f[5] = len & 0xFF;
    f[6] = len >> 8;
    return f;
}

struct Scenario {
    const char* name;
    std::vector<uint8_t> prefix;   // octets envoyés avant les trames valides
    int validFrames;               // trames valides envoyées ensuite
    int expected;                  // trames qu'on doit retrouver à la sortie
    bool known;                    // défaut connu, non corrigé
};

std::vector<uint8_t> truncated(std::vector<uint8_t> f, size_t keep) { f.resize(keep); return f; }
std::vector<uint8_t> badFooter(std::vector<uint8_t> f) { f.back() = 0x00; return f; }
std::vector<uint8_t> strayF4(std::vector<uint8_t> f) { f.insert(f.begin(), 0xF4); return f; }

int runScenario(const Scenario& s) {
    stream = s.prefix;
    for (int i = 0; i < s.validFrames; ++i) {
        auto g = targetsFrame(2);
        stream.insert(stream.end(), g.begin(), g.end());
    }
    streamPos = 0;
    frameIndex = 0;

    // La sortie série du croquis part sur stdout : on la capture dans un fichier.
    std::fflush(stdout);
    FILE* cap = std::tmpfile();
    FILE* saved = stdout;
    stdout = cap;
    while (streamPos < stream.size()) loop();
    std::fflush(stdout);
    stdout = saved;

    std::rewind(cap);
    std::string out;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), cap)) > 0) out.append(buf, n);
    std::fclose(cap);

    int decoded = 0;
    for (size_t p = out.find("--- TARGETS"); p != std::string::npos; p = out.find("--- TARGETS", p + 1)) ++decoded;
    return decoded;
}

} // namespace

int main() {
    const std::vector<Scenario> scenarios = {
        {"3 trames valides",                                   {},                                3, 3, false},
        {"trame sans cible",                                   targetsFrame(0),                   3, 4, false},
        {"F4 parasite avant le header",                        strayF4(targetsFrame(1)),          3, 4, false},
        {"longueur 2 (trop courte)",                           {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00}, 3, 3, false},
        {"longueur 32767 (trop longue)",                       {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0xFF, 0x7F}, 3, 3, false},
        {"footer corrompu",                                    badFooter(targetsFrame(1)),        3, 3, false},
        {"longueur corrompue mais plausible (60 au lieu de 19)", targetsFrame(2, 60),             3, 3, true},
        {"trame tronquée (coupure de ligne)",                  truncated(targetsFrame(2), 10),    3, 3, true},
        {"longueur hors format 11 + 4n (13)",                  targetsFrame(0, 13),               3, 3, true},
    };

    int failures = 0, knownFailures = 0, fixedKnown = 0;
    for (const auto& s : scenarios) {
        int got = runScenario(s);
        bool ok = got == s.expected;
        const char* status;
        if (ok && !s.known)       status = "OK";
        else if (ok && s.known) { status = "CORRIGÉ (retirer known)"; ++fixedKnown; }
        else if (s.known)       { status = "ÉCHEC CONNU"; ++knownFailures; }
        else                    { status = "ÉCHEC"; ++failures; }
        std::printf("[%s] %s : %d/%d trames\n", status, s.name, got, s.expected);
    }
    std::printf("\n%zu scénarios, %d échec(s), %d échec(s) connu(s), %d connu(s) désormais corrigé(s)\n",
                scenarios.size(), failures, knownFailures, fixedKnown);
    return (failures || fixedKnown) ? 1 : 0;
}
