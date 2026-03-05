/**
 * @file MKRI1 Controller Unit.ino
 * @brief Mini KRI Microcontroller Unit Programs
 * @details --- UPDATED SOON (MALES) ---
 * @version 2.80
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
static const uint8_t PIN_IR_SENSOR  = 34; 
static const uint8_t PIN_ENA = 13; 
static const uint8_t PIN_IN1 = 12; 
static const uint8_t PIN_IN2 = 14; 
static const uint8_t PIN_IN3 = 27; 
static const uint8_t PIN_IN4 = 26; 
static const uint8_t PIN_ENB = 25; 
static const uint8_t PIN_SERVO_ARM     = 33;
static const uint8_t PIN_SERVO_GRIPPER = 32;

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
static bool isArmMovingUp   = false;
static bool isArmMovingDown = false;
static bool isGripperOpening= false;
static bool isGripperClosing= false;
static unsigned long lastServoUpdate = 0;
uint8_t SERVO_DELAY_MS  = 15; 

void processCommand(uint8_t cmd);
void controlMotor(int8_t leftDir, int8_t rightDir, uint8_t speed);
void updateServos(void);
void checkIRSensor(void);
void parseSettingCommand(String cmdString);

void setup() {
    Serial.begin(115200);
    preferences.begin("robot_cfg", false); 
    
    SPEED_NORMAL   = preferences.getUChar("motor_speed", 180);
    SERVO_DELAY_MS = preferences.getUChar("arm_speed", 15);

    if (SPEED_NORMAL < 50) { 
        SPEED_NORMAL = 180;  
        preferences.putUChar("motor_speed", SPEED_NORMAL); 
    }
    if (SERVO_DELAY_MS < 1) { 
        SERVO_DELAY_MS = 15; 
        preferences.putUChar("arm_speed", SERVO_DELAY_MS);
    }
    
    currentSpeed = SPEED_NORMAL;

    Serial.println("=== LahBaKoSan MKRI1 INITIATED ===");
    Serial.print(" MEMORY READED -> Motor: "); Serial.print(SPEED_NORMAL);
    Serial.print(" | Servo MS: "); Serial.println(SERVO_DELAY_MS);

    pinMode(PIN_ENA, OUTPUT);
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_IN3, OUTPUT);
    pinMode(PIN_IN4, OUTPUT);
    pinMode(PIN_ENB, OUTPUT);
    controlMotor(0, 0, 0);

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
        Serial.println("BT_READY! Waitin For Connection");
        digitalWrite(PIN_LED_STATUS, HIGH); 
    }
}

void loop() {
    while (SerialBT.available() > 0) {
        char c = (char)SerialBT.read();

        if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S' ||
            c == 'U' || c == 'D' || c == 'd' || c == 'O' || c == 'C' || c == 'c' ||
            c == 'X' || c == 'x') {
            processCommand(c);
        }
        else if (c == '\n') {
            parseSettingCommand(inputBuffer);
            inputBuffer = ""; 
        }
        else {
            inputBuffer += c;
            if (inputBuffer.length() > 30) {
                inputBuffer = ""; 
            }
        }
    }
    updateServos();
    checkIRSensor();
}

void processCommand(uint8_t cmd) {
    Serial.print("Button Pressed:"); 
    Serial.println((char)cmd); 
    
    switch (cmd) {
        case 'F': controlMotor( 1,  1, currentSpeed); break;
        case 'B': controlMotor(-1, -1, currentSpeed); break;
        case 'L': controlMotor(-1,  1, currentSpeed); break; 
        case 'R': controlMotor( 1, -1, currentSpeed); break; 
        case 'S': controlMotor( 0,  0, 0); break;            

        case 'U': isArmMovingUp = true;  isArmMovingDown = false; break;
        case 'D': isArmMovingDown = true; isArmMovingUp = false; break;
        case 'd': isArmMovingUp = false; isArmMovingDown = false; break;
        
        case 'O': isGripperOpening = true; isGripperClosing = false; break;
        case 'C': isGripperClosing = true; isGripperOpening = false; break;
        case 'c': isGripperOpening = false; isGripperClosing = false; break;

        case 'X': currentSpeed = SPEED_BOOST; break;
        case 'x': currentSpeed = SPEED_NORMAL; break;

        default: break;
    }
}

void controlMotor(int8_t leftDir, int8_t rightDir, uint8_t speed) {
    if (leftDir == 1) { digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW); } 
    else if (leftDir == -1) { digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH); } 
    else { digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, HIGH); }
    analogWrite(PIN_ENA, (leftDir == 0) ? 0 : speed);

    if (rightDir == 1) { digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW); } 
    else if (rightDir == -1) { digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, HIGH); } 
    else { digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, HIGH); }
    analogWrite(PIN_ENB, (rightDir == 0) ? 0 : speed);
}

void updateServos(void) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastServoUpdate >= SERVO_DELAY_MS) {
        lastServoUpdate = currentMillis;

        if (isArmMovingUp && armPos < ARM_MAX_ANGLE) { armPos++; servoArm.write(armPos); } 
        else if (isArmMovingDown && armPos > ARM_MIN_ANGLE) { armPos--; servoArm.write(armPos); }

        if (isGripperOpening && gripperPos < GRIPPER_OPEN_ANGLE) { gripperPos++; servoGripper.write(gripperPos); } 
        else if (isGripperClosing && gripperPos > GRIPPER_CLOSE_ANGLE) { gripperPos--; servoGripper.write(gripperPos); }
    }
}

void checkIRSensor(void) {
    uint8_t irState = digitalRead(PIN_IR_SENSOR);
    if (irState == LOW) {
        digitalWrite(PIN_LED_STATUS, LOW); 
        if (isGripperClosing) isGripperClosing = false;
    } else {
        digitalWrite(PIN_LED_STATUS, HIGH); 
    }
}

void parseSettingCommand(String cmdString) {
    cmdString.trim(); 

    if (cmdString.startsWith("MVal:")) {
        int speed = cmdString.substring(5).toInt();
        if (speed >= 0 && speed <= 255) {
            SPEED_NORMAL = speed;
            currentSpeed = speed; 
            Serial.print("TEMP_MOTOR_SPEED: "); Serial.println(SPEED_NORMAL);
        }
    }
    else if (cmdString.startsWith("AVal:")) {
        int delayMs = cmdString.substring(5).toInt();
        if (delayMs >= 1 && delayMs <= 100) {
            SERVO_DELAY_MS = delayMs;
            Serial.print("TEMP_ARM_SPEED: "); Serial.println(SERVO_DELAY_MS);
        }
    }
    else if (cmdString.startsWith("UMval:")) {
        int speed = cmdString.substring(6).toInt();
        if (speed >= 0 && speed <= 255) {
            SPEED_NORMAL = speed;
            currentSpeed = speed; 
            preferences.putUChar("motor_speed", SPEED_NORMAL);
            Serial.print("SAVED_MOTOR_SPEED: "); Serial.println(SPEED_NORMAL);
        }
    }
    else if (cmdString.startsWith("UAVal:")) { 
        int delayMs = cmdString.substring(6).toInt();
        if (delayMs >= 1 && delayMs <= 100) {
            SERVO_DELAY_MS = delayMs;
            preferences.putUChar("arm_speed", SERVO_DELAY_MS); 
            Serial.print("SAVED_ARM_SPEED: "); Serial.println(SERVO_DELAY_MS);
        }
    }
}