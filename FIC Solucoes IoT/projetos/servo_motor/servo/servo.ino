#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "ESP32-Servo-9";
const char* password = "12345678";

WebServer server(80);
Servo myServo;
const int servoPin = 14;
int currentAngle = 90;

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Controle de Servo</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background: linear-gradient(to right, #2980b9, #6dd5fa); color: #fff; margin: 0; padding: 0; height: 100vh; display: flex; flex-direction: column; justify-content: center; align-items: center; }";
  html += "h1 { margin-top: 20px; text-shadow: 1px 1px 4px #000; }";
  html += ".container { background-color: rgba(255,255,255,0.15); padding: 30px; border-radius: 15px; box-shadow: 0 8px 16px rgba(0,0,0,0.3); width: 90%; max-width: 400px; }";
  html += "input[type=range], input[type=number] { width: 100%; margin-top: 20px; }";
  html += "button { padding: 10px 20px; font-size: 16px; margin-top: 10px; border: none; border-radius: 5px; background-color: #27ae60; color: white; cursor: pointer; }";
  html += "button:hover { background-color: #2ecc71; }";
  html += "#angleValue { font-weight: bold; font-size: 24px; color: #fff; }";
  html += "</style></head><body>";
  html += "<h1>Controle de Servo Motor SG90</h1>";
  html += "<div class='container'>";
  
  // Slider
  html += "<input type='range' min='0' max='180' value='" + String(currentAngle) + "' id='servoSlider' oninput='updateSlider(this.value)'>";
  html += "<p>Ângulo Atual: <span id='angleValue'>" + String(currentAngle) + "</span>°</p>";

  // Campo de digitação
  html += "<input type='number' id='manualInput' min='0' max='180' placeholder='Digite o ângulo (0-180)'>";
  html += "<button onclick='sendManualAngle()'>Enviar</button>";

  html += "</div>";

  // JavaScript
  html += "<script>";
  html += "function updateSlider(val) {";
  html += "  document.getElementById('angleValue').innerText = val;";
  html += "  fetch('/setServo?angle=' + val);";
  html += "}";

  html += "function sendManualAngle() {";
  html += "  var val = document.getElementById('manualInput').value;";
  html += "  if(val >= 0 && val <= 180) {";
  html += "    document.getElementById('servoSlider').value = val;";
  html += "    document.getElementById('angleValue').innerText = val;";
  html += "    fetch('/setServo?angle=' + val);";
  html += "  } else { alert('Digite um valor entre 0 e 180'); }";
  html += "}";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSetServo() {
  if (server.hasArg("angle")) {
    currentAngle = server.arg("angle").toInt();
    currentAngle = constrain(currentAngle, 0, 180);
    myServo.write(currentAngle);
    Serial.println("Ângulo ajustado para: " + String(currentAngle));
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  myServo.attach(servoPin);
  myServo.write(currentAngle);

  WiFi.softAP(ssid, password);
  Serial.print("Access Point criado. IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/setServo", handleSetServo);

  server.begin();
  Serial.println("Servidor iniciado.");
}

void loop() {
  server.handleClient();
}
