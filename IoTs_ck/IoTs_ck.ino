#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6ji84yz7K"
#define BLYNK_TEMPLATE_NAME "LAB2"
#define BLYNK_AUTH_TOKEN "v_XS8I7xZSqRNDGWovlBWEkb5ttDQJsN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThingSpeak.h> 
#include <ArduinoJson.h> // THÊM THƯ VIỆN ARDUINOJSON CHO MCP
#include "WebSocketMCP.h"

// ===== WIFI =====
char ssid[] = "realme C55";
char pass[] = "123456789";
WiFiClient client; 

// ===== THINGSPEAK =====
unsigned long myChannelNumber = 3326937;         
const char * myWriteAPIKey = "FGVL61F2QLA7ABC9";    

// ===== MCP AI =====
const char* mcpEndpoint = "wss://api.xiaozhi.me/mcp/?token=eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjkwNTM4OSwiYWdlbnRJZCI6MTczMjQ0OCwiZW5kcG9pbnRJZCI6ImFnZW50XzE3MzI0NDgiLCJwdXJwb3NlIjoibWNwLWVuZHBvaW50IiwiaWF0IjoxNzc2NTMxOTgxLCJleHAiOjE4MDgwODk1ODF9.Hq1usR6MzbK60yFFFKOu59TakJhQda8nIf8DTqbPnFmbw9Ju78KokMLNDhe-dgjHvy_qttmSAR4nIrfZitpT-Q"; // Điền token của bạn
WebSocketMCP mcpClient;

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== MQ135 =====
#define MQ135_PIN 35

// ===== OUTPUT =====
#define LED_GREEN  33
#define LED_YELLOW 25
#define LED_RED    26
#define BUZZER_PIN 32



BlynkTimer timer;

// ===== BIẾN =====
unsigned long startTime = 0;
int lastLevel = -1;
int currentLevel = 0;

// Các biến lưu giá trị môi trường (đưa ra toàn cục để ThingSpeak và AI dùng chung)
float t = 0;
float h = 0;
float ppmFiltered = 0;

// Ngưỡng cảnh báo nhiệt độ (AI có thể thay đổi)
float warningTemp = 40; 

// buzzer control
unsigned long lastBuzz = 0;
bool buzzState = false;

// ===== HÀM TẮT LED =====
void allLEDsOff(bool updateBlynk = true) {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  if (updateBlynk && Blynk.connected()) {
    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
  }
}

// ===== CẬP NHẬT OLED =====
void updateOLED(float t, float h, float gas, bool mqReady, int level) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(7, 0); 
  display.print("GIAM SAT MOI TRUONG");

  display.setCursor(0, 11);
  display.print("WiFi: ");
  display.print(WiFi.status() == WL_CONNECTED ? "ON" : "OFF");

  display.setCursor(0, 22);
  display.print("NHIET DO: ");
  display.print(t, 1); 
  display.print(" "); display.print((char)247); display.print("C");

  display.setCursor(0, 33);
  display.print("DO AM:    ");
  display.print(h, 0); 
  display.print(" %");

  display.setCursor(0, 44);
  display.print("Chat luong:");
  if (mqReady) {
    display.print(""); 
    display.print(gas, 0); 
    display.print(" PPM");
  } else {
    display.print("Cho 60s"); 
  }

  display.setCursor(0, 55);
  display.print("TRANG THAI: ");
  if (level == 2) {
    display.print("NGUY HIEM");
  } else if (level == 1) {
    display.print("CANH BAO");
  } else {
    display.print("AN TOAN");
  }

  display.display();
}

// ========================================================================================
// CALLBACK & MCP TOOLS
// ========================================================================================
void registerMcpTools() {
    // 1. Tool báo giá trị cảm biến cho AI (ĐÃ ĐỔI MÔ TẢ THÀNH CHẤT LƯỢNG KHÔNG KHÍ)
    mcpClient.registerTool(
        "get_environment_values",
        "Lấy giá trị cảm biến nhiệt độ, độ ẩm và chất lượng không khí hiện tại",
        R"({
            "type": "object",
            "properties": {},
            "required": []
        })",
        [](const String& args) {
            Serial.println("\n=== AI yêu cầu giá trị cảm biến ===");
            
            DynamicJsonDocument doc(256);
            doc["temperature"] = t;
            doc["humidity"] = h;
            doc["gas_ppm"] = ppmFiltered;

            String json;
            serializeJson(doc, json);
            return WebSocketMCP::ToolResponse(json);
        }
    );
    Serial.println("[MCP] Registered: get_environment_values");

    // 2. Tool cài đặt ngưỡng cảnh báo nhiệt độ cho AI
    mcpClient.registerTool(
        "set_temperature_warning",
        "Cài đặt nhiệt độ cảnh báo nguy hiểm",
        R"({
            "type": "object",
            "properties": {
                "max_temperature": { "type": "number" }
            },
            "required": ["max_temperature"]
        })",
        [](const String& args) {
            DynamicJsonDocument doc(256);
            deserializeJson(doc, args);

            warningTemp = doc["max_temperature"].as<float>();

            Serial.println("\n=== AI thay đổi ngưỡng cảnh báo nhiệt độ ===");
            Serial.print("Ngưỡng cảnh báo mới: ");
            Serial.print(warningTemp);
            Serial.println(" °C");

            String json = "{\"success\":true,\"new_limit\":" + String(warningTemp) + "}";
            return WebSocketMCP::ToolResponse(json);
        }
    );
    Serial.println("[MCP] Registered: set_temperature_warning");
}

