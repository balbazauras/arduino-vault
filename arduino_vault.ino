#include <WiFi.h>                                     // needed to connect to WiFi
#include <WebServer.h>                                // needed to create a simple webserver (make sure tools -> board is set to ESP32, otherwise you will get a "WebServer.h: No such file or directory" error)
#include <WebSocketsServer.h>                         // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)
#include <HTTPClient.h>
const char* ssid = "ASUS";
const char* password = "apsikase";
const String sensorId = "1";


const char* ntpServer = "pool.ntp.org";               //Date time server
const long  gmtOffset_sec = 3600;                     //Setting time zone
const int   daylightOffset_sec = 3600;                //Set up  summer time
char buffer[sizeof "2011-10-08T07:07:09Z"];           //exmaple of what date should look lik

const int value_lower = 0;
const int value_upper = 100;

int intervalToCheck = 100;                            // how often should the value be checked
int intervalToSend = 10000;                           // how often should the value be sent by default
unsigned long previousMillis = 0;                     //
int randomValue=0;

String setupArduinoDate()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return ("Failed to obtain time");
  }
  strftime(buffer, sizeof buffer, "%FT%TZ", &timeinfo);
  return buffer;
}
// Initialization of webserver and websocket
WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets

void setup() {
  Serial.begin(115200);                               // init serial port for debugging
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging

  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());                     // show IP address that the ESP32 has received from router

  server.on("/", []() {                               // define here wat the webserver needs to do
    //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server

  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"
}

void loop() {
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();                                   // Update function for the webSockets
  randomValue = random(100);
 
  String arduinoDate = setupArduinoDate();
  unsigned long now = millis();                       // read out the current "time" ("millis()" gives the time in ms since the Arduino started)

  if ((unsigned long)(now - previousMillis) > intervalToCheck) {
    if (!randomValue > value_lower && randomValue < value_upper) {
      sendHttpJSON(randomValue, arduinoDate, sensorId);
    }
    previousMillis = now;
  }
  if ((unsigned long)(now - previousMillis) > intervalToSend) { // check if "interval" ms has passed since last time the clients were updated

    
    sendHttpJSON(randomValue, arduinoDate, sensorId);


    String jsonString = "";                           // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                      // create a JSON container
    JsonObject object = doc.to<JsonObject>();         // create a JSON Object
    object["value"] = randomValue;                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    serializeJson(doc, jsonString);                   // convert JSON object to string
    webSocket.broadcastTXT(jsonString);

    previousMillis = now;                             // reset previousMillis
  }
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      break;
    case WStype_TEXT:
      // try to decipher the JSON string received
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        const int value_lower = doc["value_lower"];
        const int value_upper = doc["value_upper"];
        Serial.println("lower: " + String(value_upper));
        Serial.println("upper: " + String(value_lower));
      }
      break;
  }
}
void sendHttpJSON(int value, String date, String sensorId) {
  Serial.println(value);
  WiFiClient client;
  HTTPClient http;
  http.begin("https://messuring-vault.herokuapp.com/api/");
  http.addHeader("Content-Type", "application/json");
  String json = "{\"value\":\"" +
                String(value)  + "\",\"sensor\":\"1\",\"arduino_date\":\"" +
                date + "\"}";
  int httpResponseCode = http.POST(json);
  Serial.println(json);
  Serial.println(httpResponseCode);
  http.end();

}
