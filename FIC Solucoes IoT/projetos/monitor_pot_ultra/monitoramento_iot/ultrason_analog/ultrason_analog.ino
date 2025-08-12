#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <sys/time.h>

const char* ssid = "ESP32-SENAI-9";
const char* password = "12345678";

// Pinos dos sensores
const int potPin = 34;
const int trigPin = 4;
const int echoPin = 5;

// Limite de registros
#define MAX_REGISTROS 500

struct Registro {
  float distancia;
  String dataHora;
};
Registro registros[MAX_REGISTROS];
int registroIndex = 0;

WebServer server(80);

// ---------- Funções ----------
float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 9999;
  return (duration * 0.0343) / 2;
}

void setLocalTimeFromBuild() {
  struct tm tm_build = {0};
  strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm_build);
  time_t t = mktime(&tm_build);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

// ---------- HTML da Página Inicial ----------
const char* indexPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Página Inicial</title>
  <style>
    body { font-family: Arial; background: #e0f7fa; color: #333; text-align: center; padding: 50px; font-size: 80px; }
    h1 { color: #00796b; }
    button { padding: 25px 40px; font-size: 20px; background-color: #00796b; color: white; border: none; border-radius: 5px; cursor: pointer; }
    button:hover { background-color: #004d40; }
  </style>
</head>
<body>
  <h1>Bem-vindo ao Projeto IoT</h1>
  <p>Deseja monitorar os sensores?</p>
  <form action="/monitor"><button>Monitorar Sensores</button></form>
</body>
</html>
)rawliteral";

// ---------- Página de Monitoramento ----------
String generateMonitorPage(int potValue, float voltage, float distance) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta http-equiv="refresh" content="2">
  <title>Monitoramento</title>
  <style>
    body { font-family: Arial; background: #f2f2f2; color: #333; text-align: center; padding: 20px; font-size: 45px; }
    h1 { color: #004d99; }
    p { font-size: 18px; }
    a { display: inline-block; margin-top: 20px; padding: 10px 20px; background: #004d99; color: white; text-decoration: none; border-radius: 5px; }
  </style>
</head>
<body>
  <h1>Monitoramento de Sensores</h1>
)rawliteral";

  html += "<p><strong>Valor ADC (Potenciômetro):</strong> " + String(potValue) + "</p>";
  html += "<p><strong>Tensão:</strong> " + String(voltage, 2) + " V</p>";
  html += "<p><strong>Distância (Ultrassônico):</strong> " + String(distance, 2) + " cm</p>";
  html += "<a href='/'>Voltar</a><br><br>";
  html += "<a href='/historico200'>Ver histórico &lt; 200 cm</a></body></html>";

  return html;
}

// ---------- Página de Histórico &lt; 200 cm ----------
void handleHistorico200() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>Histórico &lt; 200 cm</title>";
  html += "<style>";
  html += "body { font-family: Arial; background: #fff; color: #333; text-align: center; padding: 20px; }";
  html += "table { margin: auto; border-collapse: collapse; width: 90%; max-width: 800px; }";
  html += "th, td { border: 1px solid #ccc; padding: 10px; font-size: 16px; }";
  html += "th { background-color: #004d99; color: white; }";
  html += "tr:nth-child(even) { background-color: #e6f2ff; }";
  html += "a { display: inline-block; margin-top: 20px; padding: 10px 20px; background: #00796b; color: white; text-decoration: none; border-radius: 5px; }";
  html += "</style></head><body>";
  html += "<h1>Histórico de Distâncias &lt; 200 cm</h1>";
  html += "<table><tr><th>Data/Hora</th><th>Distância (cm)</th></tr>";

  for (int i = 0; i < registroIndex; i++) {
    if (registros[i].distancia < 200.0) {
      html += "<tr><td>" + registros[i].dataHora + "</td><td>" + String(registros[i].distancia, 2) + "</td></tr>";
    }
  }

  html += "</table><br><a href='/'>Voltar à Página Inicial</a></body></html>";
  server.send(200, "text/html", html);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  WiFi.softAP(ssid, password);
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  setLocalTimeFromBuild();

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", indexPage);
  });

  server.on("/monitor", HTTP_GET, []() {
    int potValue = analogRead(potPin);
    float voltage = (potValue / 4095.0) * 3.3;
    float distance = measureDistance();

    struct tm timeinfo;
    String dataHora = "N/A";
    if (getLocalTime(&timeinfo)) {
      char timeStringBuff[64];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%d/%m/%Y %H:%M:%S", &timeinfo);
      dataHora = String(timeStringBuff);
    }

    if (registroIndex < MAX_REGISTROS) {
      registros[registroIndex].distancia = distance;
      registros[registroIndex].dataHora = dataHora;
      registroIndex++;
    }

    String page = generateMonitorPage(potValue, voltage, distance);
    server.send(200, "text/html", page);
  });

  server.on("/historico200", HTTP_GET, handleHistorico200);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Página não encontrada");
  });

  server.begin();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
}
