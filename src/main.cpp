// Project Name : LineTracer Robot
// 검정색 라인을 추적
// 하드웨어 연결 정보

/*
L298 모터 드라이버 ---- Arduino
IN1---D7
IN2---D8
ENA---D5(PWM 왼쪽 바퀴 속도 조절)
IN3---D9
IN4---10
ENB---D6(PWM 오른쪽 바퀴 속도 조절)

IR 적외선 센서 ------ Arduino
LS---D11
CS---D12
RS---D13
*/

// 필요한 라이브러리 불러오기
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 핀 번호 정의(하드웨어 설정)
// 모터 제어용 핀
#define IN1 7 // 왼쪽 모터의 방향 제어 핀 1
#define IN2 8 // 왼쪽 모터의 방향 제어 핀 2
#define ENA 5 // 왼쪽 모터의 속도(PWM) 제어 핀

#define IN3 9 // 오른쪽 모터의 방향 제어 핀 1
#define IN4 10 // 오른쪽 모터의 방향 제어 핀 2
#define ENB 6 // 오른쪽 모터의 속도(PWM) 제어 핀

// 적외선 센서용 핀
#define LS 11 // 왼쪽 센서가 연결된 핀
#define CS 12 // 중앙 센서가 연결된 핀
#define RS 13 // 오른쪽 센서가 연결된 핀

// FSM(유한 상태 기계) 상태 정의
enum RobotState {
  STATE_STOP,   // 정지 상태
  STATE_FWD,    // 직진 상태
  STATE_SOFT_L, // 소프트 좌회전 (한쪽 정지)
  STATE_HARD_L, // 하드 좌회전 (한쪽 역회전)
  STATE_SOFT_R, // 소프트 우회전 (한쪽 정지)
  STATE_HARD_R, // 하드 우회전 (한쪽 역회전)
  STATE_BWD,    // 후진 상태
  STATE_SPIN_L, // 왼쪽 제자리 스핀턴 (라인 이탈 시)
  STATE_SPIN_R  // 오른쪽 제자리 스핀턴 (라인 이탈 시)
};

// FSM 제어를 위한 현재 상태와 다음 상태를 저장할 변수를 선언
RobotState currentState = STATE_STOP;
RobotState nextState = STATE_STOP;

// 마지막 회전 방향을 기억하기 위한 변수 (0: 왼쪽, 1: 오른쪽)
int lastTurn = 0; 

// 객체(object) 생성
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 모터 제어 함수 정의
// 스핀턴 및 하드턴을 위해 왼쪽/오른쪽 바퀴의 회전 방향(전/후진)을 각각 제어
void setMotor(int leftPwm, int rightPwm, bool leftFwd = true, bool rightFwd = true) {
  // 왼쪽 모터 전/후진 방향 설정
  digitalWrite(IN1, leftFwd ? HIGH : LOW);  
  digitalWrite(IN2, leftFwd ? LOW : HIGH);
  
  // 오른쪽 모터 전/후진 방향 설정
  digitalWrite(IN3, rightFwd ? HIGH : LOW); 
  digitalWrite(IN4, rightFwd ? LOW : HIGH);
  
  // 아날로그 출력(PWM)을 통해서 모터의 회전속도 조절
  analogWrite(ENA, leftPwm);  // 왼쪽 모터 속도 제어
  analogWrite(ENB, rightPwm); // 오른쪽 모터 속도 제어
}

// LCD 출력 함수 정의
void displayStatus(const char* msg, int leftPwm, int rightPwm) {
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(msg); 

  lcd.setCursor(0, 1);
  lcd.print("PWM=");
  lcd.print(leftPwm); 
  lcd.print(",");
  lcd.print(rightPwm);  
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(5, 0);
  lcd.print("Hello");
  lcd.setCursor(2, 1);
  lcd.print("RobotSystem");
  delay(2000);  

  // 모터 제어 핀들을 모두 출력 모드로 설정
  for(int i = 5; i <= 10 ; i++) pinMode(i, OUTPUT);
  // 센서 제어 핀들을 모두 입력 모드로 설정
  pinMode(LS, INPUT);
  pinMode(CS, INPUT);
  pinMode(RS, INPUT);
}

