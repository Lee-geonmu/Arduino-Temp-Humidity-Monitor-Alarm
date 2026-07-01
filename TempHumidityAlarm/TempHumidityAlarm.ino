#include <LiquidCrystal.h>
#include "DHT.h"
#define DHTPIN 7
#define DHTTYPE DHT11
#define LED_PIN 13
#define BUZZER_PIN 8
#define TEMP_LIMIT 30.0
#define HUMI_LIMIT 70.0
#define FILTER_SIZE 5
#define ALARM_DELAY 3
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
float tempBuffer[FILTER_SIZE];
float humiBuffer[FILTER_SIZE];
int bufferIndex = 0;
bool bufferFilled = false;
int alarmCounter = 0;
//이동평균 필터: 센서 노이즈 제거
float getAverage(float* buffer) {
  int count = bufferFilled ? FILTER_SIZE : bufferIndex;
  if (count == 0) return 0;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += buffer[i];
  return sum / count;
}
//알람 카운터(0~3)에 따라 LCD 2행에 상태바 표시
void drawBar(int count) {
  int barLength = 0;
  if (count == 1) barLength = 5;
  else if (count == 2) barLength = 10;
  else if (count == 3) barLength = 16;
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.write(i < barLength ? 255 : 32);
  }
}
void setup() {
  lcd.begin(16, 2);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.setCursor(0, 0);
  lcd.print("Smart Home v1.0");
  lcd.setCursor(0, 1);
  lcd.print("Warming up...  ");
  delay(2000);
  lcd.clear();
  //버퍼 사전 충전: 초기 이상값 출력 방지
  lcd.setCursor(0, 0);
  lcd.print("Calibrating... ");
  lcd.setCursor(0, 1);
  lcd.print("Please wait... ");
  for (int i = 0; i < FILTER_SIZE; i++) {
    delay(500);
    float rawT = dht.readTemperature();
    float rawH = dht.readHumidity();
    if (!isnan(rawT) && !isnan(rawH)) {
      tempBuffer[i] = rawT;
      humiBuffer[i] = rawH;
    }
  }
  bufferFilled = true;
  bufferIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  System Ready!");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...  ");
  delay(1500);
  lcd.clear();
}
void loop() {
  delay(1000);
  float rawH = dht.readHumidity();
  float rawT = dht.readTemperature();
  if (isnan(rawH) || isnan(rawT)) {
    lcd.setCursor(0, 0);
    lcd.print("  Sensor Error!");
    lcd.setCursor(0, 1);
    lcd.print(" Check wiring! ");
    //에러 상태에서도 alarmCounter를 감소시켜 알람 고착 방지
    if (alarmCounter > 0) alarmCounter--;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }
  //링 버퍼 방식으로 최근 5개 값 유지
  tempBuffer[bufferIndex] = rawT;
  humiBuffer[bufferIndex] = rawH;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  if (bufferIndex == 0) bufferFilled = true;
  float t = getAverage(tempBuffer);
  float h = getAverage(humiBuffer);
  //디바운싱: 3회 연속 초과 시에만 알람(오작동 방지)
  if (t > TEMP_LIMIT || h > HUMI_LIMIT) {
    if (alarmCounter < ALARM_DELAY) alarmCounter++;
  } else {
    if (alarmCounter > 0) alarmCounter--;
  }
  bool alarmOn = (alarmCounter >= ALARM_DELAY);
  if (alarmOn) {
    lcd.setCursor(0, 0);
    if (t > TEMP_LIMIT && h > HUMI_LIMIT) {
      lcd.print("!!TEMP+HUMI!!  ");
    } else if (t > TEMP_LIMIT) {
      lcd.print("!!TEMP HIGH!!  ");
    } else {
      lcd.print("!!HUMI HIGH!!  ");
    }
    drawBar(3);
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(t, 1);
    lcd.print("C H:");
    lcd.print(h, 0);
    lcd.print("%  ");
    drawBar(alarmCounter);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}
