#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <arduino_secrets.h>

// Wifi vars
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;

// Sensor vars
int value_lower = 10;
int value_upper = 90;
const String sensorId = "4";
int randomValue;

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
  randomValue = random(100);
  if ((unsigned long)(now - previousMillisCheck) > intervalToCheck)
  {
    if (randomValue < value_lower || randomValue > value_upper)
    {
      Serial.println("Value exeecds limits");
      sendHttpJSON(randomValue, arduinoDate, sensorId);
    }
    sendWithWebscoket();
    previousMillisCheck = now;
  }
  if ((unsigned long)(now - previousMillisSend) > intervalToSend)
  { 
    Serial.println("Normal Post");
    sendHttpJSON(randomValue, arduinoDate, sensorId);
    previousMillisSend = now;
  }
}

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
      value_lower = doc["value_lower"];
      value_upper = doc["value_upper"];
      Serial.println("lower: " + String(value_upper));
      Serial.println("upper: " + String(value_lower));
    }
    break;
  }
}

void getSensorParams(String sensorId)
{
  WiFiClient client;
  HTTPClient http;
  http.begin("http://127.0.0.1:8000/api/get-params/?sensor-id=5");
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
  doc["value"] = String(value);
  doc["date"] = String(date);
  serializeJson(doc, jsonString);
  int httpResponseCode = http.POST(jsonString);
  Serial.println(jsonString);
  Serial.println(httpResponseCode);
  http.end();
}

void sendWithWebscoket()
{
  String jsonString = "";
  StaticJsonDocument<200> doc;
  JsonObject object = doc.to<JsonObject>();
  object["value"] = randomValue;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}
