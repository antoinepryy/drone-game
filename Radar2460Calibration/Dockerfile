# Radar2460Calibration: compile the ESP32-C3 firmware and run the sketch natively
# against a simulated LD2460 radar. No local toolchain needed.

ARG ARDUINO_CLI_VERSION=1.1.1
ARG ESP32_CORE_VERSION=3.0.7

# ---------- 1. Real firmware for the XIAO ESP32-C3 ----------
FROM debian:bookworm-slim AS firmware
ARG ARDUINO_CLI_VERSION
ARG ESP32_CORE_VERSION
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl python3 python3-serial \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
      | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}
RUN arduino-cli config init \
    && arduino-cli config add board_manager.additional_urls \
       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json \
    && arduino-cli core update-index \
    && arduino-cli core install esp32:esp32@${ESP32_CORE_VERSION}
WORKDIR /src/Radar2460Calibration
COPY Radar2460Calibration.ino Config.h ./
RUN arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 --output-dir /out . \
    && ls -l /out

# ---------- 2. Native build of the same sketch ----------
FROM gcc:14 AS host
WORKDIR /src
COPY Radar2460Calibration.ino Config.h ./
COPY host/ host/
RUN mkdir -p /out && g++ -std=c++17 -O2 -Wall -Wextra -Ihost -I. host/main.cpp host/radar_sim.cpp -static -o /out/radar-calibration

# ---------- Tests de réception des trames : docker build --target tests . ----------
# Compilés avec AddressSanitizer et UBSan : un débordement de tampon fait échouer le build.
FROM gcc:14 AS tests
WORKDIR /src
COPY Radar2460Calibration.ino Config.h ./
COPY host/ host/
RUN g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
      -Ihost -I. host/tests/frame_tests.cpp -o /frame-tests \
    && /frame-tests

# ---------- Firmware export: docker build --target firmware-export -o out . ----------
FROM scratch AS firmware-export
COPY --from=firmware /out/ /

# ---------- 3. Runtime ----------
FROM debian:bookworm-slim
COPY --from=host /out/radar-calibration /usr/local/bin/radar-calibration
COPY --from=firmware /out/ /firmware/
ENV SIM_FRAMES=10 SIM_PERIOD_MS=300 SIM_TARGETS=2
CMD ["radar-calibration"]
