// ============================================================
// Smart Air Quality Monitor — ESP32 (Optimized)
// Components: MQ-135, DHT11, Buzzer, 16x2 LCD (I2C)
// By: Aleena Soni, S4 EV, Saintgits College of Engineering
// LCD I2C Address: 0x27 (confirmed via scanner)
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ---------- PIN DEFINITIONS ----------
#define MQ135_PIN 34
#define DHT_PIN 4
#define BUZZER_PIN 18
#define DHT_TYPE DHT11

// ---------- TIMING ----------
#define WARMUP_MS 120000UL
#define SCREEN_INTERVAL 3000UL
#define DHT_INTERVAL 2000UL
#define BUZZER_BEEP_ON 200UL
#define BUZZER_BEEP_OFF 800UL

// ---------- THRESHOLDS ----------
const int GOOD_LIMIT = 150;
const int MODERATE_LIMIT = 300;

// ---------- MQ-135 CALIBRATION ----------
const int MQ135_BASELINE = 200;
const int MQ135_MAX_RAW = 3500;

// ---------- OBJECTS ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

// ---------- STATE ----------
float temperature = 0.0;
float humidity = 0.0;
int ppm = 0;
bool dangerActive = false;

unsigned long lastDHTRead = 0;
unsigned long lastScreenSwap = 0;
unsigned long lastBuzzerToggle= 0;
bool screenToggle = false;
bool buzzerState = false;

// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Air Quality Monitor ===");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // ---- Warmup screen ----
  unsigned long startTime = millis();
  Serial.println("MQ-135 warming up...");

  while (millis() - startTime < WARMUP_MS) {
    unsigned long remaining = (WARMUP_MS - (millis() - startTime)) / 1000;
    lcd.setCursor(0, 0);
    lcd.print("Warming up MQ135");
    lcd.setCursor(0, 1);
    lcd.print("Wait: ");
    lcd.print(remaining);
    lcd.print("s ");
    delay(500);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Air Quality ");
  lcd.setCursor(0, 1);
  lcd.print(" Monitor ");
  delay(1500);
  lcd.clear();

  lcd.setCursor(0, 0); lcd.print("PPM: AQ: ");
  lcd.setCursor(0, 1); lcd.print("T: H: ");

  Serial.println("Monitoring started.\n");
  lastDHTRead = millis();
  lastScreenSwap = millis();
}

// ============================================================
void loop() {
  unsigned long now = millis();

  // ---- 1. Read MQ-135 ----
  long rawSum = 0;
  for (int i = 0; i < 8; i++) {
    rawSum += analogRead(MQ135_PIN);
    delayMicroseconds(500);
  }
  int rawValue = rawSum / 8;
  int clampedRaw = max(rawValue - MQ135_BASELINE, 0);
  int scaledMax = max(MQ135_MAX_RAW - MQ135_BASELINE, 1);
  ppm = map(clampedRaw, 0, scaledMax, 0, 1000);
  ppm = constrain(ppm, 0, 1000);

  // ---- 2. Read DHT11 ----
  if (now - lastDHTRead >= DHT_INTERVAL) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
    } else {
      Serial.println("WARN: DHT11 read failed.");
    }
    lastDHTRead = now;
  }

  // ---- 3. Classify air quality ----
  const char* airQuality;
  if (ppm <= GOOD_LIMIT) airQuality = "GOOD ";
  else if (ppm <= MODERATE_LIMIT) airQuality = "MOD ";
  else airQuality = "DANGER! ";

  dangerActive = (ppm > MODERATE_LIMIT);

  // ---- 4. Buzzer ----
  if (dangerActive) {
    unsigned long beepPeriod = buzzerState ? BUZZER_BEEP_ON : BUZZER_BEEP_OFF;
    if (now - lastBuzzerToggle >= beepPeriod) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBuzzerToggle = now;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    lastBuzzerToggle = now;
  }

  // ---- 5. LCD ----
  if (now - lastScreenSwap >= SCREEN_INTERVAL) {
    screenToggle = !screenToggle;
    lastScreenSwap = now;

    if (screenToggle) {
      lcd.setCursor(0, 0); lcd.print("PPM: ");
      lcd.setCursor(4, 0); lcd.print(ppm);
      lcd.print(" ");
      lcd.setCursor(9, 0); lcd.print(airQuality);

      lcd.setCursor(0, 1); lcd.print("Temp: ");
      lcd.print(temperature, 1);
      lcd.write((byte)223);
      lcd.print("C ");
    } else {
      lcd.setCursor(0, 0); lcd.print("Humidity: ");
      lcd.setCursor(9, 0); lcd.print(humidity, 1);
      lcd.print(" % ");

      lcd.setCursor(0, 1); lcd.print("Raw ADC: ");
      lcd.print(rawValue);
      lcd.print(" ");
    }
  }

  // ---- 6. Serial log ----
  static unsigned long lastLog = 0;
  if (now - lastLog >= 6000) {
    Serial.println("----------------------------------");
    Serial.print("Gas PPM : "); Serial.println(ppm);
    Serial.print("Raw ADC : "); Serial.println(rawValue);
    Serial.print("Temperature : "); Serial.print(temperature, 1); Serial.println(" C");
    Serial.print("Humidity : "); Serial.print(humidity, 1); Serial.println(" %");
    Serial.print("Air Quality : "); Serial.println(airQuality);
    Serial.println("----------------------------------\n");
    lastLog = now;
  }
}
