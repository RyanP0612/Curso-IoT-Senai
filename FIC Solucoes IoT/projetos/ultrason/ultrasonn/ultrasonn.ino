#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>
#include <sys/time.h>

// ===== CONFIGURAÇÃO WI-FI =====
const char* ssid = "smart_city";
const char* password = "senai501";

WebServer server(80);

// ===== PINOS DO SENSOR =====
const int trigPin = 4;
const int echoPin = 5;

// ===== CONFIGURAÇÃO DE ARQUIVO =====
#define CSV_FILE "/medicoes.csv"
#define MAX_REGISTROS 200 // limite de linhas no CSV

// ===== VARIÁVEIS DA ÚLTIMA LEITURA =====
String ultimaDataHora = "N/A";
float ultimaDistancia = -1.0;

// ===== AJUSTA DATA/HORA A PARTIR DA COMPILAÇÃO =====
void setLocalTimeFromBuild() {
  struct tm tm_build = {0};
  strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm_build);
  time_t t = mktime(&tm_build);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

// ===== REMOVE PRIMEIRA LINHA DO CSV =====
void removePrimeiraLinha() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) return;

  File tempFile = SPIFFS.open("/temp.csv", FILE_WRITE);
  if (!tempFile) {
    file.close();
    return;
  }

  bool primeira = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (primeira) { primeira = false; continue; }
    tempFile.println(line);
  }

  file.close();
  tempFile.close();
  SPIFFS.remove(CSV_FILE);
  SPIFFS.rename("/temp.csv", CSV_FILE);
}

// ===== CONTA LINHAS DO CSV =====
int contarLinhas() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) return 0;
  int linhas = 0;
  while (file.available()) {
    file.readStringUntil('\n');
    linhas++;
  }
  file.close();
  return linhas;
}

// ===== LÊ A ÚLTIMA LINHA DO CSV PARA MEMÓRIA =====
void carregarUltimaDoCSV() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) {
    ultimaDataHora = "N/A";
    ultimaDistancia = -1.0;
    return;
  }
  String lastLine = "";
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) lastLine = line;
  }
  file.close();
  if (lastLine.length() > 0) {
    int sep = lastLine.indexOf(',');
    if (sep > 0) {
      ultimaDataHora = lastLine.substring(0, sep);
      ultimaDistancia = lastLine.substring(sep + 1).toFloat();
    }
  }
}

// ===== MEDIÇÃO DO HC-SR04 =====
float measureDistance() {
  long duration;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 9999.0;
  return (duration * 0.0343) / 2.0;
}

// ===== OBTÉM DATA/HORA ATUAL =====
String getNowString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "N/A";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

// ===== ADICIONA LINHA AO CSV (BUFFER CIRCULAR) =====
void appendToCSVAndUpdateLast(const String &dataHora, float distancia) {
  if (contarLinhas() >= MAX_REGISTROS) {
    removePrimeiraLinha();
  }
  File file = SPIFFS.open(CSV_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("Erro ao abrir arquivo para escrita.");
    return;
  }
  String line = dataHora + "," + String(distancia, 2);
  file.println(line);
  file.close();

  ultimaDataHora = dataHora;
  ultimaDistancia = distancia;

  Serial.println("Registro adicionado: " + line);
}

