//comment this define bellow to disable the debug serial output
#define DEBUG_ESP_PORT Serial

#ifdef DEBUG_ESP_PORT
#define SERIAL_DEBUG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
#define SERIAL_DEBUG(...)
#endif

#include <ESP8266WiFi.h>

#define MQTT_PING_INTERVAL 10000
#define MQTT_CONNECT_INTERVAL 10000
#define WIFI_CHECK_INTERVAL 10000
#define BATTERY_CHECK_INTERVAL 10000

#define MQTT_TOPIC_LENGTH_MAX 64
#define MQTT_PAYLOAD_LENGTH_MAX 256

#define WIFI_FAIL 0
#define WIFI_CONNECTED 1
#define WS_DISCONNECTED 2
#define WS_CONNECTED 3
#define MQTT_CONNECTED 4

byte WS_MQTT_status = WIFI_FAIL;

// change the WiFi name and psw to your own
const static char* wifiSSID = "KAISCLAN";
const static char* wifiPassword = "kaisclan";

#include <Hash.h>
// MQTT communication over websocket layer
#include <WebSocketsClient.h>
WebSocketsClient webSocket;

const static String WS_server = "broker.hivemq.com";
const int WS_port = 8000;
const static char* WS_domain = "/mqtt";
//for broker.hivemq.com, you don't need a mqtt username and password, but you can just leave it
const static String MQTT_username = "yourname";
const static String MQTT_password = "yourpassword";
const static int MQTT_ALIVE_TIME = 30;

// create wifiClient to check WiFi/Internet latency error
WiFiClient wifiClient;
// change the http request url to your own
const static char* API_server = "api.cloudmqtt.com";

long lastTimeMQTTActive = -10000;
long lastTimeMQTTConnect = 0;
int mqtt_feedback_count = 0;
long lastTimeMQTTPublish = 0;

byte MQTT_package_id = 2;
static uint8_t bufferMQTTPing[2] = {192, 0};
static uint8_t MQTTConnectHeader[12] = {16, 78, 0, 4, 77, 81, 84, 84, 4, 194, 0, 15};
static uint8_t pSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 7];
static uint8_t pUnSubscribeRequest[MQTT_TOPIC_LENGTH_MAX + 6];
static uint8_t pPublishRequest[MQTT_TOPIC_LENGTH_MAX + MQTT_PAYLOAD_LENGTH_MAX + 5];
static uint8_t pConnectRequest[128];

#include <ArduinoJson.h>
void MQTT_Payload_Process(char* MQTT_payload, int MQTT_payload_length) {
  for (int i = 0; i < MQTT_payload_length; i++) {
    SERIAL_DEBUG("/%d,", MQTT_payload[i]);
  }
  SERIAL_DEBUG("\n");
  StaticJsonBuffer<400> JSON_MQTT_BUFFER;//512 or 256? need load test
  JsonObject& JSON_MQTT_OBJ = JSON_MQTT_BUFFER.parseObject(MQTT_payload);
  if (JSON_MQTT_OBJ.success()) {
    // payload: {"debug1":1}
    if (JSON_MQTT_OBJ.containsKey("debug1")) {
      // do some thing to handle the message
      int debugValue = JSON_MQTT_OBJ["debug1"];
      SERIAL_DEBUG("JSON PARSE RESULT: debug1 = %d\n", debugValue);
    }
  }
}

