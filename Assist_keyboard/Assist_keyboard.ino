#include <Adafruit_TinyUSB.h> /* 반드시 코드에 포함되어야 합니다. 부트로더에서 사용합니다. */
#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h> 

// ----------------------------------------------------
// 하드웨어 핀 정의
// ----------------------------------------------------
const int BTN_SHIFT = 31; // P0.31
const int BTN_2     = 39; // P1.07
const int BTN_3     = 38; // P1.06
const int BTN_4     = 37; // P1.05

const int OLED_SDA  = 22; // P0.22
const int OLED_SCL  = 21; // P0.21

// 네오픽셀 핀 및 개수 정의
#define NEOPIXEL_PIN 20   // P0.20
#define NUM_LEDS     4    // 연결된 LED 개수

// ----------------------------------------------------
// OLED, 네오픽셀 및 상태 변수 설정
// ----------------------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 네오픽셀 객체 생성
Adafruit_NeoPixel pixels(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// 밝기 제어 변수 추가 (0 ~ 4단계)
int currentBrightnessLevel = 3; // 기본값 3
const uint8_t brightnessMap[5] = {0, 64, 128, 192, 255}; // 0%, 25%, 50%, 75%, 100%

// 현재 색상 저장용 변수 (밝기 변경 시 색상 열화 방지용)
uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;

// 버튼 이전 상태 저장용
int lastStateShift = HIGH; 
int lastState2 = HIGH;
int lastState3 = HIGH;
int lastState4 = HIGH;

// 조합키(BTN_4) 사용 여부 확인용 플래그
bool btn4UsedAsModifier = false; 

// 토글(번갈아 표시)을 위한 가상 상태 변수
bool isPlaying = false; 
bool isMuted = false;

// BLE HID 객체 생성
BLEHidAdafruit blehid;

// ----------------------------------------------------
// 네오픽셀 랜덤 컬러 변경 함수
// ----------------------------------------------------
void setRandomNeoPixelColor() {
  currentR = random(0, 256);
  currentG = random(0, 256);
  currentB = random(0, 256);

  for(int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(currentR, currentG, currentB));
  }
  pixels.show(); 
}

// 밝기만 업데이트하고 원본 색상을 다시 입히는 함수
void updateNeoPixelBrightness() {
  pixels.setBrightness(brightnessMap[currentBrightnessLevel]);
  for(int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(currentR, currentG, currentB));
  }
  pixels.show(); 
}

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
  // 네오픽셀 초기화 및 기본 밝기 설정
  pixels.begin();
  pixels.setBrightness(brightnessMap[currentBrightnessLevel]);
  pixels.clear();
  pixels.show(); // 데이터 핀을 잡아주어 초기 노이즈 깜빡임 방지

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

  int currentStateShift = digitalRead(BTN_SHIFT);
  bool isShiftPressed = (currentStateShift == LOW);
  
  int currentState2 = digitalRead(BTN_2);
  int currentState3 = digitalRead(BTN_3);
  int currentState4 = digitalRead(BTN_4);

  // ----------------------------------------------------
  // BTN_SHIFT
  // ----------------------------------------------------
  if (lastStateShift == HIGH && currentStateShift == LOW) {
    setRandomNeoPixelColor(); 
  }

  // ----------------------------------------------------
  // BTN_2 : 다음 곡 / 볼륨 UP OR 밝기 DOWN
  // ----------------------------------------------------
  if (lastState2 == HIGH && currentState2 == LOW) {
    if (currentState4 == LOW) { // BTN_4 + BTN_2 : 밝기 한 단계 낮춤
      if (currentBrightnessLevel > 0) currentBrightnessLevel--;
      updateNeoPixelBrightness();
      
      char msg[16];
      sprintf(msg, "BRIGHT:%d", currentBrightnessLevel);
      showOledMessage(msg);
      
      btn4UsedAsModifier = true; // BTN_4가 조합키로 쓰였음을 표시
    } else {
      setRandomNeoPixelColor(); 

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
  }

  // ----------------------------------------------------
  // BTN_3 : 이전 곡 / 볼륨 DOWN OR 밝기 UP
  // ----------------------------------------------------
  if (lastState3 == HIGH && currentState3 == LOW) {
    if (currentState4 == LOW) { // BTN_4 + BTN_3 : 밝기 한 단계 높임
      if (currentBrightnessLevel < 4) currentBrightnessLevel++;
      updateNeoPixelBrightness();
      
      char msg[16];
      sprintf(msg, "BRIGHT:%d", currentBrightnessLevel);
      showOledMessage(msg);
      
      btn4UsedAsModifier = true; // BTN_4가 조합키로 쓰였음을 표시
    } else {
      setRandomNeoPixelColor(); 

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
  }

  // ----------------------------------------------------
  // BTN_4 : 음소거 / 재생 및 일시정지 (손을 뗄 때 작동)
  // ----------------------------------------------------
  if (lastState4 == LOW && currentState4 == HIGH) { // 버튼을 누르다 뗄 때
    if (!btn4UsedAsModifier) { // 밝기 조절용으로 쓰지 않은 경우에만 작동
      setRandomNeoPixelColor(); 

      if (isShiftPressed) {
        blehid.consumerKeyPress(HID_USAGE_CONSUMER_MUTE);
        if (isMuted) {
          showOledMessage("NORMAL");
          isMuted = false;
        } else {
          showOledMessage("MUTE");
          isMuted = true;
        }
      } else {
        blehid.consumerKeyPress(HID_USAGE_CONSUMER_PLAY_PAUSE);
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
    btn4UsedAsModifier = false; // 플래그 초기화
  }

  lastStateShift = currentStateShift;
  lastState2 = currentState2;
  lastState3 = currentState3;
  lastState4 = currentState4;

  delay(10);
}