void onConnectionStatus(bool connected) {
    if (connected) {
        Serial.println("[MCP] Đã kết nối với AI Xiaozhi");
        registerMcpTools();
    } else {
        Serial.println("[MCP] Mất kết nối MCP");
    }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  allLEDsOff(false);

  // Khởi động OLED
  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED ko ket noi duoc!"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 25);
  display.print("SYSTEM STARTING...");
  display.display();

  // Khởi tạo WiFi thông qua Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  ThingSpeak.begin(client); // KHỞI TẠO THINGSPEAK
  
  // Khởi tạo MCP (Kết nối AI)
  mcpClient.begin(mcpEndpoint, onConnectionStatus);

  startTime = millis();

  timer.setInterval(3000L, sendSensorData);     // Cập nhật Blynk, OLED mỗi 3s
  timer.setInterval(20000L, sendToThingSpeak);  // Cập nhật ThingSpeak mỗi 20s
}

// ===== LOOP =====
void loop() {
  Blynk.run();
  timer.run();
  mcpClient.loop(); // Duy trì kết nối AI

  // ===== BUZZER =====
  if (currentLevel == 2) {
    if (millis() - lastBuzz > 500) {
      lastBuzz = millis();
      buzzState = !buzzState;
      digitalWrite(BUZZER_PIN, buzzState);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ===== HÀM GỬI LÊN THINGSPEAK =====
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);
    ThingSpeak.setField(3, ppmFiltered);

    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    
    if (x == 200) {
      Serial.println("Gửi ThingSpeak thành công!");
    } else {
      Serial.println("Lỗi gửi ThingSpeak. Mã lỗi HTTP: " + String(x));
    }
  }
}

// ===== SENSOR & BLYNK =====
void sendSensorData() {
  // ===== ĐỌC DHT =====
  float temp_read = dht.readTemperature();
  float hum_read = dht.readHumidity();

  if (isnan(temp_read) || isnan(hum_read)) {
    Serial.println("Loi DHT!");
    return;
  }
  
  // Lưu vào biến toàn cục 
  t = temp_read;
  h = hum_read;

  // ===== CHECK MQ135 READY =====
  bool mqReady = (millis() - startTime >= 60000);
  float ppm = 0;

  if (mqReady) {
    int raw = analogRead(MQ135_PIN);
    ppm = map(raw, 0, 4095, 0, 500);
    if (ppm < 0) ppm = 0;

    ppmFiltered = ppmFiltered * 0.7 + ppm * 0.3;
  }

  // ===== PHÂN LOẠI (Cập nhật dùng warningTemp từ AI) =====
  int levelTemp = (t >= warningTemp) ? 2 : (t >= 33 ? 1 : 0);
  int levelHum  = (h > 80) ? 2 : (h >= 70 ? 1 : 0);
  int levelTH   = max(levelTemp, levelHum);

  int levelGas = 0;
  if (mqReady) {
    levelGas = (ppmFiltered > 300) ? 2 : (ppmFiltered > 100 ? 1 : 0);
  }

  currentLevel = max(levelTH, levelGas);

  // ===== LED =====
  if (currentLevel != lastLevel) {
    lastLevel = currentLevel;
    allLEDsOff();

    if (currentLevel == 2) {
      digitalWrite(LED_RED, HIGH);
      if (Blynk.connected()) Blynk.virtualWrite(V2, 1);
    }
    else if (currentLevel == 1) {
      digitalWrite(LED_YELLOW, HIGH);
      if (Blynk.connected()) Blynk.virtualWrite(V3, 1);
    }
    else {
      digitalWrite(LED_GREEN, HIGH);
      if (Blynk.connected()) Blynk.virtualWrite(V4, 1);
    }
  }

  // ===== GỬI DATA BLYNK =====
  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, t);
    Blynk.virtualWrite(V1, h);
    Blynk.virtualWrite(V5, mqReady ? ppmFiltered : 0);
    Blynk.virtualWrite(V6, mqReady ? 1 : 0);
  }

  // ===== CẬP NHẬT OLED =====
  updateOLED(t, h, ppmFiltered, mqReady, currentLevel);

  // ===== DEBUG =====
  Serial.print("Temp: "); Serial.print(t); Serial.print(" C | Hum: "); Serial.print(h);
  if (mqReady) {
    Serial.print(" % | Chat luong: "); Serial.print(ppmFiltered);
  } else {
    Serial.print(" % | Chat luong : Cho 60s");
  }
  Serial.print(" | Level: "); Serial.println(currentLevel);
}