#include "MQTT_STACK.h"
void MQTT_Callback(uint8_t* MSG_payload, size_t MSG_length) {
  if (WS_MQTT_status == MQTT_CONNECTED) {
    if (MSG_length == 2) {
      if (MSG_payload[0] == 208 && MSG_payload[1] == 0) {
        mqtt_feedback_count = 0;
        SERIAL_DEBUG("MQTT PING RESPONSE\n");
      }
    }
    if (MSG_length > 4) {
      if (MSG_payload[0] == 48) {
        if (MSG_length > 127 + 2) {
          if ((MSG_payload[2] - 1) * 128 + MSG_payload[1] + 3 == MSG_length) {
            char mqtt_payload[MSG_length - 5 - MSG_payload[4]];
            for (int i = 5 + MSG_payload[4]; i < MSG_length; i++) {
              mqtt_payload[i - 5 - MSG_payload[4]] = (char)MSG_payload[i];
            }
            MQTT_Payload_Process(mqtt_payload, MSG_length - 5 - MSG_payload[4]);
          }
        }
        else if (MSG_payload[1] + 2 == MSG_length) {
          char mqtt_payload[MSG_length - 4 - MSG_payload[3]];
          for (int i = 4 + MSG_payload[3]; i < MSG_length; i++) {
            mqtt_payload[i - 4 - MSG_payload[3]] = (char)MSG_payload[i];
          }
          MQTT_Payload_Process(mqtt_payload, MSG_length - 4 - MSG_payload[3]);
        }
      }
    }
  }
  else if (WS_MQTT_status == WS_CONNECTED ) {
    if (MSG_length == 4) {
      if (MSG_payload[0] == 32 && MSG_payload[3] == 0) {
        WS_MQTT_status = MQTT_CONNECTED;
        Publish_Stach_Push("to/ESP/" + WiFi.macAddress(), "SUBSCRIBE");
        SERIAL_DEBUG("MQTT CONNECTED\n");
      }
    }
  }
}
void MQTT_Loop() {
  webSocket.loop();

  if (WS_MQTT_status == MQTT_CONNECTED) {

    Publish_Stack_Pop();

    if (millis() - lastTimeMQTTActive > MQTT_PING_INTERVAL) {
      if (mqtt_feedback_count > 2) {
        WS_MQTT_status = WS_DISCONNECTED;
        webSocket.disconnect();

        mqtt_feedback_count = 0;

        lastTimeMQTTConnect = millis();
      }
      else {
        webSocket.sendBIN(bufferMQTTPing, 2);
        SERIAL_DEBUG("MQTT PING REQUEST WITH FEEDBACK COUNT %d\n", mqtt_feedback_count);
        mqtt_feedback_count++;

      }
      lastTimeMQTTActive = millis();
    }
  }

  else if (WS_MQTT_status == WS_CONNECTED) {

    if ((millis() - lastTimeMQTTConnect) > MQTT_CONNECT_INTERVAL) {
      //MQTT_CONNECT("ArduinoClient-" + WiFi.macAddress() + "-" + String(millis()), MQTT_ALIVE_TIME, MQTT_username, MQTT_password);
      MQTT_CONNECT("ArduinoClient-" + WiFi.macAddress(), MQTT_ALIVE_TIME, MQTT_username, MQTT_password);
      lastTimeMQTTConnect = millis();
      SERIAL_DEBUG("MQTT CONNECT\n");
    }
  }
}
void checkInternet() {
  bool network_check = false;
  if (!wifiClient.connect(API_server, 80)) {
    return;
  }
  String request_headers = String("GET ") + "/api/user" + " HTTP/1.1\r\n" +
                           "Host: " + API_server + "\r\n" +
                           "Accept: " + "application/json" + "\r\n" +
                           "Connection: close\r\n\r\n";
  wifiClient.print(request_headers);
  unsigned long api_timeout = millis();
  while (!wifiClient.available()) {
    if (millis() - api_timeout > 5000) {
      wifiClient.stop();
      return;
    }
  }
  String respond_line;
  while (wifiClient.connected()) {
    respond_line = wifiClient.readStringUntil('\n');
    if (respond_line == "\r") {
      network_check = true;
      SERIAL_DEBUG("INTERNET IS OK\n");
      break;
    }
  }
  if (!network_check) {
    WiFi.disconnect();
    delay(2500);
    WiFi.begin(wifiSSID, wifiPassword);
    byte wifi_connect_count = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      wifi_connect_count++;
      // 20 second timeout
      if (wifi_connect_count > 20) {
        delay(5000);
        ESP.reset();
      }
    }
  }
}
void wsCallbackEvent(WStype_t MSG_type, uint8_t* MSG_payload, size_t MSG_length) {

  switch (MSG_type) {
    case WStype_DISCONNECTED: {

        WS_MQTT_status = WS_DISCONNECTED;
        SERIAL_DEBUG("WEBSOCKET DISCONNECTED\n");
        checkInternet();
        
        break;
      }
    case WStype_CONNECTED: {
        WS_MQTT_status = WS_CONNECTED;
        SERIAL_DEBUG("WEBSOCKET CONNECTED\n");
        break;
      }
    case WStype_TEXT:
      break;
    case WStype_BIN: {
        MQTT_Callback(MSG_payload, MSG_length);
        break;
      }
  }
}

void MQTT_Setup(String mqtt_server, int mqtt_port, int mqtt_reconnect_interval, char* mqtt_domain) {
  webSocket.setReconnectInterval(mqtt_reconnect_interval);
  webSocket.begin(mqtt_server, mqtt_port, mqtt_domain);
  // wss / SSL is not natively supported !
  // webSocket.beginSSL(mqtt_server, mqtt_port,"/");
  webSocket.onEvent(wsCallbackEvent);
  WS_MQTT_status = WS_DISCONNECTED;
}


void setup() {
  Serial.begin(74800);
  Serial.setTimeout(5);
  SERIAL_DEBUG("SERIAL BEGIN\n");

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  byte wifi_connect_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    SERIAL_DEBUG("*");
    delay(1000);
    if (wifi_connect_count > 20) {
      delay(5000);
      ESP.reset();
    }
  }
  checkInternet();
  WS_MQTT_status = WIFI_CONNECTED;
  SERIAL_DEBUG("WIFI CONNECTED\n");

  MQTT_Setup(WS_server, WS_port, 15000, (char*)WS_domain);
}

void loop() {

  if (WS_MQTT_status >= WS_DISCONNECTED) {
    MQTT_Loop();
  }
  if (WS_MQTT_status == MQTT_CONNECTED) {
    if (millis() - lastTimeMQTTPublish > 5000) {
      Publish_Stach_Push("to/ESP/" + WiFi.macAddress(), "{\"debug1\":" + String(millis()) + "}");
      lastTimeMQTTPublish = millis();
    }
  }
}

