#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <math.h>
#include <Servo.h>
#include <algorithm>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

const char* ssid = "TheTerminators";
const char* password = "ASDFhjkl";
const unsigned int udpPort = 4210;
const unsigned int webPort = 6967;

const char* stationIP = "192.168.4.100"; 
const uint16_t stationTcpPort = 6769;
WiFiClient tcpClient;

WiFiUDP udp;
WebServer server(webPort);
WebSocketsServer webSocket = WebSocketsServer(6968);

Adafruit_MPU6050 mpu;
bool mpuInitialized = false;
float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
float gyroBiasZ = 0.0f;

int currentMode = 0;
long ax, ay, az;
int sw1, sw2;

unsigned long prevStepTime = 0;
int mechState = 0;

unsigned long lastRecv = 0;

const float MAX_TILT_ANGLE  = 45.0f;
const float DEADZONE        = 5.0f;
const int   MAX_PWM         = 255;

const int TURN_SPEED_HIGH   = 220;
const int TURN_SPEED_LOW    = 80;
const int BASE_SPEED        = 180; 

const int stopDist = 30;

bool isRemoteConnected = false;
bool idleLogged = false;

int enA = 2;
int enB = 7;
int in1 = 4; 
int in2 = 3;
int in3 = 6;
int in4 = 5;

// int ir1 = 8;
// int ir2 = 9;
// int ir3 = 10;

int servo = 11;
int us_trigger = 12;
int us_echo = 13;

int ledRed = 21;
int ledGreen = 19;
int ledBlue = 20;

Servo scanServo;
int currentServoAngle = 90; // Tracks servo position for slow movement

// Slow servo movement to prevent wire snaps and brownouts
void slowServoMove(int targetAngle) {
    targetAngle = constrain(targetAngle, 0, 180);
    while (currentServoAngle != targetAngle) {
        if (currentServoAngle < targetAngle) {
            currentServoAngle++;
        } else {
            currentServoAngle--;
        }
        scanServo.write(currentServoAngle);
        server.handleClient();
        webSocket.loop();
        delay(15); 
    }
}

void webLog(String message, bool force = false) {
    Serial.println(message);
    static unsigned long lastLogTime = 0;

    if (force || (millis() - lastLogTime >= 200)) {
        webSocket.broadcastTXT(message);
        lastLogTime = millis();
    }
}

static bool motorsStopped = false;

void controlMotor(int leftSpeed, bool leftDir, int rightSpeed, bool rightDir) {
    if (leftSpeed != 0 || rightSpeed != 0) {
        motorsStopped = false;
    }

    static unsigned long lastLogTime = 0;
    static int lastLeftSpeed = -1;
    static int lastRightSpeed = -1;
    static bool lastLeftDir = true;
    static bool lastRightDir = true;
    
    bool dirChanged = (leftDir != lastLeftDir || rightDir != lastRightDir);
    bool speedChanged = (abs(leftSpeed - lastLeftSpeed) > 15) || (abs(rightSpeed - lastRightSpeed) > 15);
    bool isStopping = (leftSpeed == 0 && rightSpeed == 0) && (lastLeftSpeed != 0 || lastRightSpeed != 0);

    if (isStopping || ((dirChanged || speedChanged) && (millis() - lastLogTime > 1000))) {
        String logMsg = "Left PWM: " + String(leftSpeed) + " (" + (leftDir ? "FWD" : "REV") + ") | Right PWM: " + String(rightSpeed) + " (" + (rightDir ? "FWD" : "REV") + ")";
        webLog(logMsg);
        
        lastLeftSpeed = leftSpeed;
        lastRightSpeed = rightSpeed;
        lastLeftDir = leftDir;
        lastRightDir = rightDir;
        lastLogTime = millis();
    }

    if (leftDir) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }

    if (rightDir) {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    } else {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }

    analogWrite(enA, leftSpeed);
    analogWrite(enB, rightSpeed);
}

void brakeMotors() {
    controlMotor(0, true, 0, true);
    if (!motorsStopped) {
        webLog("Brakes applied: All motors stopped.", true);
        motorsStopped = true;
    }
}

void idleMode() {
    if (!idleLogged) {
        brakeMotors();
        webLog("System switched to IDLE Mode (Mode 0).", true);
        idleLogged = true;
    }
}