// ===== LIMPA O CSV =====
void handleClear() {
  if (SPIFFS.exists(CSV_FILE)) {
    SPIFFS.remove(CSV_FILE);
    Serial.println("Arquivo CSV limpo pelo usuário.");
  }
  ultimaDataHora = "N/A";
  ultimaDistancia = -1.0;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== PÁGINA PRINCIPAL =====
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Histórico de Medições</title>";
  html += "<style>";
  html += "body{font-family:Segoe UI,Tahoma,Geneva,Verdana,sans-serif;background:#f4f6f8;padding:20px;}";
  html += "h1{text-align:center;color:#333;}";
  html += ".btn-container{display:flex;flex-wrap:wrap;justify-content:center;gap:10px;margin-bottom:15px;}";
  html += "a,button{background:#007bff;color:white;padding:10px 20px;text-decoration:none;border-radius:5px;font-weight:600;border:none;cursor:pointer;}";
  html += "a:hover,button:hover{background:#0056b3;}";
  html += "table{border-collapse:collapse;width:100%;max-width:800px;margin:auto;background:#fff;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += "th,td{padding:12px;text-align:center;border-bottom:1px solid #ddd;}";
  html += "th{background:#007bff;color:white;}";
  html += "tr:hover{background:#f1f9ff;}";
  html += "footer{text-align:center;margin-top:30px;color:#666;}";
  html += "</style></head><body>";
  html += "<h1>Histórico de Medições (&lt; 25 cm)</h1>";

  // Botões organizados com flexbox
  html += "<div class='btn-container'>";
  html += "<a href='/download'>📥 Baixar CSV</a>";
  html += "<a href='/api/ultima'>Última API</a>";
  html += "<a href='/api/todas'>Todas API</a>";
  html += "<a href='/api/ler'>Forçar Leitura</a>";
  html += "<form method='POST' action='/clear' onsubmit='return confirm(\"Limpar registros?\");'>";
  html += "<button type='submit'>🗑️ Limpar</button>";
  html += "</form>";
  html += "</div>";

  html += "<table><thead><tr><th>Data/Hora</th><th>Distância (cm)</th></tr></thead><tbody>";

  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      int sep = line.indexOf(',');
      if (sep > 0) {
        html += "<tr><td>" + line.substring(0, sep) + "</td><td>" + line.substring(sep + 1) + "</td></tr>";
      }
    }
    file.close();
  } else {
    html += "<tr><td colspan='2'>Nenhum registro</td></tr>";
  }

  html += "</tbody></table><footer>Projeto IoT Ultrassônico &copy; 2025</footer></body></html>";
  server.send(200, "text/html", html);
}


// ===== DOWNLOAD CSV =====
void handleDownload() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Arquivo não encontrado");
    return;
  }
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=\"medicoes.csv\"");
  server.streamFile(file, "text/csv");
  file.close();
}

// ===== API: Última leitura =====
void apiUltima() {
  String json = "{\"dataHora\":\"" + ultimaDataHora + "\",\"distancia_cm\":" + String(ultimaDistancia, 2) + "}";
  server.send(200, "application/json", json);
}

// ===== API: Todas as leituras =====
void apiTodas() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) {
    server.send(200, "application/json", "[]");
    return;
  }
  String json = "[";
  bool first = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    int sep = line.indexOf(',');
    if (sep > 0) {
      if (!first) json += ",";
      json += "{\"dataHora\":\"" + line.substring(0, sep) + "\",\"distancia_cm\":" + line.substring(sep + 1) + "}";
      first = false;
    }
  }
  json += "]";
  file.close();
  server.send(200, "application/json", json);
}

// ===== API: Forçar leitura =====
void apiLer() {
  float dist = measureDistance();
  String dataHora = getNowString();
  if (dist < 25.0) {
    appendToCSVAndUpdateLast(dataHora, dist);
  } else {
    ultimaDataHora = dataHora;
    ultimaDistancia = dist;
  }
  String json = "{\"dataHora\":\"" + dataHora + "\",\"distancia_cm\":" + String(dist, 2) + "}";
  server.send(200, "application/json", json);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return;
  }

  if (!SPIFFS.exists(CSV_FILE)) {
    File f = SPIFFS.open(CSV_FILE, FILE_WRITE);
    if (f) f.close();
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando à rede");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());

  setLocalTimeFromBuild();
  carregarUltimaDoCSV();

  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/api/ultima", apiUltima);
  server.on("/api/todas", apiTodas);
  server.on("/api/ler", apiLer);
  server.begin();

  Serial.println("Servidor HTTP iniciado.");
}

// ===== LOOP =====
void loop() {
  float dist = measureDistance();
  if (dist < 25.0) {
    appendToCSVAndUpdateLast(getNowString(), dist);
    delay(1000);
  }
  server.handleClient();
}
