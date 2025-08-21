#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ===== CONFIGURAÇÕES DE REDE =====
const char* ssid     = "lele";       // Substitua pelo seu WiFi
const char* password = "12345678";      // Senha do WiFi

// ===== CONFIGURAÇÃO DA API =====
// Substituir pelo script gerado para a sua planilha
const char* serverName = "https://script.google.com/macros/s/AKfycbzxeaV2tsR4D_TjCl4UA9twhI-533K0L_zZsM0Wc5GBUghK315wFTH7_IwOqQmICx4y/exec";  

// ===== SENSOR DHT =====
#define DHTPIN 14        // GPIO14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== CONFIGURAÇÃO NTP =====
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // fuso -3h, atualização a cada 60s

void setup() {
  Serial.begin(115200);

  // Conexão WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Inicializa sensor DHT
  dht.begin();

  // Inicializa NTP
  timeClient.begin();
}

void loop() {
  // Atualiza NTP
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  // Converte para data legível
  struct tm *ptm = gmtime((time_t *)&epochTime);
  int dia = ptm->tm_mday;
  int mes = ptm->tm_mon + 1;
  int ano = ptm->tm_year + 1900;

  String dataHora = String(ano) + "-" + (mes < 10 ? "0" : "") + String(mes) + "-" +
                    (dia < 10 ? "0" : "") + String(dia) + " " +
                    timeClient.getFormattedTime(); // YYYY-MM-DD HH:MM:SS

  // Lê DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler DHT11");
    delay(2000);
    return;
  }

  Serial.printf("  Temperatura: %.1f °C |  Umidade: %.1f %% |  %s\n", t, h, dataHora.c_str());

  // Envia para API se WiFi conectado
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);               // URL da API
    http.addHeader("Content-Type", "application/json");

    // Monta JSON com API Key
    String jsonData = "{";
    jsonData += "\"dataHora\":\"" + dataHora + "\",";
    jsonData += "\"temperatura\":" + String(t, 1) + ",";
    jsonData += "\"umidade\":" + String(h, 1) + ",";
    jsonData += "\"sensor\":\"Temp - Umid sala_Uaian\",";
    jsonData += "\"apiKey\":\"SOLUCOES_IOT_SENAI501\"";
    jsonData += "}";

    // Envia POST
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.printf(" Dados enviados! Código: %d\n", httpResponseCode);
      Serial.println("Resposta: " + payload);
    } else {
      Serial.printf(" Erro no envio. Código: %d\n", httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi desconectado");
  }

  delay(10000); // envia a cada 10 segundos
}
