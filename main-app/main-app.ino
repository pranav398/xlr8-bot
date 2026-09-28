#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

// ============================================================
// WIFI / UDP
// ============================================================

WiFiUDP controlUDP;

const char* ssid = "TheTerminators";
const char* password = "ASDFhjkl";
const uint16_t udpPort = 5000;


// ============================================================
// MOTOR PINS
// Taken directly from your existing working bot code
// ============================================================

const int enA = 2;
const int enB = 7;

const int in1 = 4;
const int in2 = 3;

const int in3 = 6;
const int in4 = 5;


// ============================================================
// CONTROL PACKET
// ============================================================

struct __attribute__((packed)) ControlPacket {
    int8_t speed;        // -100 to +100
    int8_t steering;     // -100 to +100
    uint8_t mode;        // 0=NORMAL, 1=LEFT, 2=RIGHT
    uint8_t brake;       // 1 = brake
    uint8_t direction;   // informational only
    uint8_t sequence;
};


// ============================================================
// CURRENT CONTROL STATE
// ============================================================

int8_t currentSpeed = 0;
int8_t currentSteering = 0;

uint8_t currentMode = 0;
uint8_t currentBrake = 0;
uint8_t currentDirection = 0;

uint8_t lastSequence = 255;


// ============================================================
// CONNECTION FAILSAFE
// ============================================================

unsigned long lastControlPacket = 0;

const unsigned long CONTROL_TIMEOUT = 1000;


// ============================================================
// MOTOR CONTROL
// ============================================================

void controlMotor(
    int leftSpeed,
    bool leftForward,
    int rightSpeed,
    bool rightForward
) {
    leftSpeed = constrain(leftSpeed, 0, 255);
    rightSpeed = constrain(rightSpeed, 0, 255);

    // LEFT MOTOR
    if (leftForward) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }

    // RIGHT MOTOR
    if (rightForward) {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    } else {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }

    analogWrite(enA, leftSpeed);
    analogWrite(enB, rightSpeed);
}


// ============================================================
// STOP / BRAKE
// ============================================================

void brakeMotors() {
    analogWrite(enA, 0);
    analogWrite(enB, 0);

    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);

    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
}


// ============================================================
// -100..100  --->  0..255
// ============================================================

int speedToPWM(int speedValue) {
    speedValue = constrain(speedValue, -100, 100);

    return map(abs(speedValue), 0, 100, 0, 255);
}


// ============================================================
// NORMAL DIFFERENTIAL STEERING
// ============================================================

void normalDrive(int speedValue, int steeringValue) {

    speedValue = constrain(speedValue, -100, 100);
    steeringValue = constrain(steeringValue, -100, 100);

    // Speed = 0 => stop
    if (speedValue == 0) {
        brakeMotors();
        return;
    }

    int basePWM = speedToPWM(speedValue);

    /*
       Steering represents turn sharpness.

       steering = 0
           L = 100%
           R = 100%

       steering = +50
           one side = 50%
           other side = 100%

       steering = +100
           one side = 0%
           other side = 100%

       This gives differential steering without changing
       the requested forward/reverse direction.
    */

    int steeringMagnitude = abs(steeringValue);

    int reducedPWM = map(
        steeringMagnitude,
        0,
        100,
        basePWM,
        0
    );

    int leftPWM;
    int rightPWM;

    if (steeringValue > 0) {

        // RIGHT TURN
        leftPWM = basePWM;
        rightPWM = reducedPWM;

    } else if (steeringValue < 0) {

        // LEFT TURN
        leftPWM = reducedPWM;
        rightPWM = basePWM;

    } else {

        // STRAIGHT
        leftPWM = basePWM;
        rightPWM = basePWM;
    }

    bool forward = (speedValue > 0);

    controlMotor(
        leftPWM,
        forward,
        rightPWM,
        forward
    );
}


// ============================================================
// ON-SPOT LEFT TURN
// ============================================================

void turnLeft(int speedValue) {

    int pwm = speedToPWM(speedValue);

    if (pwm == 0) {
        brakeMotors();
        return;
    }

    /*
       LEFT SIDE  -> REVERSE
       RIGHT SIDE -> FORWARD

       This produces an on-spot left rotation.
    */

    controlMotor(
        pwm,
        false,
        pwm,
        true
    );
}


// ============================================================
// ON-SPOT RIGHT TURN
// ============================================================

void turnRight(int speedValue) {

    int pwm = speedToPWM(speedValue);

    if (pwm == 0) {
        brakeMotors();
        return;
    }

    /*
       LEFT SIDE  -> FORWARD
       RIGHT SIDE -> REVERSE

       This produces an on-spot right rotation.
    */

    controlMotor(
        pwm,
        true,
        pwm,
        false
    );
}


// ============================================================
// APPLY CURRENT CONTROL
// ============================================================

void applyControl() {

    // --------------------------------------------------------
    // BRAKE OVERRIDES EVERYTHING
    // --------------------------------------------------------

    if (currentBrake == 1 || currentSpeed == 0) {
        brakeMotors();

        Serial.println("ACTION: BRAKE");
        return;
    }


    // --------------------------------------------------------
    // MODE 0: NORMAL
    // --------------------------------------------------------

    if (currentMode == 0) {

        normalDrive(
            currentSpeed,
            currentSteering
        );

        Serial.println("MODE: NORMAL");
    }


    // --------------------------------------------------------
    // MODE 1: LEFT
    // --------------------------------------------------------

    else if (currentMode == 1) {

        turnLeft(currentSpeed);

        Serial.println("MODE: LEFT");
    }


    // --------------------------------------------------------
    // MODE 2: RIGHT
    // --------------------------------------------------------

    else if (currentMode == 2) {

        turnRight(currentSpeed);

        Serial.println("MODE: RIGHT");
    }


    // --------------------------------------------------------
    // UNKNOWN MODE
    // --------------------------------------------------------

    else {

        brakeMotors();

        Serial.println("UNKNOWN MODE -> BRAKE");
    }
}


