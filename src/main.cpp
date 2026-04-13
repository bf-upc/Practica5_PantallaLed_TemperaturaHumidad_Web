#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <WebServer.h>

// ── Pantalla ──────────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 8
#define SCL_PIN 9
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// ── WiFi AP ───────────────────────────────────────────────────────
#define AP_SSID "ESP32-Sensor"
#define AP_PASS "12345678"       // mínimo 8 caracteres
#define AP_IP   "192.168.4.1"   // IP por defecto del AP

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0   aht;
WebServer        server(80);

// Variables globales de lectura
float temperatura = 0.0;
float humedad     = 0.0;
unsigned long lastRead = 0;

// ══════════════════════════════════════════════════════════════════
//  HTML de la página web
// ══════════════════════════════════════════════════════════════════
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="5">  
  <title>ESP32 Sensor</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', sans-serif;
      background: #0f172a;
      color: #e2e8f0;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      padding: 20px;
    }
    h1 {
      font-size: 1.4rem;
      color: #94a3b8;
      margin-bottom: 30px;
      letter-spacing: 2px;
      text-transform: uppercase;
    }
    .cards {
      display: flex;
      gap: 20px;
      flex-wrap: wrap;
      justify-content: center;
    }
    .card {
      background: #1e293b;
      border-radius: 16px;
      padding: 30px 40px;
      text-align: center;
      border: 1px solid #334155;
      min-width: 160px;
    }
    .card .icon { font-size: 2.5rem; margin-bottom: 10px; }
    .card .label {
      font-size: 0.8rem;
      color: #64748b;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 8px;
    }
    .card .value {
      font-size: 2.8rem;
      font-weight: 700;
    }
    .card .unit {
      font-size: 1rem;
      color: #64748b;
      margin-left: 4px;
    }
    .temp .value { color: #f97316; }
    .hum  .value { color: #38bdf8; }
    .footer {
      margin-top: 30px;
      font-size: 0.75rem;
      color: #475569;
    }
  </style>
</head>
<body>
  <h1>&#127777; ESP32-S3 · Sensor AHT</h1>
  <div class="cards">
    <div class="card temp">
      <div class="icon">🌡️</div>
      <div class="label">Temperatura</div>
      <div class="value">TEMP_VAL<span class="unit">°C</span></div>
    </div>
    <div class="card hum">
      <div class="icon">💧</div>
      <div class="label">Humedad</div>
      <div class="value">HUM_VAL<span class="unit">%</span></div>
    </div>
  </div>
  <div class="footer">Actualización automática cada 5 segundos</div>
</body>
</html>
)rawliteral";

// ── Endpoint JSON para apps / fetch externo ───────────────────────
void handleJSON() {
  String json = "{";
  json += "\"temperatura\":" + String(temperatura, 1) + ",";
  json += "\"humedad\":"     + String(humedad, 1);
  json += "}";
  server.send(200, "application/json", json);
}

// ── Endpoint página principal ─────────────────────────────────────
void handleRoot() {
  String page = String(HTML_PAGE);
  page.replace("TEMP_VAL", String(temperatura, 1));
  page.replace("HUM_VAL",  String(humedad, 1));
  server.send(200, "text/html", page);
}

// ══════════════════════════════════════════════════════════════════
//  PANTALLA
// ══════════════════════════════════════════════════════════════════
void mostrarDatos() {
  display.clearDisplay();

  // Cabecera
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("  Sensor AHT");

  // Temperatura
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.print("Temp:");
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(temperatura, 1);
  display.print(" C");

  // Separador
  display.drawLine(0, 44, 127, 44, SSD1306_WHITE);

  // Humedad
  display.setTextSize(1);
  display.setCursor(0, 47);
  display.print("Hum: ");
  display.print(humedad, 1);
  display.print(" %");

  // IP en pie
  display.setCursor(0, 56);
  display.print(AP_IP);

  display.display();
}

void mostrarArranque() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi AP: ");
  display.println(AP_SSID);
  display.print("Pass:    ");
  display.println(AP_PASS);
  display.print("IP:      ");
  display.println(AP_IP);
  display.setCursor(0, 40);
  display.println("Abre el navegador");
  display.println("y visita la IP!");
  display.display();
}

// ══════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // Pantalla
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: SSD1306 no encontrado");
    while (true);
  }

  // Sensor
  if (!aht.begin()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ERROR:");
    display.println("Sensor AHT no");
    display.println("encontrado!");
    display.display();
    while (true);
  }

  // WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP iniciado: %s  IP: %s\n", AP_SSID, AP_IP);

  // Rutas del servidor
  server.on("/",     handleRoot);
  server.on("/json", handleJSON);
  server.begin();
  Serial.println("Servidor HTTP iniciado");

  // Mostrar info de conexión en pantalla 4 segundos
  mostrarArranque();
  delay(4000);
}

// ══════════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════════
void loop() {
  server.handleClient(); // ← siempre primero, sin bloquear

  // Leer sensor cada 2 segundos
  if (millis() - lastRead >= 2000) {
    lastRead = millis();

    sensors_event_t hum_ev, temp_ev;
    aht.getEvent(&hum_ev, &temp_ev);
    temperatura = temp_ev.temperature;
    humedad     = hum_ev.relative_humidity;

    Serial.printf("T: %.1f°C  H: %.1f%%\n", temperatura, humedad);
    mostrarDatos();
  }
}