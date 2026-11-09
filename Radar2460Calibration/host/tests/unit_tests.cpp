// Tests unitaires des fonctions du croquis : formatDm, processFrame, parseByte,
// receiveRadarByte, loop, setup.
// Le croquis est inclus tel quel ; seule la couche Arduino est remplacée (test_support.h).
// Lancement : docker build --target tests .
#include "test_support.h"

#include "../../Radar2460Calibration.ino"

#include <functional>

namespace {

int checks = 0, failures = 0;

void checkEq(const std::string& got, const std::string& want, const char* expr, int line) {
    ++checks;
    if (got == want) return;
    ++failures;
    std::printf("  [ÉCHEC] ligne %d : %s\n    obtenu  : \"%s\"\n    attendu : \"%s\"\n", line, expr, got.c_str(),
                want.c_str());
}
#define CHECK_EQ(got, want) checkEq((got), (want), #got, __LINE__)
#define CHECK_INT(got, want) checkEq(std::to_string(got), std::to_string(want), #got, __LINE__)

std::string fmt(int16_t dm) {
    char out[DM_TEXT_LEN];
    formatDm(dm, out);
    return out;
}

// Sortie de processFrame pour une trame construite par le test.
std::string process(std::vector<uint8_t> f) {
    return test::capture([&] { processFrame(f.data(), static_cast<uint16_t>(f.size())); });
}

// Sortie de loop() une fois le flux entièrement lu.
std::string runLoop(const std::vector<uint8_t>& bytes) {
    test::feed(bytes);
    frameIndex = 0;
    return test::capture([] { while (!test::drained()) loop(); });
}

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
    std::vector<uint8_t> out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

const std::vector<uint8_t> ONE_TARGET = test::targetsFrame({{12, 35}});

void testFormatDm() {
    CHECK_EQ(fmt(0), "0.00");
    CHECK_EQ(fmt(1), "0.10");
    CHECK_EQ(fmt(9), "0.90");
    CHECK_EQ(fmt(10), "1.00");
    CHECK_EQ(fmt(25), "2.50");
    CHECK_EQ(fmt(-1), "-0.10");
    CHECK_EQ(fmt(-25), "-2.50");
    CHECK_EQ(fmt(INT16_MAX), "3276.70");
    CHECK_EQ(fmt(INT16_MIN), "-3276.80");   // le texte le plus long doit tenir dans DM_TEXT_LEN
}

void testProcessFrameOneTarget() {
    CHECK_EQ(process(ONE_TARGET),
             "\n--- TARGETS ---\n"
             "Target → X=1.20 m | Y=3.50 m\n"
             "Y min = 3.50 m | Y max = 3.50 m\n");
}

void testProcessFrameMinMax() {
    std::string out = process(test::targetsFrame({{-25, 20}, {5, 70}, {0, -3}}));
    CHECK_EQ(out,
             "\n--- TARGETS ---\n"
             "Target → X=-2.50 m | Y=2.00 m\n"
             "Target → X=0.50 m | Y=7.00 m\n"
             "Target → X=0.00 m | Y=-0.30 m\n"
             "Y min = -0.30 m | Y max = 7.00 m\n");
}

void testProcessFrameNoTarget() {
    CHECK_EQ(process(test::targetsFrame({})), "\n--- TARGETS ---\nNo target\n");
}

void testProcessFrameIgnoresOtherTypes() {
    CHECK_EQ(process(test::targetsFrame({{12, 35}}, -1, 0x01)), "");
}

void testProcessFrameLargestFrame() {
    // 29 cibles = 11 + 4 x 29 = 127 octets, la plus grande trame qui tient dans frame[128].
    std::vector<std::pair<int16_t, int16_t>> t(29, {INT16_MIN, INT16_MIN});
    std::string out = process(test::targetsFrame(t));
    CHECK_INT(test::count(out, "Target →"), 29);
    CHECK_INT(test::count(out, "Y min = -3276.80 m | Y max = -3276.80 m\n"), 1);
}

void testLoopDecodesValidFrame() {
    CHECK_EQ(runLoop(ONE_TARGET), process(ONE_TARGET));
    CHECK_INT(frameIndex, 0);   // prêt pour la trame suivante
}

void testLoopBackToBackFrames() {
    CHECK_INT(test::count(runLoop(concat({ONE_TARGET, ONE_TARGET, ONE_TARGET})), "--- TARGETS"), 3);
}

void testLoopFrameSplitAcrossCalls() {
    // La trame arrive en deux fois : loop() doit reprendre là où il s'était arrêté.
    std::vector<uint8_t> first(ONE_TARGET.begin(), ONE_TARGET.begin() + 9);
    std::vector<uint8_t> rest(ONE_TARGET.begin() + 9, ONE_TARGET.end());
    test::feed(first);
    frameIndex = 0;
    std::string out = test::capture([&] {
        loop();
        test::stream.insert(test::stream.end(), rest.begin(), rest.end());
        loop();
    });
    CHECK_INT(test::count(out, "--- TARGETS"), 1);
}

void testLoopSkipsGarbageBeforeHeader() {
    std::vector<uint8_t> noise = {0x00, 0xFF, 0xF3, 0xF4, 0xF3, 0x12, 0xF8, 0xF7};
    CHECK_INT(test::count(runLoop(concat({noise, ONE_TARGET})), "--- TARGETS"), 1);
}

void testLoopReportsInvalidLength() {
    std::string out = runLoop(concat({{0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00}, ONE_TARGET}));
    CHECK_INT(test::count(out, "[RADAR] invalid length dropped: 2\n"), 1);
    CHECK_INT(test::count(out, "--- TARGETS"), 1);
}

void testLoopReportsBadFooter() {
    std::vector<uint8_t> bad = ONE_TARGET;
    bad.back() = 0x00;
    std::string out = runLoop(concat({bad, ONE_TARGET}));
    CHECK_INT(test::count(out, "[RADAR] bad footer, frame dropped\n"), 1);
    CHECK_INT(test::count(out, "--- TARGETS"), 1);
}

void testProcessFrameRejectsBadDataSize() {
    std::vector<uint8_t> f = ONE_TARGET;
    f.insert(f.begin() + 11, {0xAA, 0xBB});   // 6 octets de données au lieu de 4
    f[5] = static_cast<uint8_t>(f.size());
    CHECK_EQ(process(f), "[RADAR] invalid data size dropped: 6\n");
}

// Résultats de parseByte octet par octet, sous forme de lettres : m(ore), d(one), x (dropped).
std::string parseAll(const std::vector<uint8_t>& bytes) {
    std::string r;
    test::capture([&] {
        for (uint8_t b : bytes) {
            ParseResult p = parseByte(b);
            r += p == PARSE_MORE ? 'm' : p == PARSE_DONE ? 'd' : 'x';
            if (p == PARSE_DROPPED) frameIndex = 0;
        }
    });
    return r;
}

void testParseByteValidFrame() {
    frameIndex = 0;
    CHECK_EQ(parseAll(ONE_TARGET), std::string(ONE_TARGET.size() - 1, 'm') + "d");
    CHECK_INT(frameIndex, 0);
}

void testParseByteDropsAsSoonAsLengthIsKnown() {
    frameIndex = 0;
    CHECK_EQ(parseAll({0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x02, 0x00}), "mmmmmmx");        // longueur 2
    CHECK_EQ(parseAll({0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x0D, 0x00}), "mmmmmmx");        // 13 : pas 11 + 4n
    CHECK_EQ(parseAll({0xF4, 0xF3, 0xF2, 0xF1, 0x07, 0x0D, 0x00}), "mmmmmmm");        // autre type : accepté
}

void testParseByteKeepsDroppedBytes() {
    // Sur rejet, les octets restent dans frame[] pour que l'appelant les rejoue.
    std::vector<uint8_t> bad = ONE_TARGET;
    bad.back() = 0x00;
    frameIndex = 0;
    test::capture([&] { for (uint8_t b : bad) parseByte(b); });
    CHECK_INT(frameIndex, bad.size());
    frameIndex = 0;
}

void testReceiveRadarByteReplaysDroppedFrame() {
    // Trame tronquée suivie d'une trame valide, octet par octet : la trame valide,
    // dont le header est dans les octets rejetés, est retrouvée.
    std::vector<uint8_t> cut = test::targetsFrame({{1, 2}, {3, 4}, {5, 6}});
    cut.resize(15);
    frameIndex = 0;
    std::string out = test::capture([&] { for (uint8_t b : concat({cut, ONE_TARGET})) receiveRadarByte(b); });
    CHECK_INT(test::count(out, "[RADAR] bad footer, frame dropped\n"), 1);
    CHECK_INT(test::count(out, "--- TARGETS"), 1);
    CHECK_INT(frameIndex, 0);
}

void testLoopRejectsBadDataSizeAndResyncs() {
    std::string out = runLoop(concat({test::targetsFrame({}, 13), ONE_TARGET}));
    CHECK_INT(test::count(out, "[RADAR] invalid data size dropped: 2\n"), 1);
    CHECK_INT(test::count(out, "--- TARGETS"), 1);
}

void testSetupBanner() {
    std::string out = test::capture([] { setup(); });
    CHECK_INT(test::count(out, "=== LD2460 X / Y CALIBRATION ===\n"), 1);
    CHECK_INT(test::count(out, "[RADAR] UART started\n"), 1);
}

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"formatDm : décimètres vers mètres", testFormatDm},
        {"processFrame : une cible", testProcessFrameOneTarget},
        {"processFrame : Y min / Y max, valeurs négatives", testProcessFrameMinMax},
        {"processFrame : trame sans cible", testProcessFrameNoTarget},
        {"processFrame : type autre que 0x04 ignoré", testProcessFrameIgnoresOtherTypes},
        {"processFrame : plus grande trame (29 cibles)", testProcessFrameLargestFrame},
        {"loop : trame valide décodée", testLoopDecodesValidFrame},
        {"loop : trames collées", testLoopBackToBackFrames},
        {"loop : trame reçue en deux fois", testLoopFrameSplitAcrossCalls},
        {"loop : octets parasites avant le header", testLoopSkipsGarbageBeforeHeader},
        {"loop : longueur impossible signalée", testLoopReportsInvalidLength},
        {"loop : footer corrompu signalé", testLoopReportsBadFooter},
        {"processFrame : taille des données hors format rejetée", testProcessFrameRejectsBadDataSize},
        {"parseByte : trame valide", testParseByteValidFrame},
        {"parseByte : rejet dès la longueur connue", testParseByteDropsAsSoonAsLengthIsKnown},
        {"parseByte : octets rejetés conservés pour le rejeu", testParseByteKeepsDroppedBytes},
        {"receiveRadarByte : trame valide retrouvée dans une trame rejetée", testReceiveRadarByteReplaysDroppedFrame},
        {"loop : taille des données hors format, puis resynchronisation", testLoopRejectsBadDataSizeAndResyncs},
        {"setup : bannière de démarrage", testSetupBanner},
    };

    int failedTests = 0;
    for (const auto& [name, fn] : tests) {
        int before = failures;
        fn();
        bool ok = failures == before;
        if (!ok) ++failedTests;
        std::printf("[%s] %s\n", ok ? "OK" : "ÉCHEC", name);
    }
    std::printf("\n%zu tests unitaires, %d vérifications, %d test(s) en échec\n", tests.size(), checks, failedTests);
    return failedTests ? 1 : 0;
}
