#include <WiFi.h>             
#include <WebServer.h>        
#include <FS.h>               
#include <SPIFFS.h>           
#include <time.h>             
#include <sys/time.h>         

const char* ssid = "smart_city";
const char* password = "senai501";

WebServer server(80);

const int trigPin = 4;
const int echoPin = 5;

#define CSV_FILE "/medicoes.csv"
#define MAX_REGISTROS 200    // Limite de linhas no CSV

// Configura a hora local usando a data/hora da compilação do sketch
void setLocalTimeFromBuild() {
  struct tm tm_build = {0};
  strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm_build);
  time_t t = mktime(&tm_build);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

// Remove a primeira linha do CSV para manter o limite de linhas
void removePrimeiraLinha() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) return;

  File tempFile = SPIFFS.open("/temp.csv", FILE_WRITE);
  if (!tempFile) {
    file.close();
    return;
  }

  bool skipFirstLine = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (skipFirstLine) {
      skipFirstLine = false;  // pula a primeira linha
      continue;
    }
    tempFile.println(line);
  }
  file.close();
  tempFile.close();

  SPIFFS.remove(CSV_FILE);
  SPIFFS.rename("/temp.csv", CSV_FILE);
}

// Conta as linhas existentes no arquivo CSV
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

// Mede a distância usando sensor HC-SR04
float measureDistance() {
  long duration;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000); // Timeout de 30ms

  if (duration == 0) return 9999;

  return (duration * 0.0343) / 2;
}

// Adiciona uma nova linha ao CSV, removendo a mais antiga se atingir o limite
void appendToCSV(String dataLine) {
  int linhas = contarLinhas();
  if (linhas >= MAX_REGISTROS) {
    Serial.println("Arquivo cheio. Removendo a linha mais antiga...");
    removePrimeiraLinha();
  }

  File file = SPIFFS.open(CSV_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo para escrita.");
    return;
  }

  file.println(dataLine);
  file.close();

  Serial.println("Registro adicionado: " + dataLine);
}

// Página principal do servidor web com tabela estilizada e botão de download
// Função para apagar todo o conteúdo do arquivo CSV
void handleClear() {
  if (SPIFFS.exists(CSV_FILE)) {
    SPIFFS.remove(CSV_FILE);
    Serial.println("Arquivo CSV limpo pelo usuário.");
  }
  server.sendHeader("Location", "/");  // Redireciona para a página principal
  server.send(303);
}

// Página principal do servidor web com botão limpar
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Histórico de Medições</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f4f6f8; margin: 0; padding: 20px; }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 20px; }";
  html += "a.download-btn, button.clear-btn { display: inline-block; background-color: #007bff; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; font-weight: 600; margin: 10px 5px; cursor: pointer; border: none; }";
  html += "a.download-btn:hover, button.clear-btn:hover { background-color: #0056b3; }";
  html += "button.clear-btn { font-size: 16px; }";
  html += "button.clear-btn .icon { margin-right: 8px; }";
  html += "table { border-collapse: collapse; width: 100%; max-width: 800px; margin: 20px auto 0 auto; background-color: white; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += "th, td { padding: 12px 15px; text-align: center; border-bottom: 1px solid #ddd; }";
  html += "th { background-color: #007bff; color: white; font-weight: 600; }";
  html += "tr:hover { background-color: #f1f9ff; }";
  html += "footer { text-align: center; margin-top: 30px; font-size: 0.9em; color: #666; }";
  html += "</style>";
  // Script para confirmar limpeza
  html += "<script>function confirmarLimpar() { return confirm('Tem certeza que deseja limpar todos os registros?'); }</script>";
  html += "</head><body>";
  html += "<h1>Histórico de Medições Registradas (&lt; 25 cm) - Máx " + String(MAX_REGISTROS) + " registros</h1>";
  html += "<div style='text-align: center;'>";
  html += "<a href='/download' class='download-btn'>📥 Baixar arquivo CSV</a>";

  // Botão limpar com ícone de lixeira
  html += "<form method='POST' action='/clear' style='display:inline;' onsubmit='return confirmarLimpar();'>";
  html += "<button type='submit' class='clear-btn'><span class='icon'>🗑️</span>Limpar Registros</button>";
  html += "</form>";

  html += "</div>";

  html += "<table>";
  html += "<thead><tr><th>Data/Hora</th><th>Distância (cm)</th></tr></thead><tbody>";

  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      int sep = line.indexOf(',');
      if (sep > 0) {
        String dataHora = line.substring(0, sep);
        String distancia = line.substring(sep + 1);
        html += "<tr><td>" + dataHora + "</td><td>" + distancia + "</td></tr>";
      }
    }
    file.close();
  } else {
    html += "<tr><td colspan='2'>Nenhum dado disponível.</td></tr>";
  }

  html += "</tbody></table>";
  html += "<footer>Projeto de Medição Ultrassônica &copy; 2025</footer>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Manipula o download do arquivo CSV
void handleDownload() {
  File file = SPIFFS.open(CSV_FILE, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Arquivo não encontrado");
    return;
  }

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=\"medicoes.csv\"");
  server.streamFile(file, "text/csv");
  file.close();
}

void setup() {
  
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando à rede");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());

  setLocalTimeFromBuild();
  
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.begin();
}

void loop() {
  float dist = measureDistance();

  if (dist < 25.0) {  // Só registra se a distância for menor que 25 cm
    struct tm timeinfo;
    String dataHora = "N/A";

    if (getLocalTime(&timeinfo)) {
      char timeStringBuff[64];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%d/%m/%Y %H:%M:%S", &timeinfo);
      dataHora = String(timeStringBuff);
    }

    String dataLine = dataHora + "," + String(dist, 2);
    appendToCSV(dataLine);

    delay(1000);
  }

  server.handleClient();
}