void calculateMotorPWM(long raw_ax, long raw_ay, long raw_az, int &leftPWM, int &rightPWM, bool &leftFwd, bool &rightFwd) {
    float fax = (float)raw_ax;
    float fay = (float)raw_ay;
    float faz = (float)raw_az;

    float pitch = atan2(fax, sqrt(fay * fay + faz * faz)) * 180.0f / M_PI;
    float roll  = atan2(fay, sqrt(fax * fax + faz * faz)) * 180.0f / M_PI;

    if (fabs(pitch) < DEADZONE) pitch = 0.0f;
    if (fabs(roll) < DEADZONE)  roll  = 0.0f;

    pitch = constrain(pitch, -MAX_TILT_ANGLE, MAX_TILT_ANGLE);
    roll  = constrain(roll,  -MAX_TILT_ANGLE, MAX_TILT_ANGLE);

    float throttle = pitch / MAX_TILT_ANGLE;
    float steering = roll  / MAX_TILT_ANGLE;

    float left_mix  = throttle - steering;
    float right_mix = throttle + steering;

    float max_magnitude = max(fabs(left_mix), fabs(right_mix));
    if (max_magnitude > 1.0f) {
        left_mix  /= max_magnitude;
        right_mix /= max_magnitude;
    }

    leftPWM  = (int)fabs(left_mix * (float)MAX_PWM);
    rightPWM = (int)fabs(right_mix * (float)MAX_PWM);

    leftFwd  = (left_mix >= 0.0f);
    rightFwd = (right_mix >= 0.0f);
}

void remoteControl() {
    int currentLeftPWM  = 0;
    int currentRightPWM = 0;
    bool leftDir        = true;
    bool rightDir       = true;

    int packetSize = udp.parsePacket();

    if (packetSize) {
        char buf[128];
        int len = udp.read(buf, sizeof(buf) - 1);

        if (len > 0) {
            buf[len] = '\0';
            for (int i = 0; i < len; i++) {
                if (buf[i] == '\r' || buf[i] == '\n') { buf[i] = '\0'; break; }
            }

            long values[9] = {0}; 
            int idx = 0;
            char* token = strtok(buf, ",");

            while (token != NULL && idx < 9) {
                values[idx++] = atol(token);
                token = strtok(NULL, ",");
            }

            if (idx == 9) {
                ay = values[1]; ax = values[2]; az = values[3];
                sw1 = values[7]; sw2 = values[8];

                lastRecv = millis();
                isRemoteConnected = true; 

                if (sw1 == 1) { 
                    ay = 0; 
                }

                calculateMotorPWM(ax, ay, az, currentLeftPWM, currentRightPWM, leftDir, rightDir);

                if (sw2 == 1) {
                    if (currentLeftPWM > 0) {
                        currentLeftPWM = map(currentLeftPWM, 1, 255, 110, 220);
                    }
                    if (currentRightPWM > 0) {
                        currentRightPWM = map(currentRightPWM, 1, 255, 110, 220);
                    }
                }

                controlMotor(currentLeftPWM, leftDir, currentRightPWM, rightDir);
            }
        }
    }

    if (isRemoteConnected && (millis() - lastRecv > 1000)) {
        isRemoteConnected = false;
        currentLeftPWM = 0; currentRightPWM = 0;
        leftDir = true; rightDir = true;

        webLog("Connection lost! Halting motors.", true);
        brakeMotors();
    }
}

int getDistance() {
    int readings[3];
    int validReadings = 0;

    for (int i = 0; i < 3; i++) {
        digitalWrite(us_trigger, LOW);
        delayMicroseconds(2);
        digitalWrite(us_trigger, HIGH);
        delayMicroseconds(10);
        digitalWrite(us_trigger, LOW);
        
        long duration = pulseIn(us_echo, HIGH, 15000); 
        if (duration > 0) {
            readings[i] = (int)(duration * 0.034 / 2);
            validReadings++;
        } else {
            readings[i] = -1;
        }
        delay(20);
    }

    if (validReadings == 0) return 999;

    std::sort(readings, readings + 3);
    return readings[2];
}