// 상태 전환 결정(transition)
void determinNextState() {
  bool LSV = digitalRead(LS);
  bool CSV = digitalRead(CS);
  bool RSV = digitalRead(RS);

  String SensorValue = String(LSV) + String(CSV) + String(RSV);
  Serial.println("Sensor=" + SensorValue);

  // 1. 라인 이탈 (000) -> 기억된 마지막 방향으로 스핀턴
  if(LSV == 0 && CSV == 0 && RSV == 0) {
    if(lastTurn == 0) nextState = STATE_SPIN_L;
    else nextState = STATE_SPIN_R;
  }
  // 2. 직진 (010)
  else if(LSV == 0 && CSV == 1 && RSV == 0) {
    nextState = STATE_FWD;
  }
  // 3. 소프트 좌회전 (110)
  else if(LSV == 1 && CSV == 1 && RSV == 0) {
    nextState = STATE_SOFT_L;
    lastTurn = 0; // 마지막 방향 '왼쪽'으로 기억
  }
  // 4. 하드 좌회전 (100)
  else if(LSV == 1 && CSV == 0 && RSV == 0) {
    nextState = STATE_HARD_L;
    lastTurn = 0; // 마지막 방향 '왼쪽'으로 기억
  }
  // 5. 소프트 우회전 (011)
  else if(LSV == 0 && CSV == 1 && RSV == 1) {
    nextState = STATE_SOFT_R;
    lastTurn = 1; // 마지막 방향 '오른쪽'으로 기억
  }
  // 6. 하드 우회전 (001)
  else if(LSV == 0 && CSV == 0 && RSV == 1) {
    nextState = STATE_HARD_R;
    lastTurn = 1; // 마지막 방향 '오른쪽'으로 기억
  }
  // 7. 교차로 또는 정지선 (111) -> 후진
  else if(LSV == 1 && CSV == 1 && RSV == 1) {
    // if(lastTurn == 0) nextState = STATE_SPIN_L;
    // else nextState = STATE_SPIN_R;
    nextState = STATE_FWD;
  }
}

// FSM 상태별 동작을 실행
void excuteState() {
  if(currentState != nextState) {
    currentState = nextState; 
    
    switch(currentState) {
      case STATE_STOP :
        setMotor(0, 0); 
        displayStatus("ROBOT STOP", 0, 0);  
        break;
        
      case STATE_FWD :
        setMotor(200, 200, true, true); 
        displayStatus("ROBOT FWD", 200, 200);
        break;
        
      // ---- 소프트 턴 (한쪽 바퀴 0, 한쪽 바퀴 200 전진) ----
      case STATE_SOFT_L :
        setMotor(90, 220, true, true); 
        displayStatus("SOFT L_TURN", 100, 220); 
        break;
        
      case STATE_SOFT_R :  
        setMotor(220, 90, true, true); 
        displayStatus("SOFT R_TURN", 220, 100);
        break;
        
      // ---- 하드 턴 (꺾는 방향의 바퀴는 100으로 역회전, 반대쪽은 200 전진) ----
      case STATE_HARD_L :
        setMotor(0, 200, true, true); // 왼쪽은 false(역회전)로 100
        displayStatus("HARD L_TURN", 0, 200); 
        break;
        
      case STATE_HARD_R :  
        setMotor(200, 0, true, true); // 오른쪽은 false(역회전)로 100
        displayStatus("HARD R_TURN", 200, 0);
        break;
        
      // ---- 후진 및 제자리 스핀턴 ----
      case STATE_BWD :     
        setMotor(200, 200, false, false); // 양쪽 모두 후진으로 속도 220
        displayStatus("ROBOT BWD", -200, -200);
        break;
        
      case STATE_SPIN_L :
        setMotor(200, 200, true, false); // 왼쪽 바퀴 후진, 오른쪽 바퀴 전진
        displayStatus("SPIN LEFT", -200, 200);
        break;
        
      case STATE_SPIN_R :
        setMotor(200, 200, false, true); // 왼쪽 바퀴 전진, 오른쪽 바퀴 후진
        displayStatus("SPIN RIGHT", 200, -200);
        break;
    }
  }
}

void loop() {
  determinNextState();
  excuteState();
  delay(10); // 상태 판단 주기 (필요시 조절)
}