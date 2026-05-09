#include <WiFi.h>
#include <esp_now.h> 
#include "esp_wifi.h"
#include <Adafruit_BMP280.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "credentials.h"

// I2C Pinout configuration for ESP32 Cucumber RS/MIS
#define SDA_PIN 41
#define SCL_PIN 40
#define BMP_ADDRESS 0x76

const char* mqtt_server   = "iotfun.iotcloudserve.net";
const int   mqtt_port     = 18106;
const char* topic_publish = "IoTFun/Group106";

WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_BMP280 bmp(&Wire);

// ---- Struct ----
typedef struct SensorData {
  int node_id;
  float temp, humi, pres, alti;
  float acx, acy, acz;
  float gyx, gyy, gyz;
} SensorData;

SensorData receivedData;
SensorData node1, node2, node3;
bool gotNode1 = false, gotNode2 = false, gotNode3 = false;

// ---- MQTT Reconnect ----
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    String clientId = "ESP32_GW_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// ---- ESP-NOW Callback ----
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len != sizeof(SensorData)) {
    Serial.println("Invalid packet size, ignored.");
    return;
  }

  memcpy(&receivedData, incomingData, sizeof(receivedData));

  if (receivedData.node_id < 1 || receivedData.node_id > 3) {
    Serial.println("Invalid Node ID, ignored.");
    return;
  }

  if (receivedData.node_id == 1) {
    node1 = receivedData;
    gotNode1 = true;
  } 
  else if (receivedData.node_id == 2) {
    node2 = receivedData;
    gotNode2 = true;
  } 
  else if (receivedData.node_id == 3) {
    node3 = receivedData;
    gotNode3 = true;
  }

  Serial.printf("Received from Node %d\n", receivedData.node_id);
}

// ---- JSON Helper ----
void addNodeToJson(JsonObject node, SensorData data) {
  node["temp"] = data.temp;
  node["humi"] = data.humi;
  node["pres"] = data.pres;
  node["alti"] = data.alti;
  node["acx"]  = data.acx;
  node["acy"]  = data.acy;
  node["acz"]  = data.acz;
  node["gyx"]  = data.gyx;
  node["gyy"]  = data.gyy;
  node["gyz"]  = data.gyz;
}

// ---- WiFi ----
void setup_wifi() {
  Serial.print("Connecting to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());
}

// ---- Setup ----
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bmp.begin(BMP_ADDRESS)) {
    Serial.println("BMP280 not found!");
    while (1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);

  // สำคัญมาก เพราะ JSON ของ 3 Node ยาวเกิน default buffer ของ PubSubClient
  client.setBufferSize(2048);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW Ready");
}

// ---- Loop ----
void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  StaticJsonDocument<1024> doc;

  doc["Gateway"] = "Cucumber_IoT-106";

  JsonObject n1 = doc.createNestedObject("Node_01");
  JsonObject n2 = doc.createNestedObject("Node_02");
  JsonObject n3 = doc.createNestedObject("Node_03");

  if (gotNode1) {
    addNodeToJson(n1, node1);
  }

  if (gotNode2) {
    addNodeToJson(n2, node2);
  }

  if (gotNode3) {
    addNodeToJson(n3, node3);
  }

  char buffer[1024];
  size_t len = serializeJson(doc, buffer);

  Serial.println("========== JSON Payload ==========");
  Serial.println(buffer);
  Serial.print("Payload length: ");
  Serial.println(len);

  bool ok = client.publish(topic_publish, (const uint8_t*)buffer, len);

  if (ok) {
    Serial.println("MQTT publish: SUCCESS");
  } else {
    Serial.print("MQTT publish: FAILED, state=");
    Serial.println(client.state());
  }

  Serial.println("==================================");

  delay(2000);
}