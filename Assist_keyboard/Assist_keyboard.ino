#include <Adafruit_TinyUSB.h> /* 반드시 코드에 포함되어야 합니다. 부트로더에서 사용합니다. */
#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----------------------------------------------------
// 하드웨어 핀 정의
// ----------------------------------------------------
const int BTN_SHIFT = 31; // P0.31
const int BTN_2     = 39; // P1.07
const int BTN_3     = 38; // P1.06
const int BTN_4     = 37; // P1.05

const int OLED_SDA  = 22; // P0.22
const int OLED_SCL  = 21; // P0.21

// ----------------------------------------------------
// OLED 및 상태 변수 설정
// ----------------------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 버튼 이전 상태 저장용
int lastState2 = HIGH;
int lastState3 = HIGH;
int lastState4 = HIGH;

// 토글(번갈아 표시)을 위한 가상 상태 변수
bool isPlaying = false; 
bool isMuted = false;   // MUTE / NORMAL 상태 저장용 변수 추가!

// BLE HID 객체 생성
BLEHidAdafruit blehid;

// ----------------------------------------------------
// 텍스트를 화면 정중앙에 출력하는 함수
// ----------------------------------------------------
void showOledMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
  
  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;

  display.setCursor(x, y);
  display.print(message);
  display.display();
}

void setup() {
  delay(1000);

  pinMode(BTN_SHIFT, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT_PULLUP);

  Wire.setPins(OLED_SDA, OLED_SCL); 
  Wire.begin();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // 초기화 실패 시 처리 생략
  }
  
  showOledMessage("READY");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("NU40 Media Ctrl");

  blehid.begin();
  startAdv();
}

void startAdv(void) {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);

  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0); 
}

void loop() {
  if (!Bluefruit.connected()) {
    delay(100);
    return;
  }

  bool isShiftPressed = (digitalRead(BTN_SHIFT) == LOW);
  int currentState2 = digitalRead(BTN_2);
  int currentState3 = digitalRead(BTN_3);
  int currentState4 = digitalRead(BTN_4);

  // ----------------------------------------------------
  // BTN_2 : 다음 곡 / 볼륨 UP
  // ----------------------------------------------------
  if (lastState2 == HIGH && currentState2 == LOW) {
    if (isShiftPressed) {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_PREVIOUS_TRACK); 
      showOledMessage("PREV_TRACK");
    } else {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_VOLUME_DECREMENT); 
      showOledMessage("VOLUME(-)");
    }
    delay(50);
    blehid.consumerKeyRelease();
  }

  // ----------------------------------------------------
  // BTN_3 : 이전 곡 / 볼륨 DOWN
  // ----------------------------------------------------
  if (lastState3 == HIGH && currentState3 == LOW) {
    if (isShiftPressed) {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_NEXT_TRACK); 
      showOledMessage("NEXT_TRACK");
    } else {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_VOLUME_INCREMENT); 
      showOledMessage("VOLUME(+)");
    }
    delay(50);
    blehid.consumerKeyRelease();     
  }

  // ----------------------------------------------------
  // BTN_4 : 음소거 / 재생 및 일시정지
  // ----------------------------------------------------
  if (lastState4 == HIGH && currentState4 == LOW) {
    if (isShiftPressed) {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_MUTE); 
      
      // MUTE / NORMAL 화면 토글 로직
      if (isMuted) {
        showOledMessage("NORMAL");
        isMuted = false;
      } else {
        showOledMessage("MUTE");
        isMuted = true;
      }
      
    } else {
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_PLAY_PAUSE); 
      
      // PLAY / PAUSE 화면 토글 로직
      if (isPlaying) {
        showOledMessage("PAUSE");
        isPlaying = false;
      } else {
        showOledMessage("PLAY");
        isPlaying = true;
      }
    }
    delay(50);
    blehid.consumerKeyRelease();
  }

  lastState2 = currentState2;
  lastState3 = currentState3;
  lastState4 = currentState4;

  delay(10);
}