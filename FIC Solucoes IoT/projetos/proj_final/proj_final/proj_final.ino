#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>

// ====== CONFIG WIFI ======
const char* ssid = "SUA_REDE_WIFI";
const char* password = "SENHA_WIFI";

// ====== GOOGLE SHEETS ======
const String urlGoogleSheets = "https://script.google.com/macros/s/AKfycbzXGyhN9Hdx9wCFjMXbu62L37rPDRbaadPuv-4e4n2PX1_fXysHLcbAluMyZsTBGW2f/exec";

// ====== PINOS ======
const int ledVerde = 2;
const int ledVermelho = 4;
const int ledAmarelo = 13; // LED indicando teste em andamento
const int buzzer = 15;
const int sensorVibracao = 34; // SW-420

// ====== VARIÁVEIS ======
bool motorAtivo = false;
bool botaoANDAtivo = false;
bool testeEmAndamento = false;
bool sucesso20s = false;

unsigned long tempoInicio = 0;
const unsigned long tempoMinimo = 20000; // 20s

// ====== HOLD SENSOR ======
unsigned long ultimaVibracao = 0;
const unsigned long holdTime = 500; // ms

// ====== WEB SERVER ======
WebServer server(80);

// ====== NTP ======
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // GMT-3
const int daylightOffset_sec = 0;

// ====== FUNÇÃO HTML ======
String paginaHTML() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt-br">
  <head>
    <meta charset="UTF-8">
    <title>Teste Climatizador</title>
    <style>
      body { font-family: Arial, sans-serif; text-align:center; background:#f4f4f9; color:#333; }
      h2 { color:#2c3e50; }
      .box { display:inline-block; background:white; padding:20px; margin:20px; border-radius:12px; box-shadow:0 4px 8px rgba(0,0,0,0.1);}
      .botao { font-size:16px; padding:12px 24px; border:none; border-radius:8px; cursor:pointer; margin:10px; color:white;}
      .motorON { background:#27ae60; } .motorOFF { background:#c0392b; }
      .andON { background:#27ae60; } .andOFF { background:#c0392b; }
      .led { width:20px; height:20px; border-radius:50%; display:inline-block; margin-bottom:5px;}
      .ledOn { background:green; } .ledOff { background:red; }
      #cronometro { font-size:24px; color:#e67e22; margin:20px;}
    </style>
    <script>
      let tempoInicioJS = 0;
      let testeAtivo = false;

      function enviarComando(c) {
        fetch('/' + c, { method:'POST' }).then(()=> location.reload());
      }

      function atualizarCronometro() {
        let ativo = document.getElementById('ativo').value=='1';
        if(ativo){
          if(!testeAtivo){
            tempoInicioJS = Date.now();
            testeAtivo = true;
          }
          let segundos = Math.floor((Date.now() - tempoInicioJS)/1000);
          document.getElementById('cronometro').innerText = "⏱ "+segundos+"s";
        } else {
          testeAtivo = false;
          document.getElementById('cronometro').innerText = "⏱ 0s";
        }
      }

      setInterval(atualizarCronometro, 1000);
    </script>
  </head>
  <body>
    <h2>📊 Teste Climatizador</h2>
    <div class="box">
      <div>Motor Virtual<br>
        <div class="led )rawliteral" + String(motorAtivo?"ledOn":"ledOff") + R"rawliteral("></div>
        <button class="botao )rawliteral" + String(motorAtivo?"motorON":"motorOFF") + R"rawliteral(" onclick="enviarComando('toggleMotor')">)rawliteral" + String(motorAtivo?"Desligar Motor":"Ligar Motor") + R"rawliteral(</button>
      </div>
      <div>Botão Porta AND<br>
        <div class="led )rawliteral" + String(botaoANDAtivo?"ledOn":"ledOff") + R"rawliteral("></div>
        <button class="botao )rawliteral" + String(botaoANDAtivo?"andON":"andOFF") + R"rawliteral(" onclick="enviarComando('toggleAND')">)rawliteral" + String(botaoANDAtivo?"Desligar":"Ligar") + R"rawliteral(</button>
      </div>
      <div id="cronometro">⏱ 0s</div>
      <input type="hidden" id="ativo" value=")rawliteral" + String(testeEmAndamento?"1":"0") + R"rawliteral(">
    </div>
  </body>
  </html>
  )rawliteral";
  return html;
}

// ====== HANDLERS ======
void handleRoot() { server.send(200,"text/html",paginaHTML()); }
void handleToggleMotor() { motorAtivo = !motorAtivo; server.sendHeader("Location","/"); server.send(303); }
void handleToggleAND() { botaoANDAtivo = !botaoANDAtivo; server.sendHeader("Location","/"); server.send(303); }

// ====== FUNÇÃO HORA ATUAL ======
String horaAtual() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Hora indisponível";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}
// ====== FUNÇÃO ENVIAR GOOGLE SHEETS ======
void enviarGoogleSheets(String resultado, unsigned long tempoTotal){
  if(WiFi.status()==WL_CONNECTED){
    HTTPClient http;
    http.begin(urlGoogleSheets);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"dataHora\":\""+horaAtual()+"\",";
    json += "\"tempo_maquina\":\""+String(tempoTotal/1000)+"\",";
    json += "\"resultado\":\""+resultado+"\",";
    json += "\"setor\":\"teste funcional\"";
    json += "}";

    int httpCode = http.POST(json);

    if(httpCode == HTTP_CODE_OK){ // resposta 200 do servidor
      Serial.println("📤 Dados enviados com sucesso: " + resultado);
      String payload = http.getString();
      Serial.println("Resposta do servidor: " + payload);
    } else {
      Serial.printf("⚠ Falha ao enviar dados. HTTP code: %d\n", httpCode);
      String payload = http.getString();
      Serial.println("Resposta do servidor: " + payload);
    }

    http.end();
  } else {
    Serial.println("❌ WiFi desconectado, não foi possível enviar os dados");
  }
}


// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  pinMode(ledVerde, OUTPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(sensorVibracao, INPUT);

  digitalWrite(ledVerde,LOW);
  digitalWrite(ledVermelho,LOW);
  digitalWrite(ledAmarelo,LOW);
  digitalWrite(buzzer,LOW);

  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){delay(500); Serial.print(".");}
  Serial.println("\n✅ Conectado ao WiFi!");
  Serial.print("🌐 Acesse pelo navegador: http://");
  Serial.println(WiFi.localIP());

  // Configura NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Rotas do servidor web
  server.on("/",handleRoot);
  server.on("/toggleMotor", HTTP_POST, handleToggleMotor);
  server.on("/toggleAND", HTTP_POST, handleToggleAND);
  server.begin();
}

