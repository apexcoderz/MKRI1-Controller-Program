/**
 * @file MKRI1 Controller Unit.ino
 * @brief Mini KRI Microcontroller Unit Programs
 * @details --- UPDATED SOON (MALES) ---
 * @version 2.84
 * @note Designed for ESP32 microcontroller with Arduino framework compatibility
 * @warning NON-critical code - modifications aren't require formal validation
 */

#include "BluetoothSerial.h"
#include <ESP32Servo.h>
#include <Preferences.h>
#include <stdint.h>

BluetoothSerial SerialBT;
Servo servoArm;
Servo servoGripper;
Preferences preferences;

static const uint8_t PIN_LED_STATUS = 2;
static const uint8_t PIN_IR_SENSOR  = 35;
static const uint8_t PIN_ENA = 25;
static const uint8_t PIN_IN1 = 26;
static const uint8_t PIN_IN2 = 27;
static const uint8_t PIN_IN3 = 23;
static const uint8_t PIN_IN4 = 22;
static const uint8_t PIN_ENB = 21;
static const uint8_t PIN_SERVO_ARM     = 19;
static const uint8_t PIN_SERVO_GRIPPER = 18;

String inputBuffer = "";
uint8_t SPEED_NORMAL = 180;
static const uint8_t SPEED_BOOST  = 255;
static uint8_t currentSpeed       = SPEED_NORMAL;
static const uint8_t ARM_MIN_ANGLE = 10;
static const uint8_t ARM_MAX_ANGLE = 150;
static const uint8_t GRIPPER_OPEN_ANGLE  = 90;
static const uint8_t GRIPPER_CLOSE_ANGLE = 10;
static uint8_t armPos     = 90;
static uint8_t gripperPos = 90;
static bool isArmMovingUp    = false;
static bool isArmMovingDown  = false;
static bool isGripperOpening = false;
static bool isGripperClosing = false;
static unsigned long lastServoUpdate = 0;
uint8_t SERVO_DELAY_MS = 15;

void processCommand(char cmd);
void setMotor(int8_t leftDir, int8_t rightDir, uint8_t leftSpd, uint8_t rightSpd);
void updateServos(void);
void checkIRSensor(void);
void parseSettingCommand(String cmdString);

void setup() {
    Serial.begin(115200);
    preferences.begin("robot_cfg", false);

    SPEED_NORMAL   = preferences.getUChar("motor_speed", 180);
    SERVO_DELAY_MS = preferences.getUChar("arm_speed", 15);

    if (SPEED_NORMAL < 50)  { SPEED_NORMAL = 180;  preferences.putUChar("motor_speed", SPEED_NORMAL); }
    if (SERVO_DELAY_MS < 1) { SERVO_DELAY_MS = 15; preferences.putUChar("arm_speed", SERVO_DELAY_MS); }

    currentSpeed = SPEED_NORMAL;

    Serial.println("=== MKRI1 v2.83 ===");
    Serial.print("Motor: "); Serial.print(SPEED_NORMAL);
    Serial.print(" | ServoMS: "); Serial.println(SERVO_DELAY_MS);

    pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
    pinMode(PIN_ENA, OUTPUT); pinMode(PIN_ENB, OUTPUT);
    setMotor(0, 0, 0, 0);

    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_IR_SENSOR, INPUT);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    servoArm.setPeriodHertz(50);
    servoGripper.setPeriodHertz(50);
    servoArm.attach(PIN_SERVO_ARM, 500, 2400);
    servoGripper.attach(PIN_SERVO_GRIPPER, 500, 2400);
    servoArm.write(armPos);
    servoGripper.write(gripperPos);

    if (!SerialBT.begin("LahBaKosan MKRI1")) {
        Serial.println("BT_ERR");
    } else {
        Serial.println("BT_READY!");
        digitalWrite(PIN_LED_STATUS, HIGH);
    }
}

void loop() {
    while (SerialBT.available() > 0) {
        char c = (char)SerialBT.read();
        if      (c == 'F') processCommand(c);
        else if (c == 'B') processCommand(c);
        else if (c == 'L') processCommand(c);
        else if (c == 'R') processCommand(c);
        else if (c == 'S') processCommand(c);
        else if (c == '3') processCommand(c);
        else if (c == '4') processCommand(c);
        else if (c == '5') processCommand(c);
        else if (c == '6') processCommand(c);
        else if (c == 'U') processCommand(c);
        else if (c == 'D') processCommand(c);
        else if (c == 'd') processCommand(c);
        else if (c == 'O') processCommand(c);
        else if (c == 'C') processCommand(c);
        else if (c == 'c') processCommand(c);
        else if (c == 'X') processCommand(c);
        else if (c == 'x') processCommand(c);
        else if (c == '1') processCommand(c);
        else if (c == '2') processCommand(c);
        else if (c == '\n') { parseSettingCommand(inputBuffer); inputBuffer = ""; }
        else {
            inputBuffer += c;
            if (inputBuffer.length() > 30) inputBuffer = "";
        }
    }
    updateServos();
    checkIRSensor();
}

