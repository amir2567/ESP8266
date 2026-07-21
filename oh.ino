#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

// ========== تنظیمات شبکه و سرور ==========
const char* WIFI_SSID = "(GT master)";
const char* WIFI_PASS = "20002000";
const char* SERVER_URL = "http://192.168.1.102:5000";
const char* API_TOKEN = "81b7791e481850f155bf6a13f93421a247f1732957f1f76acab139ffdefd8aa6";
const int DEVICE_ID = 1;

WiFiClient wifi;
bool wifiConnected = false;
String lastCommand = "";

// ========== متغیرهای رقص نور (دقیقاً مثل کد خودت) ==========
unsigned long lastPatternSwitch = 0;
unsigned long lastToggle = 0;
unsigned long lastPrint = 0;
int patternIndex = 0;
bool ledState = false;

const unsigned long PRINT_INTERVAL = 5000;
const unsigned long PATTERN_SWITCH_INTERVAL = 1200;

// ========== تعریف پایه‌ها ==========
#define DHTPIN     D2
#define DHTTYPE    DHT22
#define LDRPIN     D5
#define DANCELED   D0

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DANCELED, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(DANCELED, LOW);
  dht.begin();

  // ========== اتصال به وای‌فای ==========
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("📡 اتصال به وای‌فای");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ وای‌فای وصل شد!");
    wifiConnected = true;
  } else {
    Serial.println("\n❌ وای‌فای وصل نشد!");
    wifiConnected = false;
  }
}

void loop() {
  unsigned long now = millis();

  // ========== رقص نور (دقیقاً مثل کد خودت) ==========
  if (now - lastPatternSwitch > PATTERN_SWITCH_INTERVAL) {
    lastPatternSwitch = now;
    patternIndex = (patternIndex + 1) % 4;
  }
  runDancePattern(now);

  // ========== ارتباط با سرور (هر ۱۵ ثانیه) ==========
  static unsigned long lastServerCheck = 0;
  if (now - lastServerCheck > 15000) {
    lastServerCheck = now;
    if (wifiConnected && WiFi.status() == WL_CONNECTED) {
      sendSensorDataToServer();   // ۳ درخواست جداگانه با تایم‌اوت کم
      checkPendingCommands();     // ۱ درخواست با تایم‌اوت کم
    } else {
      wifiConnected = false;
      WiFi.reconnect();
    }
  }

  // ========== چاپ دیتا هر ۵ ثانیه ==========
  if (now - lastPrint >= PRINT_INTERVAL) {
    lastPrint = now;
    printSensorBlock();
  }
}

// ========== تابع رقص نور (دقیقاً مثل کد خودت) ==========
void runDancePattern(unsigned long now) {
  switch (patternIndex) {
    case 0:
      if (now - lastToggle > 60) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
        digitalWrite(DANCELED, ledState ? HIGH : LOW);
      }
      break;
    case 1:
      if (now - lastToggle > 90) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
        digitalWrite(DANCELED, ledState ? LOW : HIGH);
      }
      break;
    case 2:
      if (now - lastToggle > 35) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
        digitalWrite(DANCELED, ledState ? LOW : HIGH);
      }
      break;
    case 3:
      if (now - lastToggle > (ledState ? 40 : 150)) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
        digitalWrite(DANCELED, ledState ? HIGH : LOW);
      }
      break;
  }
}

void printSensorBlock() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  bool isLight = (digitalRead(LDRPIN) == LOW);
  unsigned long uptimeSec = millis() / 1000;

  Serial.println("🌈 ────────── داده‌های جدید ──────────");
  Serial.print("🌡️  دما: ");
  Serial.println(isnan(t) ? "❌ خطا" : String(t) + " °C");

  Serial.print("💧 رطوبت: ");
  Serial.println(isnan(h) ? "❌ خطا" : String(h) + " %");

  Serial.print("☀️  نور محیط: ");
  Serial.println(isLight ? "🔆 روشن" : "🌑 تاریک");

  Serial.print("⏱️  زمان فعالیت: ");
  Serial.println(String(uptimeSec) + " ثانیه");

  if (lastCommand != "") {
    Serial.println("📥 فرمان: " + lastCommand);
    lastCommand = "";
  }
}

// ========== ارسال ۳ درخواست جداگانه با تایم‌اوت ۵۰۰ میلی‌ثانیه ==========
void sendSensorDataToServer() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int lightValue = digitalRead(LDRPIN) == LOW ? 1 : 0;

  // ---- دما ----
  if (!isnan(temp)) {
    HTTPClient http;
    String url = String(SERVER_URL) + "/api/devices/" + DEVICE_ID + "/data";
    http.begin(wifi, url);
    http.setTimeout(500);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(API_TOKEN));
    String json = "{\"sensor_type\":\"temperature\",\"value\":" + String(temp) + "}";
    http.POST(json);
    http.end();
  }

  // ---- رطوبت ----
  if (!isnan(hum)) {
    HTTPClient http;
    String url = String(SERVER_URL) + "/api/devices/" + DEVICE_ID + "/data";
    http.begin(wifi, url);
    http.setTimeout(500);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(API_TOKEN));
    String json = "{\"sensor_type\":\"humidity\",\"value\":" + String(hum) + "}";
    http.POST(json);
    http.end();
  }

  // ---- نور ----
  {
    HTTPClient http;
    String url = String(SERVER_URL) + "/api/devices/" + DEVICE_ID + "/data";
    http.begin(wifi, url);
    http.setTimeout(500);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(API_TOKEN));
    String json = "{\"sensor_type\":\"light\",\"value\":" + String(lightValue) + "}";
    http.POST(json);
    http.end();
  }
}

// ========== دریافت فرمان (با تایم‌اوت ۵۰۰ میلی‌ثانیه) ==========
void checkPendingCommands() {
  HTTPClient http;
  String url = String(SERVER_URL) + "/api/devices/" + DEVICE_ID + "/pending_commands";
  http.begin(wifi, url);
  http.setTimeout(500);
  http.addHeader("Authorization", "Bearer " + String(API_TOKEN));

  int code = http.GET();
  if (code == 200) {
    String response = http.getString();
    lastCommand = "";

    if (response.indexOf("led_on") != -1) {
      digitalWrite(LED_BUILTIN, LOW);
      lastCommand = "[led_on]";
    }
    else if (response.indexOf("led_off") != -1) {
      digitalWrite(LED_BUILTIN, HIGH);
      lastCommand = "[led_off]";
    }
    else if (response.indexOf("relay_on") != -1) {
      digitalWrite(DANCELED, HIGH);
      lastCommand = "[relay_on]";
    }
    else if (response.indexOf("relay_off") != -1) {
      digitalWrite(DANCELED, LOW);
      lastCommand = "[relay_off]";
    }
  }
  http.end();
}