// ====== LOOP ======
void loop() {
  server.handleClient();

  // Atualiza sensor com hold
  if(digitalRead(sensorVibracao) == HIGH) ultimaVibracao = millis();
  bool sensorAtivo = motorAtivo || (millis() - ultimaVibracao < holdTime);
  bool condicaoTeste = sensorAtivo && botaoANDAtivo;

  if(!testeEmAndamento && condicaoTeste){
    testeEmAndamento = true;
    sucesso20s = false;
    tempoInicio = millis();
    Serial.println("▶ Teste iniciado");
    digitalWrite(ledAmarelo, HIGH);
  }

  if(testeEmAndamento){
    unsigned long tempoDecorrido = millis() - tempoInicio;

    // Sucesso 20s
    if(!sucesso20s && tempoDecorrido >= tempoMinimo){
      sucesso20s = true;
      Serial.println("✅ Sucesso - 20s atingidos");
      digitalWrite(ledVerde,HIGH);
      digitalWrite(buzzer,HIGH);
      delay(4000);
      digitalWrite(buzzer,LOW);
    }

    // Quando desligar AND ou sensor/motor
    if(!condicaoTeste){
      digitalWrite(ledAmarelo, LOW);
      if(sucesso20s){
        enviarGoogleSheets("APROVADO", tempoDecorrido);
        digitalWrite(ledVerde, LOW);
      } else {
        Serial.println("⚠ Falha - tempo insuficiente");
        digitalWrite(ledVermelho,HIGH);
        for(int i=0;i<3;i++){ digitalWrite(buzzer,HIGH); delay(300); digitalWrite(buzzer,LOW); delay(300);}
        delay(6000);
        digitalWrite(ledVermelho,LOW);
        enviarGoogleSheets("FALHA", tempoDecorrido);
      }
      testeEmAndamento=false;
    }
  }
}