void setLEDs(int redBrightness, int greenBrightness, int blueBrightness) {
    analogWrite(ledRed, redBrightness);
    analogWrite(ledGreen, greenBrightness);
    analogWrite(ledBlue, blueBrightness);
}

bool safeDelay(unsigned long ms, bool checkObstacles = false) {
    unsigned long start = millis();
    unsigned long lastDistCheck = 0;
    int startMode = currentMode;
    
    while (millis() - start < ms) {
        server.handleClient();
        webSocket.loop();
        
        if (currentMode != startMode) {
            brakeMotors();
            return false; 
        }
        
        if (checkObstacles && (millis() - lastDistCheck > 50)) {
            if (getDistance() <= stopDist) {
                brakeMotors();
                webLog("EMERGENCY STOP: Obstacle detected mid-maneuver!", true);
                return false; 
            }
            lastDistCheck = millis();
        }
        delay(1);
    }
    return true;
}

void calibrateGyro() {
    if (!mpuInitialized) return;

    sensors_event_t a, g, temp;
    const int samples = 200;

    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;

    webLog("Calibrating gyro. Keep robot completely still...", true);

    brakeMotors();
    delay(500);

    for (int i = 0; i < samples; i++) {
        mpu.getEvent(&a, &g, &temp);
        sumX += g.gyro.x;
        sumY += g.gyro.y;
        sumZ += g.gyro.z;
        delay(5);
    }

    gyroBiasX = sumX / samples;
    gyroBiasY = sumY / samples;
    gyroBiasZ = sumZ / samples;

    webLog("Gyro calibration complete.", true);
}

bool turnExactAngle(float targetAngleMagnitude, bool turnRight) {
    if (targetAngleMagnitude <= 0.0f) {
        brakeMotors();
        return true;
    }

    const int startMode = currentMode;

    if (!mpuInitialized) {
        int turnTime = (int)(targetAngleMagnitude * 15.0f);
        controlMotor(TURN_SPEED_HIGH, turnRight, TURN_SPEED_HIGH, !turnRight);
        bool result = safeDelay(turnTime, false);
        brakeMotors();
        return result;
    }

    brakeMotors();
    delay(100);

    sensors_event_t a, g, temp;
    float currentAngle = 0.0f;
    unsigned long lastTime = millis();
    unsigned long startTime = millis();
    unsigned long maxTurnTime = max(2000UL, (unsigned long)(targetAngleMagnitude * 25.0f));

    controlMotor(TURN_SPEED_HIGH, turnRight, TURN_SPEED_HIGH, !turnRight);

    while (currentAngle < targetAngleMagnitude) {
        unsigned long now = millis();

        server.handleClient();
        webSocket.loop();

        if (currentMode != startMode) {
            brakeMotors();
            webLog("Turn aborted: mode changed.", true);
            return false;
        }

        if (now - startTime > maxTurnTime) {
            brakeMotors();
            webLog("MPU TURN TIMEOUT: Aborting turn.", true);
            return false;
        }

        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        if (dt <= 0.0f) {
            delay(1);
            continue;
        }

        if (mpu.getEvent(&a, &g, &temp)) {
            float gzDeg = (g.gyro.z - gyroBiasZ) * 57.2957795f;
            currentAngle += fabs(gzDeg) * dt;
        }

        delay(5);
    }

    brakeMotors();
    webLog("MPU TURN COMPLETE: " + String(currentAngle, 1) + " deg", true);
    return true;
}

void mrt_mech(){
    static bool done = false;
    if(!done){
        turnExactAngle(15.0, true);
        setLEDs(255, 0, 0);
        delay(1000);
        turnExactAngle(15.0, false);
        setLEDs(255, 255, 0);
        done = true;
    }
}

