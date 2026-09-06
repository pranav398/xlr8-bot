#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

const char* ssid     = "TheTerminators";
const char* password = "ASDFhjkl";

const char* picoIP = "192.168.4.1";
const unsigned int udpPort = 4210;

const unsigned long sendIntervalMs = 50;

#define SDA_PIN 0
#define SCL_PIN 2
#define SW1_PIN 3
#define SW2_PIN 1

#define MPU_ADDR 0x68

WiFiUDP udp;
unsigned long lastSend = 0;

int16_t ax, ay, az, gx, gy, gz;
uint8_t i2cFailCount = 0;
bool mpuReady = false;
bool mpuInBackoff = false;
unsigned long mpuBackoffStart = 0;
const unsigned long mpuRetryBackoffMs = 2000;
bool udpStarted = false;

bool mpuInit() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0);
    return Wire.endTransmission(true) == 0;
}

bool mpuRead() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;

    uint8_t n = Wire.requestFrom(MPU_ADDR, (uint8_t)14, (uint8_t)true);
    if (n != 14) return false;

    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read();
    gx = (Wire.read() << 8) | Wire.read();
    gy = (Wire.read() << 8) | Wire.read();
    gz = (Wire.read() << 8) | Wire.read();
    return true;
}

bool i2cReleaseSCL() {
    pinMode(SCL_PIN, INPUT_PULLUP);
    unsigned long start = micros();
    while (!digitalRead(SCL_PIN)) {
        if ((unsigned long)(micros() - start) > 1000) {
            return false;
        }
        yield();
    }
    return true;
}

void i2cSetSCLLow() {
    pinMode(SCL_PIN, OUTPUT);
    digitalWrite(SCL_PIN, LOW);
}
void i2cSetSDA(bool high) {
    if (high) {
        pinMode(SDA_PIN, INPUT_PULLUP);
    } else {
        pinMode(SDA_PIN, OUTPUT);
        digitalWrite(SDA_PIN, LOW);
    }
}

bool i2cBusRecovery() {
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(10);

    if (!i2cReleaseSCL()) {
        return false;
    }

    for (int i = 0; i < 9; i++) {
        if (digitalRead(SDA_PIN)) break;
        i2cSetSCLLow();
        delayMicroseconds(5);
        if (!i2cReleaseSCL()) return false;
        delayMicroseconds(5);
        yield();
    }

    if (!digitalRead(SDA_PIN)) {
        return false;
    }

    i2cSetSDA(false);
    delayMicroseconds(5);
    if (!i2cReleaseSCL()) return false;
    delayMicroseconds(5);
    i2cSetSDA(true);
    delayMicroseconds(5);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.setClockStretchLimit(1500);

    if (!mpuInit()) return false;
    if (!mpuRead()) return false;
    return true;
}

bool wifiAttemptActive = false;
bool wifiInBackoff = false;
unsigned long wifiStateStart = 0;
const unsigned long wifiConnectTimeoutMs = 10000;
const unsigned long wifiRetryDelayMs = 2000;

void ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiAttemptActive = false;
        wifiInBackoff = false;
        if (!udpStarted) {
            udp.begin(udpPort);
            udpStarted = true;
        }
        return;
    }

    udpStarted = false;
    unsigned long now = millis();

    if (wifiInBackoff) {
        if ((unsigned long)(now - wifiStateStart) < wifiRetryDelayMs) return;
        wifiInBackoff = false;
    }

    if (!wifiAttemptActive) {
        WiFi.begin(ssid, password);
        wifiAttemptActive = true;
        wifiStateStart = now;
        return;
    }

    if ((unsigned long)(now - wifiStateStart) > wifiConnectTimeoutMs) {
        WiFi.disconnect();
        wifiAttemptActive = false;
        wifiInBackoff = true;
        wifiStateStart = now;
    }
}

