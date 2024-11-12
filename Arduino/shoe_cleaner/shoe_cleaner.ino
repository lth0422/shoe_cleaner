#include <Servo.h>
#include <NewPing.h>

// 핀 설정
#define SERVO_SWING 4  // 스윙암 조절 서보모터
#define SERVO_BRUSH_1 5  // 측면 브러시 1 조절 서보모터
#define SERVO_BRUSH_2 6  // 측면 브러시 2 조절 서보모터
// DC 모터 1 (측면)
#define DC_SIDE_1_IN1 7
#define DC_SIDE_1_IN2 8
#define DC_SIDE_1_PWM 12

// DC 모터 2 (측면)
#define DC_SIDE_2_IN1 22
#define DC_SIDE_2_IN2 23
#define DC_SIDE_2_PWM 13

// DC 모터 3 (하단)
#define DC_BOTTOM_IN1 24
#define DC_BOTTOM_IN2 25
#define DC_BOTTOM_PWM 11
#define TRIG_PIN 26  // 초음파 센서 TRIG 핀 번호 
#define ECHO_PIN 27  // 초음파 센서 ECHO 핀 번호 

// 상수 설정
int pos = 0; // 서보모터 위치
bool isRunning = true; // 모터 작동 여부

const unsigned long CLEANING_TIME = 60000; // 청소 시간 1분
const int SWING_UP = 0;    // 스윙암 올림 각도
const int SWING_DOWN = 90; // 스윙암 내림 각도

const unsigned long NORMAL_CLEANING_TIME = 180000; // 일반 모드 3분
const unsigned long QUICK_CLEANING_TIME = 60000;   // 쾌속 모드 1분
const int NORMAL_SPEED = 180;  // 일반 모드 속도
const int QUICK_SPEED = 255;   // 쾌속 모드 속도

// 청소 모드 추가
enum CleaningMode {
  NORMAL,
  QUICK
} currentMode = NORMAL;

unsigned long cleaningStartTime = 0;  // 청소 시작 시간
unsigned long cleaningDuration = NORMAL_CLEANING_TIME;  // 현재 설정된 청소 시간

// 객체 생성
Servo swingArm;
Servo sideBrush1;
Servo sideBrush2;
NewPing sonar(TRIG_PIN, ECHO_PIN, 200); // 최대 200cm

// 상태 변수
enum CleaningState {
  IDLE,           // 초기 상태
  ARM_UP,         // 스윙암 올라간 상태
  MODE_SELECT,    // 모드 선택 대기 상태
  MEASURING,      // 신발 사이즈 측정 상태
  BRUSH_ADJUST,   // 브러시 조정 상태
  CLEANING,       // 청소 중 상태
  FINISHING       // 청소 완료 상태
} currentState = IDLE;

// 블루투스 통신을 위한 변수
const int COMMAND_LENGTH = 5;
int commands[COMMAND_LENGTH];  // [POWER_OFF, NORMAL_MODE, QUICK_MODE, SWING_UP, SWING_DOWN]

void setup() {
  // 초기화 코드
  Serial.begin(9600); // 시리얼 통신을 위한 설정
  swingArm.attach(SERVO_SWING);
  sideBrush1.attach(SERVO_BRUSH_1);
  sideBrush2.attach(SERVO_BRUSH_2);
  pinMode(DC_SIDE_1_IN1, OUTPUT);
  pinMode(DC_SIDE_1_IN2, OUTPUT);
  pinMode(DC_SIDE_2_IN1, OUTPUT);
  pinMode(DC_SIDE_2_IN2, OUTPUT);
  pinMode(DC_BOTTOM_IN1, OUTPUT);
  pinMode(DC_BOTTOM_IN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  // 시리얼 통신으로 명령어 수신
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    parseCommand(input);
    
    // 명령어 처리
    if (commands[0] == 1) {  // POWER_OFF
      isRunning = false;
      resetPosition();
      stopCleaning();
      Serial.println("Power off command received");
      return;
    }

    if (isRunning) {
      switch (currentState) {
        case IDLE:
          if (commands[3] == 1) {  // SWING_UP
            Serial.println("Swing arm up command received");
            swingArmUp();
            currentState = ARM_UP;
          }
          break;

        case ARM_UP:
          if (commands[1] == 1) {  // NORMAL_MODE
            Serial.println("Normal mode command received");
            currentMode = NORMAL;
            cleaningDuration = NORMAL_CLEANING_TIME;
            swingArmDown();
            currentState = MEASURING;
          }
          else if (commands[2] == 1) {  // QUICK_MODE
            Serial.println("Quick mode command received");
            currentMode = QUICK;
            cleaningDuration = QUICK_CLEANING_TIME;
            swingArmDown();
            currentState = MEASURING;
          }
          break;

      case MEASURING:
        Serial.println("Measuring shoe size");
        measureAndAdjust();
        break;

      case CLEANING:
        if (millis() - cleaningStartTime >= cleaningDuration) {
          Serial.println("Cleaning complete, transitioning to finishing state");
          currentState = FINISHING;
          swingArmUp();
        }
        break;

      case FINISHING:
        Serial.println("Finishing state, waiting for reset");
        if (commands[4] ==1) {
          swingArmDown();
          currentState = IDLE;
          delay(200);
        }
        break;
     }
    }
  }
}

