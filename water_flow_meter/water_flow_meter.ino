#include <Wire.h>
#include "SSD1306Wire.h"
#include "pins_arduino.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// ======================================================
// WIFI
// ======================================================
const char* ssid = "SamuelWifi";
const char* password = "zvn1829d";

// IP do backend CoAP
IPAddress serverIP(172, 22, 239, 163);
const uint16_t serverPort = 5683;
const char* coapResource = "vazao";   // recurso CoAP: coap://172.22.239.163:5683/vazao

// ======================================================
// OLED Heltec
// ======================================================
SSD1306Wire display(0x3C, SDA_OLED, SCL_OLED);

// ======================================================
// CoAP
// ======================================================
WiFiUDP udp;
Coap coap(udp);

// ======================================================
// Sensor YF-S201
// ======================================================
#define FLOW_SENSOR_PIN 4
volatile uint32_t pulseCount = 0;

unsigned long lastMeasureTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastWifiCheck = 0;

float totalLiters = 0.0f;

// Janela e suavização
const uint8_t WINDOW_SIZE = 8; // 8 amostras de 250 ms = 2 s
float flowWindow[WINDOW_SIZE] = {0};
uint8_t flowIndex = 0;
bool flowWindowFilled = false;

float filteredFlow = 0.0f; // média móvel exponencial
const float ALPHA = 0.30f;  // 0.0 = muito suave, 1.0 = sem filtro

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
// Utilitários
// ======================================================
float readAndResetPulses() {
  noInterrupts();
  uint32_t pulses = pulseCount;
  pulseCount = 0;
  interrupts();
  return (float)pulses;
}

void pushFlowSample(float value) {
  flowWindow[flowIndex] = value;
  flowIndex = (flowIndex + 1) % WINDOW_SIZE;
  if (flowIndex == 0) {
    flowWindowFilled = true;
  }
}

float getFlowAverage() {
  uint8_t count = flowWindowFilled ? WINDOW_SIZE : flowIndex;
  if (count == 0) return 0.0f;

  float sum = 0.0f;
  for (uint8_t i = 0; i < count; i++) {
    sum += flowWindow[i];
  }
  return sum / count;
}

float getFlowMin() {
  uint8_t count = flowWindowFilled ? WINDOW_SIZE : flowIndex;
  if (count == 0) return 0.0f;

  float m = flowWindow[0];
  for (uint8_t i = 1; i < count; i++) {
    if (flowWindow[i] < m) m = flowWindow[i];
  }
  return m;
}

float getFlowMax() {
  uint8_t count = flowWindowFilled ? WINDOW_SIZE : flowIndex;
  if (count == 0) return 0.0f;

  float m = flowWindow[0];
  for (uint8_t i = 1; i < count; i++) {
    if (flowWindow[i] > m) m = flowWindow[i];
  }
  return m;
}

// ======================================================
// Envio CoAP
// ======================================================
void sendToServer(float rawFlow, float smoothedFlow, float total, float lph, uint32_t pulses, float avgWindow) {
  if (WiFi.status() != WL_CONNECTED) return;

  // JSON compacto para reduzir payload no UDP
  char payload[220];
  snprintf(payload, sizeof(payload),
           "{\"flow\":%.2f,\"smooth\":%.2f,\"avg\":%.2f,\"total\":%.3f,\"lph\":%.2f,\"pulses\":%lu}",
           rawFlow, smoothedFlow, avgWindow, total, lph, (unsigned long)pulses);

  // Envio não-confirmável para ficar mais leve e fluido
  coap.send(
    serverIP,
    serverPort,
    coapResource,
    COAP_NON,
    COAP_PUT,
    nullptr,
    0,
    (const uint8_t*)payload,
    strlen(payload),
    COAP_APPLICATION_JSON
  );

  Serial.print("CoAP enviado: ");
  Serial.println(payload);
}

