#include <Servo.h>
#include <NewPing.h>
#include <Adafruit_NeoPixel.h>

// 핀 설정
#define SERVO_SWING_1 4  // 스윙암 조절 서보모터 1
#define SERVO_SWING_2 9  // 스윙암 조절 서보모터 2
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

// LED 픽셀
#define LED_PIN 3
#define LED_COUNT 24


#define SWING_UP 90    
#define SWING_DOWN 30  
#define SERVO_SPEED 30  // 각도 변화 속도 (값이 작을수록 더 천천히 움직임)
#define SIDE_SERVO_SPEED 50

#define MAX_ANGLE 100
#define MIN_ANGLE 0



// 상수 설정
int pos = 0; // 서보모터 위치
int brushAngle = 0; // 브러시 각도
int lastMeasuredDistance = 0;
bool isRunning = true; // 모터 작동 여부

const unsigned long CLEANING_TIME = 60000; // 청소 시간 1분
//const int SWING_UP = 0;    // 스윙암 올림 각도
//const int SWING_DOWN = 90; // 스윙암 내림 각도

const unsigned long NORMAL_CLEANING_TIME = 30000; // 일반 모드 30s
const unsigned long QUICK_CLEANING_TIME = 15000;   // 쾌속 모드 15s
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
Servo swingArm1;
Servo swingArm2;
Servo sideBrush1;
Servo sideBrush2;
NewPing sonar(TRIG_PIN, ECHO_PIN, 400); // 최대 400cm
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 상태 변수
enum CleaningState {
  IDLE,           // 초기 상태
  ARM_UP,         // 스윙암 올라간 상태
  MODE_SELECT,    // 모드 선택 대기 상태
  CLEANING,       // 청소 중 상태
  FINISHING       // 청소 완료 상태
} currentState = IDLE;

// 블루투스 통신을 위한 변수
const int COMMAND_LENGTH = 5;
int commands[COMMAND_LENGTH];  // [POWER_OFF, NORMAL_MODE, QUICK_MODE, SWING_UP, SWING_DOWN]

// 상태 변수 추가 (파일 상단 변수 선언부에 추가)
bool isSideCleaning = false;
bool isBottomCleaning = false;

