#include <Wire.h>
#include "SSD1306Wire.h"
#include "pins_arduino.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// ======================================================
// WIFI e ThingsBoard Config
// ======================================================
const char* ssid = "SamuelWifi";
const char* password = "zvn1829d";

// Domínio do ThingsBoard (pode ser o demo ou a sua instância própria)
const char* tb_host = "demo.thingsboard.io"; 
const char* TOKEN = "GWCiZFYxsqPZxNfnWmGQ"; 

IPAddress serverIP; // Será resolvido no setup via DNS
const int coapPort = 5683;

WiFiUDP udp;
Coap coap(udp);

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

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// Callback para confirmar recebimento (opcional, mas bom para debug)
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("ACK recebido do ThingsBoard!");
}

// ======================================================
// Envio para nuvem (CoAP com JSON)
// ======================================================
void sendToServer(float flow, float total) {
  if (WiFi.status() == WL_CONNECTED) {
    
    // 1. Monta a rota (URI) exigida pelo ThingsBoard
    String uri = String("api/v1/") + TOKEN + "/telemetry";
    
    // 2. Monta o payload em formato JSON
    String payload = "{\"flow\":" + String(flow, 2) + ",\"total\":" + String(total, 3) + "}";

    Serial.print("Enviando via CoAP: ");
    Serial.println(payload);

    // 3. Dispara o pacote UDP para a nuvem
    uint16_t msgid = coap.send(
      serverIP, 
      coapPort, 
      uri.c_str(), 
      COAP_CON,   // Mensagem Confirmável (exige ACK do servidor)
      COAP_POST,  // ThingsBoard exige POST para telemetria
      NULL, 
      0, 
      (uint8_t *)payload.c_str(), 
      payload.length(),
      COAP_APPLICATION_JSON
    );
    
    if(msgid > 0) {
      Serial.println("Pacote despachado na rede!");
    } else {
      Serial.println("Falhou ao tentar enviar pacote.");
    }
  }
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
  
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);

  Serial.println("Conectando WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi conectado!");
  
  // Resolve o domínio do ThingsBoard para pegar o IP (DNS)
  WiFi.hostByName(tb_host, serverIP);
  Serial.print("IP do ThingsBoard resolvido: ");
  Serial.println(serverIP);

  display.clear();
  display.drawString(0, 0, "WiFi conectado!");
  display.drawString(0, 16, WiFi.localIP().toString());
  display.display();
  delay(2000);

  // Inicia o CoAP
  coap.response(callback_response);
  coap.start();

  lastMeasureTime = millis();
}

// ======================================================
// Loop
// ======================================================
void loop() {
  // Mantém a escuta do CoAP ativa (para pegar o ACK do ThingsBoard)
  coap.loop();

  unsigned long now = millis();

  // Atualiza OLED a cada 1 segundo
  if (now - lastMeasureTime >= 1000) {
    lastMeasureTime += 1000;

    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float flowLMin = pulses / 7.5f;
    float flowLHour = flowLMin * 60.0f;
    totalLiters += flowLMin / 60.0f;

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "YF-S201 -> ThingsBoard");

    if (flowLMin < 0.01f) {
      display.drawString(0, 16, "Vazao: sem fluxo");
    } else {
      display.drawString(0, 16, "Vazao: " + String(flowLMin, 2) + " L/min");
      display.drawString(0, 30, "L/h:   " + String(flowLHour, 2));
    }

    display.drawString(0, 46, "Total: " + String(totalLiters, 3) + " L");
    display.display();

    // Envia os dados via CoAP a cada 5 segundos
    if (now - lastSendTime > 100) {
      sendToServer(flowLMin, totalLiters);
      lastSendTime = now;
    }
  }
}