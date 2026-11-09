// Tests de réception des trames LD2460.
// Chaque scénario envoie un flux d'octets au croquis (inchangé), puis compte les trames
// effectivement décodées. Lancement : docker build --target tests .
//
// known=true : défaut connu, pas encore corrigé. Le scénario est affiché « ÉCHEC CONNU »
// sans faire échouer la suite. Le jour où il passe, la suite échoue pour qu'on retire le
// drapeau : un défaut corrigé doit devenir un test qui protège.
#include "test_support.h"

#include "../../Radar2460Calibration.ino"

namespace {

// Trame de cibles avec n cibles identiques ; forcedLen >= 0 remplace le champ longueur.
std::vector<uint8_t> targetsFrame(int targets, int forcedLen = -1) {
    std::vector<std::pair<int16_t, int16_t>> t;
    for (int i = 0; i < targets; ++i) t.push_back({static_cast<int16_t>(10 + i), 30});
    return test::targetsFrame(t, forcedLen);
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
    std::vector<uint8_t> bytes = s.prefix;
    for (int i = 0; i < s.validFrames; ++i) {
        auto g = targetsFrame(2);
        bytes.insert(bytes.end(), g.begin(), g.end());
    }
    test::feed(bytes);
    frameIndex = 0;

    // La sortie série du croquis part sur stdout : on la capture.
    std::string out = test::capture([] { while (!test::drained()) loop(); });
    return test::count(out, "--- TARGETS");
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
        {"longueur corrompue mais plausible (60 au lieu de 19)", targetsFrame(2, 60),             3, 3, false},
        {"trame tronquée (coupure de ligne)",                  truncated(targetsFrame(2), 10),    3, 3, false},
        {"longueur hors format 11 + 4n (13)",                  targetsFrame(0, 13),               3, 3, false},
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
