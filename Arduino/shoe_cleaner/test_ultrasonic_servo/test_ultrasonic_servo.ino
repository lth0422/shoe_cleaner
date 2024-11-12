#include <Servo.h>
#include <NewPing.h>

#define SERVO_SWING 4      // 스윙암 서보모터
#define SERVO_BRUSH_1 5     // 측면 브러시 1 조절 서보모터
#define SERVO_BRUSH_2 6     // 측면 브러시 2 조절 서보모터
#define TRIG_PIN 26         // 초음파 센서 TRIG 핀 번호 
#define ECHO_PIN 27         // 초음파 센서 ECHO 핀 번호 

#define SWING_UP 170    
#define SWING_DOWN 10   
#define SERVO_SPEED 30  // 각도 변화 속도 (값이 작을수록 더 천천히 움직임)

Servo swingArm;
Servo sideBrush1;
Servo sideBrush2;
NewPing sonar(TRIG_PIN, ECHO_PIN, 200);

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(9600);
  while (!Serial) {
    ; // 시리얼 포트가 연결될 때까지 대기
  }
  
  // 서보모터 초기화
  swingArm.detach();  // 혹시 모를 이전 상태를 초기화
  sideBrush1.detach();
  sideBrush2.detach();
  
  delay(1000);  // 안정화를 위한 대기
  
  swingArm.attach(SERVO_SWING);
  sideBrush1.attach(SERVO_BRUSH_1);
  sideBrush2.attach(SERVO_BRUSH_2);

  // 시리얼 버퍼 비우기
  while(Serial.available() > 0) {
    Serial.read();
  }

  // 스윙암 초기 위치로 이동 (아래로)
  Serial.println("Initializing swing arm position...");
  swingArm.write(SWING_DOWN);  // 바로 아래 위치로 이동
  delay(1000);  // 안정화 대기
  swingArm.detach();
  
  // 측면 브러시들 중립 위치로
  sideBrush1.write(90);
  sideBrush2.write(90);
  
  Serial.println("Initialization complete!");
  Serial.println("Ready for commands (s: up, f: down+measure, e: up, d: down)");
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    static int currentAngle = SWING_DOWN;
    
    // 입력된 명령어 확인 출력
    Serial.print("Received command: ");
    Serial.println(command);
    
    switch (command) {
      case 's':  // 스윙암 올리기
        Serial.println("Swing arm up");
        moveServoSlowly(currentAngle, SWING_UP);
        currentAngle = SWING_UP;
        break;
      case 'f':  // 스윙암 내리기 및 측정 시작
        Serial.println("Swing arm down and measuring...");
        moveServoSlowly(currentAngle, SWING_DOWN);
        currentAngle = SWING_DOWN;
        delay(500);
        measureAndAdjust();
        break;
      case 'e':  // 측정 후 스윙암 올리기
        Serial.println("Measurement complete, swing arm up");
        moveServoSlowly(currentAngle, SWING_UP);
        currentAngle = SWING_UP;
        break;
      case 'd':  // 측정 후 스윙암 내리기
        Serial.println("Measurement complete, swing arm down");
        moveServoSlowly(currentAngle, SWING_DOWN);
        currentAngle = SWING_DOWN;
        break;  
    }
    
    // 시리얼 버퍼 비우기
    while(Serial.available() > 0) {
      Serial.read();
    }
  }
}

void measureAndAdjust() {
  float totalDistance = 0;
  int measurements = 30;
  
  for(int i = 0; i < measurements; i++) {
    totalDistance += sonar.ping_cm();
    delay(100);
  }
  
  float avgDistance = totalDistance / measurements;
  Serial.print("Average distance: ");
  Serial.println(avgDistance);
  
  // 거리에 따른 브러시 회전 속도 조절
  int brushSpeed;
  if(avgDistance < 10) {
    brushSpeed = 45;  // 느린 속도
  } else if(avgDistance < 15) {
    brushSpeed = 30;  // 중간 속도
  } else {
    brushSpeed = 0;   // 최대 속도
  }
  
  adjustBrushes(brushSpeed);
}

void adjustBrushes(int speed) {
  // 90이 정지, 0이 최대속도 시계방향, 180이 최대속도 반시계방향
  sideBrush1.write(speed);
  sideBrush2.write(180 - speed);  // 반대 방향으로 회전
  
  delay(1000);  // 1초 동안 회전
  
  // 정지 (90이 정지 지점)
  sideBrush1.write(90);
  sideBrush2.write(90);
}

// 서보모터를 천천히 움직이는 함수 추가
void moveServoSlowly(int startAngle, int endAngle) {
  swingArm.attach(SERVO_SWING);
  
  if(startAngle < endAngle) {
    // 각도 증가
    for(int angle = startAngle; angle <= endAngle; angle += 1) {
      swingArm.write(angle);
      delay(SERVO_SPEED);
    }
  } else {
    // 각도 감소
    for(int angle = startAngle; angle >= endAngle; angle -= 1) {
      swingArm.write(angle);
      delay(SERVO_SPEED);
    }
  }
  
  delay(500);  // 최종 위치에서 안정화
  swingArm.detach();
}