void auv_elec() {
    static unsigned long lastDataPacketTime = 0;
    static unsigned long lastConnectAttempt = 0;
    static unsigned long lastRequestTime = 0;
    static bool requestSent = false;

    if (!tcpClient.connected()) {
        if (millis() - lastConnectAttempt > 1000) {
            webLog("AUV: Connecting to base station...", true);
            setLEDs(255, 0, 0);
            brakeMotors();
            
            if (tcpClient.connect(stationIP, stationTcpPort)) {
                webLog("AUV: Connected to base!", true);
                requestSent = false;
                lastDataPacketTime = millis(); 
                lastRequestTime = millis();
            } else {
                lastConnectAttempt = millis();
            }
        }
        server.handleClient();
        webSocket.loop();
        return;
    }

    if (!requestSent && (millis() - lastRequestTime > 500)) {
        JsonDocument reqDoc;
        reqDoc["header"] = "data_req";
        reqDoc["status"] = "ready";
        serializeJson(reqDoc, tcpClient);
        tcpClient.println();
        requestSent = true;
    }

    if (tcpClient.available()) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, tcpClient);
        if (!error) {
            const char* header = doc["header"];
            if (header && String(header) == "dexa_cmd") {
                int r = doc["payload"]["r"];
                int g = doc["payload"]["g"];
                int b = doc["payload"]["b"];
                
                lastDataPacketTime = millis(); 
                lastRequestTime = millis();
                
                if (r == 0 && g == 255 && b == 0) {
                    setLEDs(0, 255, 0);
                    controlMotor(200, true, 200, true);
                } else if (r == 0 && g == 0 && b == 255) {
                    setLEDs(0, 0, 255);
                    controlMotor(200, false, 200, false);
                } else if (r == 255 && g == 255 && b == 0) {
                    setLEDs(255, 255, 0);
                    turnExactAngle(90, true);
                    brakeMotors();
                } else if (r == 128 && g == 0 && b == 128) {
                    setLEDs(128, 0, 128);
                    turnExactAngle(90, false);
                    brakeMotors();
                } else if (r == 255 && g == 255 && b == 255) {
                    setLEDs(255, 255, 255);
                    turnExactAngle(360, true);
                    brakeMotors();
                } else if (r == 255 && g == 0 && b == 0) {
                    setLEDs(255, 0, 0);
                    brakeMotors();
                }
                requestSent = false;
            }
        } else {
            while (tcpClient.available()) tcpClient.read(); 
            requestSent = false;
            lastRequestTime = millis();
        }
    }

    if (millis() - lastDataPacketTime > 5000) {
        setLEDs(255, 0, 0);
        brakeMotors();
        tcpClient.stop();
        requestSent = false;
        lastDataPacketTime = millis();
        lastConnectAttempt = millis(); 
    }

    server.handleClient();
    webSocket.loop();
}

void ir_mode() {
    static unsigned long lostTime = 0;
    static bool lastTurnedLeft = false;

    bool left   = digitalRead(ir1);
    bool center = digitalRead(ir2);
    bool right  = digitalRead(ir3);

    webLog("Left: " + String(left ? 0 : 1) + " | Center: " + String(center ? 0 : 1) + " | Right: " + String(right ? 0 : 1));

    if (center && !left && !right) {
        lostTime = 0;
        controlMotor(BASE_SPEED, true, BASE_SPEED, true);
    }
    else if (left && !center && !right) {
        lostTime = 0;
        lastTurnedLeft = true;
        controlMotor(TURN_SPEED_LOW, true, BASE_SPEED, true);
    }
    else if (right && !center && !left) {
        lostTime = 0;
        lastTurnedLeft = false;
        controlMotor(BASE_SPEED, true, TURN_SPEED_LOW, true);
    }
    else if (left && center && !right) {
        lostTime = 0;
        lastTurnedLeft = true;
        controlMotor(0, true, BASE_SPEED, true);
    }
    else if (right && center && !left) {
        lostTime = 0;
        lastTurnedLeft = false;
        controlMotor(BASE_SPEED, true, 0, true);
    }
    else if (left && center && right) {
        lostTime = 0;
        controlMotor(BASE_SPEED, true, BASE_SPEED, true);
    }
    else {
        if (lostTime == 0) lostTime = millis();
        unsigned long lostDuration = millis() - lostTime;

        if (lostDuration < 300) {
            controlMotor(100, false, 100, false);
        } else if (lostDuration < 1000) {
            if (lastTurnedLeft) {
                controlMotor(100, false, 100, true);
            } else {
                controlMotor(100, true, 100, false);
            }
        } else {
            brakeMotors();
        }
    }
}

