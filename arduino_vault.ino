#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ard.h"
// Wifi vars
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;
// Sensor vars
float value_lower = 0.01;
float value_upper = 0.9;
const String sensorId = "1";
int adcValue=0;
float voltValue=0;
#define ADCPIN 36
// time vars
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
char buffer[sizeof "2011-10-08T07:07:09.123345Z"];
// Server/websocket vars
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
// interval vars
int intervalToCheck = 100;
int intervalToSend = 10000;
unsigned long previousMillisCheck = 0;
unsigned long previousMillisSend = 0;
unsigned long pvs = 0;
String setupArduinoDate()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return ("Failed to obtain time");
  }
  strftime(buffer, sizeof buffer, "%FT%TZ", &timeinfo);
  return buffer;
}
void setUpWifi()
{
  WiFi.begin(ssid, password);
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());
}
void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("Client " + String(num) + " disconnected");
    break;
  case WStype_CONNECTED:
    Serial.println("Client " + String(num) + " connected");
    break;
  case WStype_TEXT:
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    else
    {
      intervalToCheck = doc["check_interval"].as<int>();
      intervalToSend = doc["normal_interval"].as<int>();
      value_lower = doc["value_lower"].as<float>();
      value_upper = doc["value_upper"].as<int>();
      Serial.println("lower: " + String(value_lower));
      Serial.println("upper: " + String(value_upper));
      Serial.println("check_interval: " + String(intervalToCheck));
      Serial.println("normal_interval: " + String(intervalToSend));

    }
    break;
  }
}
void getSensorParams(String sensorId)
{
  WiFiClient client;
  HTTPClient http;
  http.begin("https://messuring-vault.herokuapp.com/api/get-params/?sensor-id=1");
  http.addHeader("Content-Type", "application/json");
  String jsonString = "";
  DynamicJsonDocument doc(1024);
  doc["sensor"] = String(sensorId);
  serializeJson(doc, jsonString);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.println(http.getString());
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
   
    intervalToCheck=doc["check_interval"].as<int>();
    intervalToSend=doc["normal_interval"].as<int>();
    value_lower=doc["value_lower"].as<float>();
    value_upper=doc["value_upper"].as<float>();

  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
}
void sendHttpJSON(int value, String date, String sensorId)
{
  WiFiClient client;
  HTTPClient http;
  http.begin("https://messuring-vault.herokuapp.com/api/");
  http.addHeader("Content-Type", "application/json");
  String jsonString = "";
  DynamicJsonDocument doc(1024);
  doc["sensor"] = String(sensorId);
  doc["value"] = String(readSensorValue());
  doc["date"] = String(date);
  serializeJson(doc, jsonString);
  int httpResponseCode = http.POST(jsonString);
  Serial.println(jsonString);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}
float readSensorValue()
{
  adcValue = analogRead(ADCPIN);
  voltValue = ((adcValue * 3.3) / 4095);
  float bar= (voltValue-0.586183)/0.538834;
  return bar;
}
void sendWithWebscoket()
{
  String jsonString = "";
  StaticJsonDocument<200> doc;
  JsonObject object = doc.to<JsonObject>();
  object["value"] = readSensorValue();
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}
void setup()
{
  Serial.begin(115200);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setUpWifi();
  getSensorParams(sensorId);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
void loop()
{
  server.handleClient();
  webSocket.loop();
  
  String arduinoDate = setupArduinoDate();
  
  unsigned long now = millis();
  if ((unsigned long)(now - pvs) > 100){
  sendWithWebscoket();
  }
  if ((unsigned long)(now - previousMillisCheck) > intervalToCheck)
  
  {
    if (!(value_lower < readSensorValue() && readSensorValue() < value_upper ))
    {
      Serial.println("Value exeecds limits");
      sendHttpJSON(readSensorValue(), arduinoDate, sensorId);
    }
    
    previousMillisCheck = now;
  }
  if ((unsigned long)(now - previousMillisSend) > intervalToSend)
  {
    Serial.println("Normal Post");
    sendHttpJSON(readSensorValue(), arduinoDate, sensorId);
    previousMillisSend = now;
  }
  delay(100);
}