// ======================================================
// WiFi
// ======================================================
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  static bool reconnecting = false;

  if (!reconnecting) {
    reconnecting = true;
    Serial.println("WiFi caiu, tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  if (millis() - lastWifiCheck >= 3000) {
    lastWifiCheck = millis();
    Serial.print("Status WiFi: ");
    Serial.println(WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    reconnecting = false;
    Serial.println("WiFi reconectado!");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  }
}

// ======================================================
// OLED
// ======================================================
void updateOLED(float rawFlow, float smoothed, float lph, float avgFlow, float minFlow, float maxFlow, float total) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(0, 0, "YF-S201 / ESP32");

  if (rawFlow < 0.01f) {
    display.drawString(0, 14, "Vazao: sem fluxo");
  } else {
    display.drawString(0, 14, "Vazao: " + String(rawFlow, 2) + " L/min");
    display.drawString(0, 26, "Suave: " + String(smoothed, 2) + " L/min");
  }

  display.drawString(0, 38, "L/h:   " + String(lph, 2));
  display.drawString(0, 48, "Tot: " + String(total, 3) + " L");

  display.display();
}

// ======================================================
// Setup
// ======================================================
void setup() {
  Serial.begin(115200);

  VextON();
  displayReset();
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  display.clear();
  display.drawString(0, 0, "Inicializando Heltec...");
  display.display();
  delay(800);

  // Sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // reduz latência e melhora estabilidade
  WiFi.begin(ssid, password);

  display.clear();
  display.drawString(0, 0, "Conectando WiFi...");
  display.display();

  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    display.clear();
    display.drawString(0, 0, "Conectando WiFi");
    display.drawString(0, 14, String("Status: ") + WiFi.status());
    display.display();
  }

  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  // CoAP
  coap.start(serverPort);

  display.clear();
  display.drawString(0, 0, "WiFi conectado!");
  display.drawString(0, 14, WiFi.localIP().toString());
  display.drawString(0, 30, "CoAP ativo");
  display.display();
  delay(1500);

  lastMeasureTime = millis();
  lastSendTime = millis();
  lastWifiCheck = millis();
}

// ======================================================
// Loop
// ======================================================
void loop() {
  ensureWiFi();
  coap.loop();

  unsigned long now = millis();

  // Amostragem mais fluida: 250 ms
  if (now - lastMeasureTime >= 250) {
    unsigned long elapsed = now - lastMeasureTime;
    lastMeasureTime = now;

    float pulses = readAndResetPulses();

    // Fórmula do YF-S201:
    // aproximadamente 7.5 pulsos por segundo = 1 L/min
    // aqui usamos o tempo real decorrido para ficar mais preciso
    float flowLMin = 0.0f;
    if (elapsed > 0) {
      float pulsesPerSecond = (pulses * 1000.0f) / (float)elapsed;
      flowLMin = pulsesPerSecond / 7.5f;
    }

    // Filtro exponencial para suavizar ruído
    filteredFlow = (filteredFlow == 0.0f) ? flowLMin : (ALPHA * flowLMin + (1.0f - ALPHA) * filteredFlow);

    float flowLHour = flowLMin * 60.0f;
    totalLiters += (flowLMin * ((float)elapsed / 1000.0f)) / 60.0f;

    pushFlowSample(filteredFlow);

    float avgFlow = getFlowAverage();
    float minFlow = getFlowMin();
    float maxFlow = getFlowMax();

    Serial.print("Pulsos: ");
    Serial.print((unsigned long)pulses);
    Serial.print(" | Vazao bruta: ");
    Serial.print(flowLMin, 2);
    Serial.print(" L/min | Filtrada: ");
    Serial.print(filteredFlow, 2);
    Serial.print(" L/min | Media: ");
    Serial.print(avgFlow, 2);
    Serial.print(" L/min | Total: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    updateOLED(flowLMin, filteredFlow, flowLHour, avgFlow, minFlow, maxFlow, totalLiters);

    // Envia a cada 5 segundos
    if (now - lastSendTime >= 5000) {
      sendToServer(flowLMin, filteredFlow, totalLiters, flowLHour, (uint32_t)pulses, avgFlow);
      lastSendTime = now;
    }
  }

  // Pequena pausa para aliviar o loop sem travar a leitura
  delay(2);
}