void processCommand(char cmd) {
    Serial.print("CMD: "); Serial.println(cmd);

    uint8_t slowSpd = (uint8_t)((uint16_t)currentSpeed * 3 / 10);

    switch (cmd) {
        case 'X': currentSpeed = SPEED_BOOST;  break;
        case 'x': currentSpeed = SPEED_NORMAL; break;
        case '1': setMotor(0, 0, 0, 0); Serial.println("BRAKE"); break;
        case '2': Serial.println("BRAKE OFF"); break;

        case 'F': setMotor(-1,  1, currentSpeed, currentSpeed); break;
        case 'B': setMotor( 1, -1, currentSpeed, currentSpeed); break;
        case 'L': setMotor(-1, -1, currentSpeed, currentSpeed); break;
        case 'R': setMotor( 1,  1, currentSpeed, currentSpeed); break;
        case 'S': setMotor( 0,  0, 0, 0); break;

        case '4': setMotor(-1,  1, currentSpeed, slowSpd); Serial.println("FWD-RIGHT"); break;
        case '3': setMotor(-1,  1, slowSpd, currentSpeed); Serial.println("FWD-LEFT");  break;
        case '6': setMotor( 1, -1, slowSpd, currentSpeed); Serial.println("BWD-LEFT");  break;
        case '5': setMotor( 1, -1, currentSpeed, slowSpd); Serial.println("BWD-RIGHT"); break;

        case 'U': isArmMovingUp = true;  isArmMovingDown = false; break;
        case 'D': isArmMovingDown = true; isArmMovingUp = false;  break;
        case 'd': isArmMovingUp = false;  isArmMovingDown = false; break;
        case 'O': isGripperOpening = true;  isGripperClosing = false; break;
        case 'C': isGripperClosing = true;  isGripperOpening = false; break;
        case 'c': isGripperOpening = false; isGripperClosing = false; break;

        default: break;
    }
}

void setMotor(int8_t leftDir, int8_t rightDir, uint8_t leftSpd, uint8_t rightSpd) {
    if (leftDir == 1) {
        digitalWrite(PIN_IN1, HIGH);
        digitalWrite(PIN_IN2, LOW);
    } else if (leftDir == -1) {
        digitalWrite(PIN_IN1, LOW);
        digitalWrite(PIN_IN2, HIGH);
    } else {
        digitalWrite(PIN_IN1, LOW);
        digitalWrite(PIN_IN2, LOW);
    }

    if (rightDir == 1) {
        digitalWrite(PIN_IN3, HIGH);
        digitalWrite(PIN_IN4, LOW);
    } else if (rightDir == -1) {
        digitalWrite(PIN_IN3, LOW);
        digitalWrite(PIN_IN4, HIGH);
    } else {
        digitalWrite(PIN_IN3, LOW);
        digitalWrite(PIN_IN4, LOW);
    }

    analogWrite(PIN_ENA, (leftDir  == 0) ? 0 : leftSpd);
    analogWrite(PIN_ENB, (rightDir == 0) ? 0 : rightSpd);

    Serial.print("  ENA="); Serial.print((leftDir  == 0) ? 0 : leftSpd);
    Serial.print(" ENB="); Serial.println((rightDir == 0) ? 0 : rightSpd);
}

void updateServos(void) {
    unsigned long now = millis();
    if (now - lastServoUpdate >= SERVO_DELAY_MS) {
        lastServoUpdate = now;
        if      (isArmMovingUp   && armPos < ARM_MAX_ANGLE)            { armPos++;     servoArm.write(armPos); }
        else if (isArmMovingDown && armPos > ARM_MIN_ANGLE)            { armPos--;     servoArm.write(armPos); }
        if      (isGripperOpening && gripperPos < GRIPPER_OPEN_ANGLE)  { gripperPos++; servoGripper.write(gripperPos); }
        else if (isGripperClosing && gripperPos > GRIPPER_CLOSE_ANGLE) { gripperPos--; servoGripper.write(gripperPos); }
    }
}

void checkIRSensor(void) {
    if (digitalRead(PIN_IR_SENSOR) == LOW) {
        digitalWrite(PIN_LED_STATUS, LOW);
        if (isGripperClosing) isGripperClosing = false;
    } else {
        digitalWrite(PIN_LED_STATUS, HIGH);
    }
}

void parseSettingCommand(String cmdString) {
    cmdString.trim();
    if (cmdString.startsWith("MVal:")) {
        int v = cmdString.substring(5).toInt();
        if (v >= 0 && v <= 255) { SPEED_NORMAL = v; currentSpeed = v; Serial.print("TEMP_MOTOR: "); Serial.println(v); }
    } else if (cmdString.startsWith("AVal:")) {
        int v = cmdString.substring(5).toInt();
        if (v >= 1 && v <= 100) { SERVO_DELAY_MS = v; Serial.print("TEMP_ARM: "); Serial.println(v); }
    } else if (cmdString.startsWith("UMval:")) {
        int v = cmdString.substring(6).toInt();
        if (v >= 0 && v <= 255) { SPEED_NORMAL = v; currentSpeed = v; preferences.putUChar("motor_speed", v); Serial.print("SAVED_MOTOR: "); Serial.println(v); }
    } else if (cmdString.startsWith("UAVal:")) {
        int v = cmdString.substring(6).toInt();
        if (v >= 1 && v <= 100) { SERVO_DELAY_MS = v; preferences.putUChar("arm_speed", v); Serial.print("SAVED_ARM: "); Serial.println(v); }
    }
}
