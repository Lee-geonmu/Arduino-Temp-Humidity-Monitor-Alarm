#include <LiquidCrystal.h>
#include "DHT.h"
#define DHTPIN 7
#define DHTTYPE DHT11
#define LED_PIN 13
#define BUZZER_PIN 8
#define TEMP_LIMIT 30.0
#define HUMI_LIMIT 70.0
#define FILTER_SIZE 5
#define ALARM_THRESHOLD 3 //경보 판정 카운터 임계값
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
float tempBuffer[FILTER_SIZE];
float humiBuffer[FILTER_SIZE];
int bufferIndex = 0;
bool bufferFilled = false;
int alarmCounter = 0;
//이동평균 필터: 최근 판독값 평균으로 순간 스파이크 완화
float getAverage(const float* buffer) {
  int count = bufferFilled ? FILTER_SIZE : bufferIndex;
  if (count == 0) return 0;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += buffer[i];
  return sum / count;
}
//경보 카운터 단계(1,2,3)에 따라 5/10/16칸 상태 바를 LCD 2행에 표시
void drawBar(int count) {
  int barLength = 0;
  if (count == 1) barLength = 5;
  else if (count == 2) barLength = 10;
  else if (count == 3) barLength = 16;
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.write(i < barLength ? 255 : 32); //255: HD44780 풀 블록 문자
  }
}
void setup() {
  lcd.begin(16, 2);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.setCursor(0, 0);
  lcd.print(F("Smart Home v1.0"));
  lcd.setCursor(0, 1);
  lcd.print(F("Warming up...  "));
  delay(2000);
  lcd.clear();
  //버퍼 사전 충전: 부팅 직후 이동평균 왜곡(초기 이상값 출력) 방지
  lcd.setCursor(0, 0);
  lcd.print(F("Calibrating... "));
  lcd.setCursor(0, 1);
  lcd.print(F("Please wait... "));
  for (int i = 0; i < FILTER_SIZE; i++) {
    delay(500);
    float rawT = dht.readTemperature();
    float rawH = dht.readHumidity();
    //판독 실패 시 재시도: 슬롯에 초기값 0.0이 잔류해 평균을 왜곡하는 것 방지
    int retry = 0;
    while ((isnan(rawT) || isnan(rawH)) && retry++ < 3) {
      delay(2000); //라이브러리 최소 읽기 간격(2s) 경과 후 실제 재판독
      rawT = dht.readTemperature();
      rawH = dht.readHumidity();
    }
    if (isnan(rawT) || isnan(rawH)) { //지속 실패시 모니터링 진행이 무의미하므로 부팅 중단
      lcd.clear();
      lcd.print(F("Sensor Fail!"));
      lcd.setCursor(0, 1);
      lcd.print(F("Check wiring"));
      while (true);
    }
    tempBuffer[i] = rawT;
    humiBuffer[i] = rawH;
  }
  bufferFilled = true;
  bufferIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  System Ready!"));
  lcd.setCursor(0, 1);
  lcd.print(F("Monitoring...  "));
  delay(1500);
  lcd.clear();
}
void loop() {
  delay(1000);
  float rawH = dht.readHumidity();
  float rawT = dht.readTemperature();
  //통신 실패(NaN)시 에러 표시, 카운터 감쇠, 출력 강제 OFF함으로써 경보 고착 방지
  if (isnan(rawH) || isnan(rawT)) {
    lcd.setCursor(0, 0);
    lcd.print(F("  Sensor Error!"));
    lcd.setCursor(0, 1);
    lcd.print(F(" Check wiring! "));
    if (alarmCounter > 0) alarmCounter--;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }
  //링 버퍼: 최근 FILTER_SIZE개 판독값 유지(동적 할당 없음)
  tempBuffer[bufferIndex] = rawT;
  humiBuffer[bufferIndex] = rawH;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  if (bufferIndex == 0) bufferFilled = true;
  float t = getAverage(tempBuffer);
  float h = getAverage(humiBuffer);
  //포화 업/다운 카운터: 초과 +1, 정상 -1(임계 도달 시에만 경보)
  //노이즈성 정상값 1회가 판정 이력을 리셋하지 않고 1만 감쇠(오경보 억제)
  if (t > TEMP_LIMIT || h > HUMI_LIMIT) {
    if (alarmCounter < ALARM_THRESHOLD) alarmCounter++;
  } else {
    if (alarmCounter > 0) alarmCounter--;
  }
  bool alarmOn = (alarmCounter >= ALARM_THRESHOLD);
  if (alarmOn) {
    lcd.setCursor(0, 0);
    if (t > TEMP_LIMIT && h > HUMI_LIMIT) {
      lcd.print(F("!!TEMP+HUMI!!  "));
    } else if (t > TEMP_LIMIT) {
      lcd.print(F("!!TEMP HIGH!!  "));
    } else {
      lcd.print(F("!!HUMI HIGH!!  "));
    }
    drawBar(ALARM_THRESHOLD);
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  } else {
    lcd.setCursor(0, 0);
    lcd.print(F("T:"));
    lcd.print(t, 1);
    lcd.print(F("C H:"));
    lcd.print(h, 0);
    lcd.print(F("%  "));
    drawBar(alarmCounter);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}
