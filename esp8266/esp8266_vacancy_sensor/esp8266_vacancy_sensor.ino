#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <limits.h>
#include <PubSubClient.h>
#include <string.h>

extern "C" {
  #include <user_interface.h>
}

bool doorState = false;
bool needsPublish = false;

const char* ssid = "vacancy";
const char* password = "bA24PL0Y";

const char* mqttServer = "192.168.1.1";
const int mqttPort = 1883;
const char* mqttUser = "vacancy";
const char* mqttPassword = "vacancy";

WiFiClient espClient;
PubSubClient client(espClient);

#define SENSOR_PIN 2
// 10 second sleep cycle
#define SLEEP_INTERVAL 10000 

unsigned int id = 0x00000000;
char mqttID[32];
char mqttTopic[32];

void setup() {
  // Check if EEPROM has been initialized
  EEPROM.begin(512);
  Serial.begin(115200);
  EEPROM.get(0, id);
  Serial.printf("\nBooting, read id %08x\n", id);
  if (id == 0xFFFFFFFF || id == 0) {
    randomSeed(ESP.getCycleCount());
    id = random(0xFFFFFFFD)+1;
    Serial.printf("Generated id: %08x, saving...\n", id);
    EEPROM.put(0, id);
  }
  EEPROM.end();
  sprintf(&mqttID[0], "doorsensor%08x", id);
  sprintf(&mqttTopic[0], "door/sensor%08x/status", id);
  client.setServer(mqttServer, mqttPort);
  Serial.print("Starting wifi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("done!");
  Serial.print("Setting up hardware...");
  pinMode(SENSOR_PIN, INPUT);
  attachInterrupt(SENSOR_PIN, doorStateChanged, CHANGE);
  doorStateChanged();
  Serial.println("done!");
}

void doorStateChanged(){
  doorState = (bool)digitalRead(SENSOR_PIN);
  needsPublish = true;
}

void publishState(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println("Connected");
  }
  if(!client.connected()) {
    Serial.print("Connecting MQTT...");
    while(!client.connected()) {
      if (!client.connect(mqttID, mqttUser, mqttPassword)){
        delay(2000);
        Serial.print(".");
      } else {
        Serial.println("Connected");
      }
    }
  }
  client.publish(&mqttTopic[0], (doorState ? "1" : "0"), true);
  Serial.println("Published update!");
  needsPublish = false;
  wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
  Serial.println("Heartbeat");
  if (needsPublish) {
    publishState();
  }
  delay(SLEEP_INTERVAL);
}

