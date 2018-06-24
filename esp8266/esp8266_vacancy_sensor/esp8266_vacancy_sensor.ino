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

unsigned int id = 0xFF000000;
char mqttID[32];
char mqttTopic[32];

void setup() {
  // Check if EEPROM has been initialized
  Serial.begin(115200);
  unsigned char id_indicator = EEPROM.read(0);
  Serial.println("Booting, reading id");
  if (id_indicator == 0xFF) {
    while (id >= 0xFF000000) {
      randomSeed(ESP.getCycleCount());
      id = random(INT_MAX);
    }
    Serial.printf("Generated id: %08x, saving...\n", id);
    for (int i = 0; i < sizeof(id); ++i) {
      EEPROM.write((unsigned char) (id >> (sizeof(id) - i - 1)) & 0xFF, i);
    }
  } else {
    id = 0;
    for (int i = 0; i < sizeof(id); ++i) {
      id |= EEPROM.read(i) << (sizeof(id) - i - 1);
    }
    Serial.printf("Found id %08x\n", id);
  }
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
  
  while(!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (!client.connect(mqttID, mqttUser, mqttPassword)){
      delay(2000);
      Serial.print(".");
    } else {
      Serial.println("Connected");
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