void optimisedAngle() {
    webLog("Evaluating obstacle clearance angles...", true);
    
    int clearDistThreshold = 50;
    int leftClearanceAngle = -1;
    int rightClearanceAngle = -1;

    // 1. Slow Scan Left (90 to 180)
    for (int ang = 90; ang <= 180; ang += 15) {
        slowServoMove(ang);
        if (!safeDelay(150, false)) return;
        
        int dist = getDistance();
        if (dist > clearDistThreshold) {
            leftClearanceAngle = ang;
            break;
        }
    }

    slowServoMove(90);
    if (!safeDelay(200, false)) return;

    // 2. Slow Scan Right (90 to 0)
    for (int ang = 90; ang >= 0; ang -= 15) {
        slowServoMove(ang);
        if (!safeDelay(150, false)) return;
        
        int dist = getDistance();
        if (dist > clearDistThreshold) {
            rightClearanceAngle = ang;
            break;
        }
    }

    slowServoMove(90);
    if (!safeDelay(200, false)) return;

    bool goLeft = true;
    float targetTurnAngle = 40.0f; 

    if (leftClearanceAngle != -1 && rightClearanceAngle != -1) {
        if (abs(leftClearanceAngle - 90) <= abs(rightClearanceAngle - 90)) {
            goLeft = true;
            targetTurnAngle = (float)abs(leftClearanceAngle - 90);
        } else {
            goLeft = false;
            targetTurnAngle = (float)abs(rightClearanceAngle - 90);
        }
    } else if (leftClearanceAngle != -1) {
        goLeft = true;
        targetTurnAngle = (float)abs(leftClearanceAngle - 90);
    } else if (rightClearanceAngle != -1) {
        goLeft = false;
        targetTurnAngle = (float)abs(rightClearanceAngle - 90);
    }

    if (targetTurnAngle < 40.0f) targetTurnAngle = 40.0f;

    webLog("DYNAMIC BYPASS: Dir = " + String(goLeft ? "LEFT" : "RIGHT") + " | Angle = " + String(targetTurnAngle, 1), true);

    if (!turnExactAngle(targetTurnAngle, !goLeft)) return;

    int obstacleSideAngle = goLeft ? 0 : 180;
    slowServoMove(obstacleSideAngle);
    safeDelay(200, false);

    webLog("Bypass: Driving Leg 1...", true);
    controlMotor(BASE_SPEED, true, BASE_SPEED, true);

    unsigned long leg1Start = millis();
    int clearCount = 0;
    int startMode = currentMode;

    while (millis() - leg1Start < 6000) {
        server.handleClient();
        webSocket.loop();
        if (currentMode != startMode) { brakeMotors(); return; }

        if (getDistance() > 50) {
            clearCount++;
            if (clearCount >= 2) break;
        } else {
            clearCount = 0;
        }
        delay(30);
    }

    safeDelay(400, false);
    unsigned long leg1Duration = millis() - leg1Start;

    webLog("Bypass: Turning inward...", true);
    if (!turnExactAngle(2.0f * targetTurnAngle, goLeft)) return;

    slowServoMove(90);
    safeDelay(100, false);

    webLog("Bypass: Driving Leg 2...", true);
    controlMotor(BASE_SPEED, true, BASE_SPEED, true);
    if (!safeDelay(leg1Duration, false)) return;

    if (!turnExactAngle(targetTurnAngle, !goLeft)) return;

    brakeMotors();
    webLog("BYPASS COMPLETE.", true);
}

// RESTORED mrt_elec function to fix compilation scope error
void mrt_elec() {
    int currentDistance = getDistance();
    if (currentDistance > 60) {
        setLEDs(0, 255, 0); // Green
        controlMotor(220, true, 220, true);
    } 
    else if (currentDistance >= 30 && currentDistance <= 60) {
        setLEDs(255, 255, 0); // Yellow
        controlMotor(180, true, 180, true);
    } 
    else {
        setLEDs(255, 0, 0); // Red
        brakeMotors();
        optimisedAngle();
    }
    
    safeDelay(50, false);
}