// ============================================================
// PING HANDLER
// ============================================================

void handlePing() {

    Serial.println("PING received from Android");

    controlUDP.beginPacket(
        controlUDP.remoteIP(),
        controlUDP.remotePort()
    );

    controlUDP.write(
        (const uint8_t*)"PONG",
        4
    );

    controlUDP.endPacket();

    Serial.println("PONG sent\n");
}


// ============================================================
// CONTROL PACKET HANDLER
// ============================================================

void handleControlPacket() {

    ControlPacket packet;

    if (controlUDP.read(
        (uint8_t*)&packet,
        sizeof(packet)
    ) != sizeof(packet)) {

        Serial.println("ERROR: Failed to read complete packet");
        return;
    }


    // --------------------------------------------------------
    // SEQUENCE NUMBER CHECK
    // --------------------------------------------------------

    /*
       Modular sequence difference.

       This handles:
       255 -> 0 wrapping correctly.
    */

    int8_t diff =
        (int8_t)(packet.sequence - lastSequence);

    if (diff <= 0) {
        return;
    }


    // --------------------------------------------------------
    // UPDATE STATE
    // --------------------------------------------------------

    lastSequence = packet.sequence;

    currentSpeed = constrain(
        packet.speed,
        -100,
        100
    );

    currentSteering = constrain(
        packet.steering,
        -100,
        100
    );

    currentMode = packet.mode;
    currentBrake = packet.brake;
    currentDirection = packet.direction;

    lastControlPacket = millis();


    // --------------------------------------------------------
    // DEBUG OUTPUT
    // --------------------------------------------------------

    Serial.println("================================");
    Serial.println("CONTROL PACKET RECEIVED");

    Serial.print("Sequence: ");
    Serial.println(packet.sequence);

    Serial.print("Speed: ");
    Serial.println(currentSpeed);

    Serial.print("Steering: ");
    Serial.println(currentSteering);

    Serial.print("Mode: ");

    switch (currentMode) {

        case 0:
            Serial.println("NORMAL");
            break;

        case 1:
            Serial.println("LEFT");
            break;

        case 2:
            Serial.println("RIGHT");
            break;

        default:
            Serial.println("UNKNOWN");
            break;
    }

    Serial.print("Brake: ");
    Serial.println(currentBrake);

    Serial.print("Direction field: ");

    switch (currentDirection) {

        case 0:
            Serial.println("STOP");
            break;

        case 1:
            Serial.println("FORWARD");
            break;

        case 2:
            Serial.println("REVERSE");
            break;

        default:
            Serial.println("UNKNOWN");
            break;
    }

    Serial.println("================================");


    // --------------------------------------------------------
    // ACTUALLY MOVE THE BOT
    // --------------------------------------------------------

    applyControl();

    Serial.println();
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(1000);


    // --------------------------------------------------------
    // MOTOR PINS
    // --------------------------------------------------------

    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);

    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);

    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);


    // Make sure bot starts stopped
    brakeMotors();


    // --------------------------------------------------------
    // WIFI ACCESS POINT
    // --------------------------------------------------------

    IPAddress local_ip(
        192, 168, 4, 1
    );

    IPAddress gateway(
        192, 168, 4, 1
    );

    IPAddress subnet(
        255, 255, 255, 0
    );

    WiFi.softAPConfig(
        local_ip,
        gateway,
        subnet
    );

    WiFi.softAP(
        ssid,
        password
    );


    // --------------------------------------------------------
    // UDP
    // --------------------------------------------------------

    controlUDP.begin(udpPort);


    // --------------------------------------------------------
    // STARTUP INFORMATION
    // --------------------------------------------------------

    Serial.println();
    Serial.println("==============================");
    Serial.println("XLR8 BOT CONTROL STARTED");
    Serial.println("==============================");

    Serial.print("SSID: ");
    Serial.println(ssid);

    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    Serial.print("UDP Port: ");
    Serial.println(udpPort);

    Serial.println();
    Serial.println("Waiting for Android...");
    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

    int packetSize =
        controlUDP.parsePacket();


    // --------------------------------------------------------
    // NO PACKET
    // --------------------------------------------------------

    if (packetSize <= 0) {

        // Communication failsafe
        if (
            lastControlPacket != 0 &&
            millis() - lastControlPacket >
            CONTROL_TIMEOUT
        ) {

            brakeMotors();

            Serial.println(
                "FAILSAFE: No control packet -> BRAKE"
            );

            lastControlPacket = 0;
        }

        return;
    }


    // --------------------------------------------------------
    // PING
    // --------------------------------------------------------

    if (packetSize == 4) {

        char ping[5];

        controlUDP.read(
            (uint8_t*)ping,
            4
        );

        ping[4] = '\0';

        if (strcmp(ping, "PING") == 0) {

            handlePing();
            return;
        }

        Serial.println(
            "Unknown 4-byte packet"
        );

        return;
    }


    // --------------------------------------------------------
    // CONTROL PACKET
    // --------------------------------------------------------

    if (packetSize == sizeof(ControlPacket)) {

        handleControlPacket();
        return;
    }


    // --------------------------------------------------------
    // INVALID PACKET
    // --------------------------------------------------------

    Serial.print(
        "Invalid packet size: "
    );

    Serial.println(packetSize);


    while (controlUDP.available()) {
        controlUDP.read();
    }
}