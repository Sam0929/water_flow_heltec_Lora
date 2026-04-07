#include <Wire.h>
#include "SSD1306Wire.h"
#include "pins_arduino.h"

// ======================================================
// OLED Heltec
// ======================================================
SSD1306Wire display(0x3C, SDA_OLED, SCL_OLED);

// ======================================================
// Sensor YF-S201
// ======================================================
#define FLOW_SENSOR_PIN 4   // GPIO do sensor
volatile uint32_t pulseCount = 0;

unsigned long lastMeasureTime = 0;
float totalLiters = 0.0f;

// ======================================================
// Funções de energia / reset do display Heltec
// ======================================================
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
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
// Interrupt do sensor
// ======================================================
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ======================================================
// Setup
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Sensor de fluxo
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, RISING);

  // OLED Heltec
  VextON();
  displayReset();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Inicializando...");
  display.display();

  lastMeasureTime = millis();
}

// ======================================================
// Loop principal
// ======================================================
void loop() {
  unsigned long now = millis();

  // Atualiza a cada 1 segundo
  if (now - lastMeasureTime >= 1000) {
    lastMeasureTime += 1000;

    // Copia e zera o contador com segurança
    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    // YF-S201:
    // Freq(Hz) = 7.5 * Q(L/min)
    // Q(L/min) = Hz / 7.5
    float flowLMin = pulses / 7.5f;
    float flowLHour = flowLMin * 60.0f;

    // Volume acumulado em 1 segundo
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
  }
}