void setup() {
  // 초기화 코드
  Serial.begin(9600); // 시리얼 통신을 위한 설정
  strip.begin();
  strip.show(); // 네온픽셀을 위한 설정

  pinMode(DC_SIDE_1_IN1, OUTPUT);
  pinMode(DC_SIDE_1_IN2, OUTPUT);
  pinMode(DC_SIDE_2_IN1, OUTPUT);
  pinMode(DC_SIDE_2_IN2, OUTPUT);
  pinMode(DC_BOTTOM_IN1, OUTPUT);
  pinMode(DC_BOTTOM_IN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // 서보모터 초기화
  swingArm1.detach();  // 혹시 모를 이전 상태를 초기화
  swingArm2.detach();
  sideBrush1.detach();
  sideBrush2.detach();
  
  delay(1000);  // 안정화를 위한 대기

  // 측면 브러시들 중립 위치로
  // sideBrush1.write(20);
  // sideBrush2.write(160);

  swingArm1.attach(SERVO_SWING_1);
  swingArm2.attach(SERVO_SWING_2);
  sideBrush1.attach(SERVO_BRUSH_1);
  sideBrush2.attach(SERVO_BRUSH_2);
  // 시리얼 버퍼 비우기
  while(Serial.available() > 0) {
    Serial.read();
  }

  // 스윙암 초기 위치로 이동 (아래로)
  Serial.println("Initializing swing arm position...");
  swingArm1.write(SWING_DOWN);  // 바로 아래 위치로 이동
  swingArm2.write(145);
  delay(1000);  // 안정화 대기

  // 측면 솔 조절 서보모터 초기 위치로 이동
  sideBrush1.write(10);
  sideBrush2.write(180);
  
  
  Serial.println("Initialization complete!");
}

void loop() {
  // 시리얼 통신으로 명령어 수신
  static int currentAngle = SWING_DOWN;
  
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    parseCommand(input);
    delay(100);    
    // 명령어 처리
    if (commands[0] == 1) {  // POWER_OFF
      //isRunning = false;
      resetPosition();
      stopCleaning();
      Serial.println("Power off command received");
      currentAngle = SWING_DOWN;
      currentState = IDLE;
      return;
    }
  }

    if (isRunning) {
      switch (currentState) {
        case IDLE:
          if (commands[3] == 1) {  // SWING_UP
            swingArm1.attach(SERVO_SWING_1);
            swingArm2.attach(SERVO_SWING_2);
            // swingArm1.write(10);
            // swingArm2.write(70);
            Serial.println("Swing arm up command received");
            moveServoSlowly(currentAngle, SWING_UP);
            currentAngle = SWING_UP;  // 현재 각도 업데이트
            Serial.println("SWING_UP_COMPLETE");
            //swingArmUp();
            currentState = ARM_UP;
          }
          break;

        case ARM_UP:
          if (commands[1] == 1) {  // NORMAL_MODE
            Serial.println("Normal mode command received");
            currentMode = NORMAL;
            cleaningDuration = NORMAL_CLEANING_TIME;
            moveServoSlowly(currentAngle, SWING_DOWN);
            currentAngle = SWING_DOWN;
            delay(500);  // 안정화를 위한 대기
            measureAndAdjust();  // 측정 및 브러시 조정
            delay(1000); // 측정 후 안정화 대기
            isSideCleaning = false;
            isBottomCleaning = false;
            cleaningStartTime = millis();
            currentState = CLEANING;  // 바로 CLEANING 상태로 전환
          }
          else if (commands[2] == 1) {  // QUICK_MODE
            Serial.println("Quick mode command received");
            currentMode = QUICK;
            cleaningDuration = QUICK_CLEANING_TIME;
            moveServoSlowly(currentAngle, SWING_DOWN);
            currentAngle = SWING_DOWN;
            delay(500);  // 안정화를 위한 대기
            measureAndAdjust();  // 측정 및 브러시 조정
            delay(1000);
            isSideCleaning = false;
            isBottomCleaning = false;
            cleaningStartTime = millis();
            currentState = CLEANING;  // 바로 CLEANING 상태로 전환
          }
          break;


        case CLEANING:
          if (millis() - cleaningStartTime < cleaningDuration/2) {
            // 처음 절반 시간동안 side 청소
            if (!isSideCleaning) {
              startCleaning_side();
              isSideCleaning = true;
            }
          } 
          else if (millis() - cleaningStartTime < cleaningDuration) {
            // 나머지 절반 시간동안 bottom 청소
            if (!isBottomCleaning) {
              // side 모터 정지
              analogWrite(DC_SIDE_1_PWM, 0);
              analogWrite(DC_SIDE_2_PWM, 0);
              startCleaning_bottom();
              isBottomCleaning = true;
            }
          }
          else {
            Serial.println("Cleaning complete, transitioning to finishing state");
            analogWrite(DC_SIDE_1_PWM, 0);
            analogWrite(DC_SIDE_2_PWM, 0);
            analogWrite(DC_BOTTOM_PWM, 0);

            // int brushAngle;
            if (lastMeasuredDistance < 3) {
              brushAngle = 105;
            } else {
              brushAngle = 110;
            }
            
            for (int pos = brushAngle; pos > MIN_ANGLE; pos -= 1) {
              sideBrush1.write(pos+10);
              sideBrush2.write(180-pos);
              delay(SIDE_SERVO_SPEED);
            }
            moveServoSlowly(currentAngle, SWING_UP);
            currentAngle = SWING_UP;
            Serial.println("CLEANING_END");
            Serial.println("Finishing state, waiting for reset");
            currentState = FINISHING;
          }
          break;

        case FINISHING:
          if (commands[4] ==1) {
            swingArm1.attach(SERVO_SWING_1);
            swingArm2.attach(SERVO_SWING_2);
            moveServoSlowly(currentAngle, SWING_DOWN);
            currentAngle = SWING_DOWN;  // 현재 각도 업데이트
            Serial.println("SWING_DOWN_COMPLETE");
            commands[4] = 0;  // 명령 처리 후 초기화
            currentState = IDLE;
            delay(200);
          }
          break;
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
    lastMeasuredDistance = distance;  // 측정된 거리 저장
    Serial.print("Shoe size measured: ");
    Serial.print(distance);
    Serial.println(" cm");

    adjustBrushes(distance);
    //startCleaning();
    //cleaningStartTime = millis();
    //currentState = CLEANING;
  }
}

// 브러시 조정 함수
void adjustBrushes(int distance) {
  // int brushAngle;
  if (distance < 3) {
    brushAngle = 105;
  }else {
    brushAngle = 110;
  }
  
  // 측면 브러시들 중립 위치로
  // sideBrush1.write(20);
  // sideBrush2.write(160);

  sideBrush1.attach(SERVO_BRUSH_1);
  sideBrush2.attach(SERVO_BRUSH_2);
  for (int pos = MIN_ANGLE; pos <= brushAngle; pos += 1) {
    sideBrush1.write(pos+10);
    sideBrush2.write(180-pos);
    delay(SIDE_SERVO_SPEED);
  }
  
  Serial.print("Brushes adjusted to angle: ");
  
  Serial.println(brushAngle);
  //delay(1000);

  // 필요한 경우 서보모터 분리
  // sideBrush1.detach();
  // sideBrush2.detach();
}

// 청소 시작 함수
void startCleaning_side() {

  int motorSpeed = (currentMode == NORMAL) ? NORMAL_SPEED : QUICK_SPEED;
  
  for(int i = 0; i<LED_COUNT; i++)
    {
      strip.setPixelColor(i,0, 255, 0);
    }
    strip.show();

  digitalWrite(DC_SIDE_1_IN1, HIGH);
  digitalWrite(DC_SIDE_1_IN2, LOW);
  digitalWrite(DC_SIDE_2_IN1, HIGH);
  digitalWrite(DC_SIDE_2_IN2, LOW);

  analogWrite(DC_SIDE_1_PWM, motorSpeed);
  analogWrite(DC_SIDE_2_PWM, motorSpeed);

  
  
  Serial.println(currentMode == NORMAL ? "Normal cleaning mode started" : "Quick cleaning mode started");
}

void startCleaning_bottom() {
  int motorSpeed = (currentMode == NORMAL) ? NORMAL_SPEED : QUICK_SPEED;
  
  for(int i = 0; i<LED_COUNT; i++)
    {
      strip.setPixelColor(i,0, 255, 0);
    }
    strip.show();

  digitalWrite(DC_BOTTOM_IN1, HIGH);
  digitalWrite(DC_BOTTOM_IN2, LOW);
  
  analogWrite(DC_BOTTOM_PWM, motorSpeed);
  
  Serial.println(currentMode == NORMAL ? "Normal cleaning mode started" : "Quick cleaning mode started");
}

// void swingArmUp() {
//   swingArm.write(SWING_UP);
//   delay(1000);
//   Serial.println("Swing arm moved up");
//   currentState = ARM_UP;
// }

// void swingArmDown() {
//   swingArm.write(SWING_DOWN);
//   delay(1000);
//   Serial.println("Swing arm moved down");
// }

// 서보모터를 천천히 움직이는 함수 추가
void moveServoSlowly(int startAngle, int endAngle) {  
  Serial.print("Moving from angle: ");
  Serial.print(startAngle);
  Serial.print(" to angle: ");
  Serial.println(endAngle);

  for(int i = 0; i<LED_COUNT; i++)
    {
      strip.setPixelColor(i,255, 0,0);
    }

    strip.show();

  if(startAngle < endAngle) {
    // 각도 증가
    for(int angle = startAngle; angle <= endAngle; angle += 1) {
      swingArm1.write(angle);
      swingArm2.write(180 - angle);
      delay(SERVO_SPEED);
    }
    // Serial.println("SWING_UP_COMPLETE");
  } else {
    // 각도 감소
    for(int angle = startAngle; angle >= endAngle; angle -= 1) {
      swingArm1.write(angle);
      swingArm2.write(180 - angle);
      delay(SERVO_SPEED);
    }
    // Serial.println("SWING_DOWN_COMPLETE");
  }
  
  //swingArm.detach();
  delay(500);  // 최종 위치에서 안정화
  
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
  // 서보모터 초기화
  swingArm1.detach();
  swingArm2.detach();
  sideBrush1.detach();
  sideBrush2.detach();
  Serial.println("Motors stopped");
  // Serial.println("CLEANING_END");
}

void resetPosition() {
  sideBrush1.attach(SERVO_BRUSH_1);
  sideBrush2.attach(SERVO_BRUSH_2);
  // for (int pos = brushAngle; pos > 0; pos -= 1) {
  //   sideBrush1.write(pos);
  //   sideBrush2.write(pos+10);
  //   delay(30);
  // }
  // sideBrush1.write(0);
  // sideBrush2.write(0);
  swingArm1.write(SWING_DOWN);
  swingArm2.write(SWING_DOWN);
  //delay(1000);
  Serial.println("Reset to initial position");
  currentState = IDLE;
}
