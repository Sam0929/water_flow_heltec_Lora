#include <Wire.h>
#include "SSD1306Wire.h"
#include "pins_arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ======================================================
// WIFI
// ======================================================
const char* ssid = "SamuelWifi";
const char* password = "zvn1829d";

String serverURL = "http://192.168.X.X:8000/vazao";

// ======================================================
// OLED Heltec
// ======================================================
SSD1306Wire display(0x3C, SDA_OLED, SCL_OLED);

// ======================================================
// Sensor YF-S201
// ======================================================
#define FLOW_SENSOR_PIN 4
volatile uint32_t pulseCount = 0;

unsigned long lastMeasureTime = 0;
unsigned long lastSendTime = 0;

float totalLiters = 0.0f;

// ======================================================
// Funções energia OLED
// ======================================================
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void displayReset() {
  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, HIGH);
  delay(1);
  digitalWrite(RST_OLED, LOW);
  delay(1);
  digitalWrite(RST_OLED, HIGH);
  delay(1);
}

// ======================================================
// Interrupt sensor
// ======================================================
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ======================================================
// Envio para servidor
// ======================================================
void sendToServer(float flow, float total) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    String body = "{";
    body += "\"flow\":" + String(flow, 2) + ",";
    body += "\"total\":" + String(total, 3);
    body += "}";

    int code = http.POST(body);

    Serial.print("HTTP Response: ");
    Serial.println(code);

    http.end();
  }
}

// ======================================================
// Setup
// ======================================================
void setup() {

  Serial.begin(115200);
  delay(1000);

  // WIFI
  Serial.println("Conectando WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
  delay(5000);

  // Sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);

  // OLED
  VextON();
  displayReset();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  display.clear();
  display.drawString(0, 0, "Inicializando...");
  display.display();

  // ==============================
// WIFI
// ==============================
display.clear();
display.drawString(0, 0, "Conectando WiFi...");
display.display();

Serial.println("Conectando WiFi...");
WiFi.begin(ssid, password);

// animação de pontos
int dots = 0;

while (WiFi.status() != WL_CONNECTED) {

  delay(500);
  Serial.print(".");

  display.clear();
  display.drawString(0, 0, "Conectando WiFi");

  String loading = "";
  for(int i=0;i<dots;i++){
    loading += ".";
  }

  display.drawString(0, 16, loading);
  display.display();

  dots++;
  if(dots > 6) dots = 0;
}

// ==============================
// WIFI conectado
// ==============================

Serial.println("");
Serial.println("WiFi conectado!");
Serial.print("IP ESP32: ");
Serial.println(WiFi.localIP());

display.clear();
display.drawString(0, 0, "WiFi conectado!");

display.drawString(0, 16, "IP:");

display.drawString(0, 30, WiFi.localIP().toString());

display.display();

delay(5000);

  lastMeasureTime = millis();
}

// ======================================================
// Loop
// ======================================================
void loop() {

  unsigned long now = millis();

  if (now - lastMeasureTime >= 1000) {

    lastMeasureTime += 1000;

    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float flowLMin = pulses / 7.5f;
    float flowLHour = flowLMin * 60.0f;

    totalLiters += flowLMin / 60.0f;

    // Serial
    Serial.print("Pulsos: ");
    Serial.print(pulses);
    Serial.print(" | Vazao: ");
    Serial.print(flowLMin, 2);
    Serial.print(" L/min | ");
    Serial.print(flowLHour, 2);
    Serial.print(" L/h | Total: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    // OLED
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0, "YF-S201 / ESP32");

    if (flowLMin < 0.01f) {
      display.drawString(0, 16, "Vazao: sem fluxo");
    } else {
      display.drawString(0, 16, "Vazao: " + String(flowLMin, 2) + " L/min");
      display.drawString(0, 30, "L/h:   " + String(flowLHour, 2));
    }

    display.drawString(0, 46, "Total: " + String(totalLiters, 3) + " L");

    display.display();

    // ==================================================
    // ENVIO PARA SERVIDOR A CADA 5 SEGUNDOS
    // ==================================================
    if (now - lastSendTime > 5000) {

      sendToServer(flowLMin, totalLiters);
      lastSendTime = now;
    }
  }
}