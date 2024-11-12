#include <Servo.h>
#include <NewPing.h>

// 핀 설정
//일단 버튼을 기준으로 작성했음. app으로 할 지는 이번주안에 결정
#define SWING_ARM_BTN 2     // 버튼 1: 스윙암 올리기 버튼
#define NORMAL_MODE_BTN 3   // 버튼 2: 일반모드 버튼
#define QUICK_MODE_BTN 9    // 버튼 3: 쾌속모드 버튼
#define SWING_DOWN_BTN 10   // 버튼 4: 스윙암 내리기 버튼
#define POWER_BTN 14        // 버튼 5: 전원 off 버튼

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
  FINISHING      // 청소 완료 상태
} currentState = IDLE;

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
  pinMode(SWING_ARM_BTN, INPUT);
  pinMode(NORMAL_MODE_BTN, INPUT);
  pinMode(QUICK_MODE_BTN, INPUT);
  pinMode(POWER_BTN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  // 전원 버튼 처리
  if (digitalRead(POWER_BTN) == HIGH) {
    isRunning = false;
    stopAllMotors();
    Serial.println("Power button pressed. Shutting down.");
    return;
  }

  if (isRunning) {
    switch (currentState) {
      case IDLE:
        // 버튼 1: 스윙암 올리기
        if (digitalRead(SWING_ARM_BTN) == HIGH) {
          swingArmUp();
          currentState = ARM_UP;
          delay(200);
        }
        break;

      case ARM_UP:
        // 모드 선택 대기
        if (digitalRead(NORMAL_MODE_BTN) == HIGH) {
          currentMode = NORMAL;
          cleaningDuration = NORMAL_CLEANING_TIME;
          swingArmDown();
          currentState = MEASURING;
          delay(200);
        }
        else if (digitalRead(QUICK_MODE_BTN) == HIGH) {
          currentMode = QUICK;
          cleaningDuration = QUICK_CLEANING_TIME;
          swingArmDown();
          currentState = MEASURING;
          delay(200);
        }
        break;

      case MEASURING:
        measureAndAdjust();
        break;

      case CLEANING:
        if (millis() - cleaningStartTime >= cleaningDuration) {
          currentState = FINISHING;
          swingArmUp();
        }
        break;

      case FINISHING:
        if (digitalRead(SWING_DOWN_BTN) == HIGH) {
          swingArmDown();
          currentState = IDLE;
          delay(200);
        }
        break;
    }
  }
}

// 신발 사이즈 측정 및 브러시 조정 함수
void measureAndAdjust() {
  int distance = sonar.ping_cm();
  if (distance > 0 && distance <= 200) {  // 유효한 거리 범위 내에서만 처리
    adjustBrushes(distance);
    startCleaning();
    cleaningStartTime = millis();
    currentState = CLEANING;
  }
}

// 브러시 조정 함수
void adjustBrushes(int distance) {
  // 거리에 따른 브러시 각도 매핑
  // 예: 거리가 가까울수록(신발이 작을수록) 브러시 간격을 좁게
  int brushAngle;
  if (distance < 50) {  // 작은 신발
    brushAngle = 30;
  } else if (distance < 100) {  // 중간 크기 신발
    brushAngle = 60;
  } else {  // 큰 신발
    brushAngle = 90;
  }
  
  sideBrush1.write(brushAngle);
  sideBrush2.write(180 - brushAngle);  // 반대쪽 브러시는 반대 방향으로
  delay(1000);  // 브러시 조정 시간
}

// 청소 시작 함수
void startCleaning() {
  int motorSpeed = (currentMode == NORMAL) ? NORMAL_SPEED : QUICK_SPEED;
  
  // 모든 DC 모터 작동
  digitalWrite(DC_SIDE_1_IN1, HIGH);
  digitalWrite(DC_SIDE_1_IN2, LOW);
  digitalWrite(DC_SIDE_2_IN1, HIGH);
  digitalWrite(DC_SIDE_2_IN2, LOW);
  digitalWrite(DC_BOTTOM_IN1, HIGH);
  digitalWrite(DC_BOTTOM_IN2, LOW);
  
  analogWrite(DC_SIDE_1_PWM, motorSpeed);
  analogWrite(DC_SIDE_2_PWM, motorSpeed);
  analogWrite(DC_BOTTOM_PWM, motorSpeed);
  
  Serial.println(currentMode == NORMAL ? "일반 모드 시작" : "쾌속 모드 시작");
}

void swingArmUp() {
  // 스윙암 올리기
  swingArm.write(SWING_UP);
  delay(1000); // 1초 대기
  currentState = WAITING;
}

void swingArmDown() {
  swingArm.write(SWING_DOWN);
  delay(1000); // 1초 대기
}

void stopCleaning() {
  // 모든 DC 모터 정지
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
  // 초기 위치로 복귀
  sideBrush1.write(0);
  sideBrush2.write(0);
  swingArm.write(SWING_DOWN);
  delay(1000);
  currentState = WAITING;
}


