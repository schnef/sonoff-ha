#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DebounceEvent.h>
#include "credentials.h"

#ifdef LED_BUILTIN
#define LED_PIN LED_BUILTIN;
#else
#define LED_PIN 13; // manually configure LED pin
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
#define MQTT_PORT   8883
#define INTERVAL    2000
// MQTT Topics
#define MQTT_TOPIC_STATE  "sonoff/relay"
#define MQTT_TOPIC_SET    "sonoff/relay/set"
#define MQTT_PAYLOAD_ON   "on"
#define MQTT_PAYLOAD_OFF  "off"

#define HARD_RESET_THRESHOLD_MS 5000

#define BUFSIZE     128


// ============================ Id and host name ============================

String id = String(ESP.getChipId(), HEX);
String hostName = "sonoff-" + id;

// ============================ Wifi ============================

const char* ssid = STASSID;
const char* password = STAPSK;

// Initialise the WiFi Client object
WiFiClientSecure wifiClient;

// ============================ TLS ============================

const char fingerprint[] PROGMEM = MQTT_FP;

// ============================ MQTT ============================

const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWD;
const char* mqtt_topic_state = MQTT_TOPIC_STATE;
const char* mqtt_topic_set = MQTT_TOPIC_SET;
const char* on = MQTT_PAYLOAD_ON;
const char* off = MQTT_PAYLOAD_OFF;

// Initialise the MQTT Client object
PubSubClient pubsubClient(wifiClient); 

// For (re)connecting, a non-blocking wait is used.
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
    Serial.println("Connection Failed! Rebooting...");
    digitalWrite(led_pin, LED_OFF);
    delay(5000);
    ESP.restart();
  }

  // Setup TLS fingerprint
  wifiClient.setFingerprint(fingerprint);
  // wifiClient.setInsecure();

  // Set MQTT broker, port to use and the internal send/receive buffer size
  pubsubClient.setServer(mqtt_server, mqtt_port);
  pubsubClient.setCallback(callback);
  
  // Set-up OTA
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
  pubsubClient.publish(mqtt_topic_state, onOff ? on : off, true);
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
  Serial.print("Reconnect client ");
  Serial.print(hostName);
  if (pubsubClient.connect(hostName.c_str(), mqtt_user, mqtt_password)) {
    // Once connected, publish an announcement...
    publishState();
    // ... and resubscribe
    pubsubClient.subscribe(mqtt_topic_set);
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
  if(response == "on") {
    onOff = true;
  } else if(response == "off") {
    onOff = false;
  }
  toggleRelay();
}