void tl() {}

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Pico W Controller Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: monospace; background: #121212; color: #00ff66; margin: 0; padding: 20px; }
        .card { background: #1e1e1e; border: 1px solid #333; padding: 15px; border-radius: 8px; margin-bottom: 15px; }
        h2 { margin-top: 0; color: #fff; }
        select, button { background: #2b2b2b; color: #00ff66; border: 1px solid #00ff66; padding: 10px; font-size: 16px; border-radius: 4px; }
        #console { height: 250px; background: #000; padding: 10px; overflow-y: scroll; border: 1px solid #333; font-size: 13px; white-space: pre-wrap; }
        .badge { background: #00ff66; color: #000; padding: 2px 8px; border-radius: 4px; font-weight: bold; }
    </style>
</head>
<body>
    <div class="card">
        <h2>Connected Devices: <span id="device-count" class="badge">0</span></h2>
    </div>
    <div class="card">
        <h2>Mode Selection</h2>
        <select id="modeSelect">
            <option value="0" selected>Mode 0: Idle</option>
            <option value="1">Mode 1: Remote Control</option>
            <option value="2">Mode 2: IR Mode</option>
            <option value="3">Mode 3: MRT Electronics</option>
            <option value="4">Mode 4: MRT Mechatronics</option>
            <option value="5">Mode 5: AUV TCP</option>
            <option value="6">Mode 6: TL Maze</option>
        </select>
        <button onclick="sendMode()">Set Mode</button>
        <p>Active Mode: <span id="currentModeDisplay">0</span></p>
    </div>
    <div class="card">
        <h2>Live System Output</h2>
        <div id="console"></div>
    </div>
    <script>
        var socket = new WebSocket('ws://' + window.location.hostname + ':6968/');
        socket.onmessage = function(event) {
            var consoleElem = document.getElementById('console');
            if (event.data.startsWith("MODE_ACK:")) {
                var activeMode = event.data.split(":")[1];
                document.getElementById('currentModeDisplay').innerText = activeMode;
                document.getElementById('modeSelect').value = activeMode;
            } else if (event.data.startsWith("CLIENT_COUNT:")) {
                document.getElementById('device-count').innerText = event.data.split(":")[1];
            } else {
                consoleElem.innerHTML += event.data + "<br>";
                consoleElem.scrollTop = consoleElem.scrollHeight;
            }
        };
        function sendMode() {
            var val = document.getElementById('modeSelect').value;
            socket.send("SET_MODE:" + val);
        }
    </script>
</body>
</html>
)rawliteral";

void resetStateVariables() {
    brakeMotors();
    idleLogged = false;
    prevStepTime = millis();
    isRemoteConnected = false;
    if (tcpClient.connected()) tcpClient.stop();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
        webSocket.sendTXT(num, "MODE_ACK:" + String(currentMode));
        webSocket.sendTXT(num, "Connected to Pico W Control Board.");
    } else if (type == WStype_TEXT) {
        String message = String((char*)payload, length);
        if (message.startsWith("SET_MODE:")) {
            int newMode = message.substring(9).toInt();
            resetStateVariables();
            currentMode = newMode;
            webLog("Mode changed to: " + String(currentMode), true);
            webSocket.broadcastTXT("MODE_ACK:" + String(currentMode));
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ssid, password);

    udp.begin(udpPort);
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", HTML_INDEX); });
    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Wire.setSDA(16); 
    Wire.setSCL(17); 
    Wire.begin();
    Wire.setTimeout(50, true);
    
    if (!mpu.begin()) {
        mpuInitialized = false;
    } else {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpuInitialized = true;
        calibrateGyro();
    }

    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);

    // pinMode(ir1, INPUT);
    // pinMode(ir2, INPUT);
    // pinMode(ir3, INPUT);

    pinMode(us_trigger, OUTPUT);
    pinMode(us_echo, INPUT);
    
    pinMode(ledRed, OUTPUT); 
    pinMode(ledGreen, OUTPUT);
    pinMode(ledBlue, OUTPUT);

    scanServo.attach(servo);
    slowServoMove(90);
}

void loop() {
    server.handleClient();
    webSocket.loop();
    
    switch (currentMode) {
        case 0: idleMode(); break;
        case 1: remoteControl(); break;
        case 2: ir_mode(); break;
        case 3: mrt_elec(); break;
        case 4: mrt_mech(); break;
        case 5: auv_elec(); break;
        case 6: tl(); break;
        default: idleMode(); break;
    }
}