#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h> 
#include <WiFi.h>   
const char* ssid     = "Mi 11 Lite";                  //Wifi ssid
const char* password = "12345678";                    //Wifi passsowrd
const String sensorId ="1";

const char* ntpServer = "pool.ntp.org";               //Date time server
const long  gmtOffset_sec = 3600;                     //Setting time zone
const int   daylightOffset_sec = 3600;                //Set up  summer time
char buffer[sizeof "2011-10-08T07:07:09Z"];           //exmaple of what date should look like

WebServer server(12345);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(12346);    // the websocket uses port 81 (standard port for websockets
int interval = 1000;                                  // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0;                     // we use the "millis()" command for time reference and this will output an unsigned long


String webpage ="<!DOCTYPE html><html><head><title>Page Title</title></head><body style='background-color: #eeeeee'><span style='color: #003366'><h1>Lets generate a random number</h1><p>The first random number is: <span id='rand1'>-</span></p><p>The second random number is: <span id='rand2'>-</span></p><p><h1>Zdarov gadi</h1><form><input type='text' id='laukas'><br><br></form><button type='button' id='BTN_SEND_BACK'>Send info to ESP32</button></p></span></body><script>;var Socket;document.getElementById('BTN_SEND_BACK').addEventListener('click',button_send_back);function init(){Socket=new WebSocket('ws://'+window.location.hostname+':81/');Socket.onmessage=function(n){processCommand(n)}};function button_send_back(){let laukas=document.getElementById('laukas').value;var n={laukas};Socket.send(JSON.stringify(n))};function processCommand(e){var n=JSON.parse(e.data);document.getElementById('rand1').innerHTML=n.rand1;document.getElementById('rand2').innerHTML=n.rand2;console.log(n.rand1);console.log(n.rand2)};window.onload=function(n){init()};</script></html>";

void printData(){
//    Serial.println("Date time:"+printLocalTime());
  //  Serial.println("Sensor value:"+rvalue);
    Serial.print("HTTP Response code: ");
   // Serial.println(httpResponseCode);
}
void sendHttpJSON(String value, String date,String sensorId){
    WiFiClient client;
    HTTPClient http;
    http.begin("https://messuring-vault.herokuapp.com/api/");
    http.addHeader("Content-Type", "application/json");
    String json = "{\"value\":\""+
        value  +"\",\"sensor\":\"1\",\"date\":\""+
    date+"\"}";
    int httpResponseCode = http.POST(json);
    Serial.println(json);
    Serial.println(http.POST(json));
    Serial.println(httpResponseCode);
    http.end();
  
}

void setUpWifi(){
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void setUpServer(){
  server.on("/", []() {                               // define here wat the webserver needs to do
  server.send(200, "text/html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin(); 
}
void setUpWebSocket(){
  webSocket.begin();                                  
  webSocket.onEvent(webSocketEvent); 
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  switch (type) {                                     // switch on the type of information sent
    case WStype_DISCONNECTED:                         // if a client is disconnected, then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            // if a client is connected, then type == WStype_CONNECTED
      Serial.println("Client " + String(num) + " connected");
      // optionally you can add code here what to do when connected
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT
      // try to decipher the JSON string received
      StaticJsonDocument<200> doc;                    // create a JSON container
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        // JSON string was received correctly, so information can be retrieved:
        const char* laukas = doc["laukas"];
        const char* g_type = doc["type"];
        const int g_year = doc["year"];
        const char* g_color = doc["color"];
        Serial.println("Received guitar info from user: " + String(num));
        Serial.println("Laukas:: " + String(laukas));
        Serial.println("Type: " + String(g_type));
        Serial.println("Year: " + String(g_year));
        Serial.println("Color: " + String(g_color));
      }
      Serial.println("");
      break;
  }
}
String setupArduinoDate()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return("Failed to obtain time");
  }
   strftime(buffer, sizeof buffer, "%FT%TZ", &timeinfo);
  return buffer;
}


void setup() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.begin(9600);
  
  setUpWifi();
  setUpServer();
  setUpWebSocket();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    String randomValue=String(random(100));
    String arduinoDate=setupArduinoDate();
    sendHttpJSON(randomValue,arduinoDate,sensorId);
    
    server.handleClient();                              // Needed for the webserver to handle all clients
    webSocket.loop();  
    unsigned long now = millis();                       // read out the current "time" ("millis()" gives the time in ms since the Arduino started)
      if ((unsigned long)(now - previousMillis) > interval) { // check if "interval" ms has passed since last time the clients were updated
    
          String jsonString = "";                           // create a JSON string for sending data to the client
          StaticJsonDocument<200> doc;                      // create a JSON container
          JsonObject object = doc.to<JsonObject>();         // create a JSON Object
          object["rand1"] = random(100);                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
          object["rand2"] = random(100);
          serializeJson(doc, jsonString);                   // convert JSON object to string
          Serial.println(jsonString);                       // print JSON string to console for debug purposes (you can comment this out)
          webSocket.broadcastTXT(jsonString);               // send JSON string to clients
          previousMillis = now;                             // reset previousMillis
      }
  }
  else {
    Serial.println("WiFi Disconnected");
  }
  delay(10000);
}