// 블루투스 명령어 파싱 함수
void parseCommand(String input) {
  int index = 0;
  int prevIndex = 0;
  
  // 쉼표로 구분된 명령어 파싱
  for (int i = 0; i < COMMAND_LENGTH; i++) {
    index = input.indexOf(',', prevIndex);
    if (index == -1) {
      commands[i] = input.substring(prevIndex).toInt();
      break;
    }
    commands[i] = input.substring(prevIndex, index).toInt();
    prevIndex = index + 1;
  }
  Serial.print("Commands received: ");
  for (int i = 0; i < COMMAND_LENGTH; i++) {
    Serial.print(commands[i]);
    Serial.print(" ");
  }
  Serial.println();
}

// 신발 사이즈 측정 및 브러시 조정 함수
void measureAndAdjust() {
  int distance = sonar.ping_cm();
  if (distance > 0 && distance <= 200) {  // 유효한 거리 범위 내에서만 처리
    Serial.print("Shoe size measured: ");
    Serial.print(distance);
    Serial.println(" cm");

    adjustBrushes(distance);
    startCleaning();
    cleaningStartTime = millis();
    currentState = CLEANING;
  }
}

// 브러시 조정 함수
void adjustBrushes(int distance) {
  int brushAngle;
  if (distance < 50) {
    brushAngle = 30;
  } else if (distance < 100) {
    brushAngle = 60;
  } else {
    brushAngle = 90;
  }
  
  sideBrush1.write(brushAngle);
  sideBrush2.write(180 - brushAngle);
  Serial.print("Brushes adjusted to angle: ");
  Serial.println(brushAngle);
  delay(1000);
}

// 청소 시작 함수
void startCleaning() {
  int motorSpeed = (currentMode == NORMAL) ? NORMAL_SPEED : QUICK_SPEED;
  
  digitalWrite(DC_SIDE_1_IN1, HIGH);
  digitalWrite(DC_SIDE_1_IN2, LOW);
  digitalWrite(DC_SIDE_2_IN1, HIGH);
  digitalWrite(DC_SIDE_2_IN2, LOW);
  digitalWrite(DC_BOTTOM_IN1, HIGH);
  digitalWrite(DC_BOTTOM_IN2, LOW);
  
  analogWrite(DC_SIDE_1_PWM, motorSpeed);
  analogWrite(DC_SIDE_2_PWM, motorSpeed);
  analogWrite(DC_BOTTOM_PWM, motorSpeed);
  
  Serial.println(currentMode == NORMAL ? "Normal cleaning mode started" : "Quick cleaning mode started");
}

void swingArmUp() {
  swingArm.write(SWING_UP);
  delay(1000);
  Serial.println("Swing arm moved up");
  currentState = ARM_UP;
}

void swingArmDown() {
  swingArm.write(SWING_DOWN);
  delay(1000);
  Serial.println("Swing arm moved down");
}

void stopCleaning() {
  digitalWrite(DC_SIDE_1_IN1, LOW);
  digitalWrite(DC_SIDE_1_IN2, LOW);
  digitalWrite(DC_SIDE_2_IN1, LOW);
  digitalWrite(DC_SIDE_2_IN2, LOW);
  digitalWrite(DC_BOTTOM_IN1, LOW);
  digitalWrite(DC_BOTTOM_IN2, LOW);
  analogWrite(DC_SIDE_1_PWM, 0);
  analogWrite(DC_SIDE_2_PWM, 0);
  analogWrite(DC_BOTTOM_PWM, 0);
  Serial.println("Motors stopped");
}

void resetPosition() {
  sideBrush1.write(0);
  sideBrush2.write(0);
  swingArm.write(SWING_DOWN);
  delay(1000);
  Serial.println("Reset to initial position");
  currentState = IDLE;
}
