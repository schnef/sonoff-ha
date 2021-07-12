#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
//#include <WiFiClientSecure.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DebounceEvent.h>
#include "credentials.h"

#ifdef LED_BUILTIN
#define LED_PIN LED_BUILTIN;
#else
#define LED_PIN 13 // manually configure LED pin
#endif
#define LED_OFF HIGH
#define LED_ON LOW

#define RELAY_PIN 12
#define RELAY_OFF LOW
#define RELAY_ON HIGH

#define BUTTON_PIN 0
#define BUTTON_CONFIG (BUTTON_PUSHBUTTON | BUTTON_SET_PULLUP | BUTTON_DEFAULT_HIGH)

#ifndef STASSID
#define STASSID "XXXX"
#define STAPSK  "XXXX"
#endif

#ifndef MQTT_USER
#define MQTT_USER   "YYYY"
#define MQTT_PASSWD "YYYY"
#define MQTT_SERVER "FQDN or IP address"
#define MQTT_FP     "F7 ... 31"
#endif
//#define MQTT_PORT   8883
#define MQTT_PORT   1883
#define INTERVAL    2000

// MQTT Topics
#define MQTT_TOPIC_BASE   "homeassistant/light/"
#define MQTT_TOPIC_CONFIG "/config"
#define MQTT_TOPIC_STATE  "/state"
#define MQTT_TOPIC_CMD    "/command"
// MQTT payload_on/off values. Use light default values.
#define LIGHT_ON   "ON"
#define LIGHT_OFF  "OFF"

#define HARD_RESET_THRESHOLD_MS 5000

#define BUFSIZE     128


// ============================ Id and host name ============================

String id = String(ESP.getChipId(), HEX);
String hostName = "sonoff-R2-" + id;

// ============================ Wifi ============================

const char* ssid = STASSID;
const char* password = STAPSK;

// Initialise the WiFi Client object
//WiFiClientSecure wifiClient;
WiFiClient wifiClient;

// ============================ TLS ============================

const char fingerprint[] PROGMEM = MQTT_FP;

// ============================ MQTT ============================

const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWD;
const char* mqtt_topic_state = MQTT_TOPIC_STATE;
const char* mqtt_topic_cmd = MQTT_TOPIC_CMD;
const char* on = LIGHT_ON;
const char* off = LIGHT_OFF;

// discovery topic

String baseTopic = String(MQTT_TOPIC_BASE + hostName);
String configPayload = String("{\"~\": \"" + baseTopic +
                              "\", \"name\": \"" + hostName +
                              "\", \"unique_id\": \"" + id +
                              "\", \"cmd_t\": \"~" + MQTT_TOPIC_CMD +
                              "\", \"stat_t\": \"~" + MQTT_TOPIC_STATE +
                              "\"}");
String configTopic = baseTopic + "/config";
String stateTopic = baseTopic + "/state";
String cmdTopic = baseTopic + "/command";

// Initialise the MQTT Client object
PubSubClient pubsubClient(wifiClient); 

// For (re)connecting the MQTT client, a non-blocking wait is used.
unsigned long previousMillis = 0;
unsigned long interval = INTERVAL;


// Input and output pins
const int button_pin = BUTTON_PIN;
const int hard_reset_threshold_ms = HARD_RESET_THRESHOLD_MS;
const int relay_pin = RELAY_PIN;
const int led_pin = LED_PIN;

bool onOff = false;
DebounceEvent *button;


void setup() {
  // inputs
  pinMode(BUTTON_PIN, INPUT);
  button = new DebounceEvent(BUTTON_PIN, BUTTON_CONFIG);

  // outputs
  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, RELAY_OFF);
  pinMode(led_pin, OUTPUT);

  // Boot-up and connect to wifi
  digitalWrite(led_pin, LED_ON);
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi Connection Failed! Rebooting...");
    digitalWrite(led_pin, LED_OFF);
    delay(5000);
    ESP.restart();
  }

  Serial.println("Setup TLS");
  // Setup TLS fingerprint
  //wifiClient.setFingerprint(fingerprint);
  //wifiClient.setInsecure();

  // Set MQTT broker, port to use and the internal send/receive buffer size
  Serial.println("Setup MQTT broker");
  pubsubClient.setServer(mqtt_server, mqtt_port);
  pubsubClient.setCallback(callback);
  
  // Set-up OTA
  Serial.println("Setup OTA");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Host name: ");
  Serial.println(hostName);

  digitalWrite(led_pin, LED_OFF);    
}



void loop() {
  unsigned long currentMillis;

  handleButton();
  ArduinoOTA.handle();
  if (!pubsubClient.connected()) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      reconnect();
      previousMillis = currentMillis;
    }
  } else {
    pubsubClient.loop();
  }
}

void toggleRelay() {
  digitalWrite(relay_pin, onOff ? RELAY_ON : RELAY_OFF);
  digitalWrite(led_pin, onOff ? LED_ON : LED_OFF);
  publishState();
}

void publishState() {
  pubsubClient.publish(stateTopic.c_str(), onOff ? LIGHT_ON : LIGHT_OFF, true);
}

void handleButton() {
  if (unsigned int event = button->loop()) {
    if (event == EVENT_RELEASED) {
      switch (button->getEventCount()) {
        case 1:
          if (button->getEventLength() >= hard_reset_threshold_ms) {
            WiFi.disconnect();
            delay(3000);
            ESP.reset();
          } else {
            onOff = !onOff;
            Serial.print("Button pressed ");
            Serial.println(onOff);
            toggleRelay();
          }
          break;
        default:
          break;
      }
    }
  }
}

void reconnect() {
  // Attempt to connect to the mqtt broker
  Serial.print("(Re)connect MQTT client ");
  Serial.print(hostName);
  if (pubsubClient.connect(hostName.c_str(), mqtt_user, mqtt_password)) {
    // Once connected, publish an announcement...
    pubsubClient.publish(configTopic.c_str(), configPayload.c_str(), true);
    publishState();
    // ... and resubscribe
    pubsubClient.subscribe(cmdTopic.c_str());
    Serial.println(" Ok");
  } else {
    Serial.println(" failed");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String response;

  for (int i = 0; i < length; i++) {
    response += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(response);
  if(response == LIGHT_ON) {
    onOff = true;
  } else if(response == LIGHT_OFF) {
    onOff = false;
  }
  toggleRelay();
}