void updateMpuState(unsigned long now) {
    if (!mpuReady) {
        bool dueForAttempt = !mpuInBackoff ||
            (unsigned long)(now - mpuBackoffStart) >= mpuRetryBackoffMs;

        if (dueForAttempt) {
            bool recovered = (mpuInit() && mpuRead()) || i2cBusRecovery();
            mpuReady = recovered;
            mpuInBackoff = !recovered;
            mpuBackoffStart = now;
        }
        if (!mpuReady) {
            ax = ay = az = gx = gy = gz = 0;
        }
        return;
    }

    if (mpuRead()) {
        i2cFailCount = 0;
    } else {
        i2cFailCount++;
        if (i2cFailCount >= 5) {
            mpuReady = false;
            mpuInBackoff = false;
            i2cFailCount = 0;
            ax = ay = az = gx = gy = gz = 0;
        }
    }
}

void setup() {
    pinMode(SW1_PIN, INPUT);
    pinMode(SW2_PIN, INPUT);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.setClockStretchLimit(1500);
    mpuReady = mpuInit() && mpuRead();

    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    ensureWifi();
}

void loop() {
    ensureWifi();
    yield();

    unsigned long now = millis();
    if (now - lastSend >= sendIntervalMs) {
        lastSend = now;

        updateMpuState(now);

        int sw1 = digitalRead(SW1_PIN) == LOW ? 1 : 0;
        int sw2 = digitalRead(SW2_PIN) == LOW ? 1 : 0;

        char buf[112];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                mpuReady ? 1 : 0, ax, ay, az, gx, gy, gz, sw1, sw2);

        if (WiFi.status() == WL_CONNECTED && udpStarted) {
            udp.beginPacket(picoIP, udpPort);
            udp.write(buf);
            udp.endPacket();
        }
    }
}

// #include <ESP8266WiFi.h>
// #include <WiFiUdp.h>
// #include <Wire.h>

// const char* ssid     = "TheTerminators";
// const char* password = "ASDFhjkl";

// const char* picoIP = "192.168.4.1";
// const unsigned int udpPort = 4210;

// const unsigned long sendIntervalMs = 20;

// #define SDA_PIN 0
// #define SCL_PIN 2
// #define SW1_PIN 3
// #define SW2_PIN 1

// #define MPU_ADDR 0x68

// WiFiUDP udp;
// unsigned long lastSend = 0;

// int16_t ax, ay, az, gx, gy, gz;

// void mpuInit() {
//     Wire.beginTransmission(MPU_ADDR);
//     Wire.write(0x6B);
//     Wire.write(0);
//     Wire.endTransmission(true);
// }

// void mpuRead() {
//     Wire.beginTransmission(MPU_ADDR);
//     Wire.write(0x3B);
//     Wire.endTransmission(false);
//     Wire.requestFrom(MPU_ADDR, 14, true);

//     ax = (Wire.read() << 8) | Wire.read();
//     ay = (Wire.read() << 8) | Wire.read();
//     az = (Wire.read() << 8) | Wire.read();
//     Wire.read(); Wire.read();
//     gx = (Wire.read() << 8) | Wire.read();
//     gy = (Wire.read() << 8) | Wire.read();
//     gz = (Wire.read() << 8) | Wire.read();
// }

// void setup() {
//     pinMode(SW1_PIN, INPUT_PULLUP);
//     pinMode(SW2_PIN, INPUT_PULLUP);

//     Wire.begin(SDA_PIN, SCL_PIN);
//     Wire.setClock(400000);
//     mpuInit();

//     WiFi.mode(WIFI_STA);
//     WiFi.setSleepMode(WIFI_NONE_SLEEP);
//     WiFi.begin(ssid, password);

//     while (WiFi.status() != WL_CONNECTED) {
//         delay(200);
//     }

//     udp.begin(udpPort);
// }

// void loop() {
//     unsigned long now = millis();
//     if (now - lastSend >= sendIntervalMs) {
//         lastSend = now;

//         mpuRead();
//         int sw1 = digitalRead(SW1_PIN) == LOW ? 1 : 0;
//         int sw2 = digitalRead(SW2_PIN) == LOW ? 1 : 0;

//         char buf[96];
//         snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d,%d",
//                 ax, ay, az, gx, gy, gz, sw1, sw2);

//         udp.beginPacket(picoIP, udpPort);
//         udp.write(buf);
//         udp.endPacket();
//     }
// }