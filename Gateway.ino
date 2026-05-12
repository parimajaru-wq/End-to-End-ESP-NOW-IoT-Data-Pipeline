#include <WiFi.h>
#include <esp_now.h> 
#include "esp_wifi.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "credentials.h"

const char* mqtt_server   = "iotfun.iotcloudserve.net";
const int   mqtt_port     = 18106;
const char* topic_publish = "IoTFun/Group106";

WiFiClient espClient;
PubSubClient client(espClient);

// ---- Struct ----
typedef struct SensorData {
  int node_id; // Identifies which parcel (1, 2, or 3) the data is from
  float temp, humi, pres, alti; // Environmental data
  float acx, acy, acz; // Motion & Impact data (The core features for AI training)
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

  // Sort data into specific containers based on Node ID
  if (receivedData.node_id < 1 || receivedData.node_id > 3) {
    Serial.println("Invalid Node ID, ignored.");
    return;
  }
  // 3. Multi-Node Identification & Sorting
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

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
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
