#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HTS221.h>
#include <Adafruit_SHT4x.h>

// ================= ESP-NOW Setting =================
uint8_t gatewayAddress[] = {0x68, 0x67, 0x25, 0x26, 0x90, 0x34};

#define NODE_ID 3
#define GATEWAY_CHANNEL 6

// ================= I2C Pin Setting =================
#define SDA_PIN 41
#define SCL_PIN 40
#define BMP_ADDRESS 0x76

// ================= Sensor Objects =================
Adafruit_BMP280 bmp(&Wire);
Adafruit_MPU6050 mpu;

Adafruit_HTS221 hts;
Adafruit_SHT4x sht4;

enum SensorType { NONE, SENSOR_HTS221, SENSOR_SHT4X };
SensorType activeTempHumidSensor = NONE;

// ================= Data Structure =================
typedef struct SensorData {
  int node_id;
  float temp, humi, pres, alti;
  float acx, acy, acz;
  float gyx, gyy, gyz;
} SensorData;

SensorData dataToSend;

// ================= ESP-NOW Send Callback =================
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Success");
  } else {
    Serial.println("Fail");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting Node with Real Sensors...");

  // ================= Start I2C =================
  Wire.begin(SDA_PIN, SCL_PIN);

  // ================= Start BMP280 =================
  if (!bmp.begin(BMP_ADDRESS)) {
    Serial.println("Could not find BMP280 sensor!");
    while (1) delay(10);
  }

  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );

  Serial.println("BMP280 Found!");

  // ================= Start MPU6050 =================
  if (!mpu.begin()) {
    Serial.println("Could not find MPU6050 sensor!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 Found!");

  // ================= Start Humidity Sensor =================
  if (hts.begin_I2C()) {
    activeTempHumidSensor = SENSOR_HTS221;
    hts.setDataRate(HTS221_RATE_1_HZ);
    Serial.println("HTS221 Found!");
  } 
  else if (sht4.begin()) {
    activeTempHumidSensor = SENSOR_SHT4X;
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
    Serial.println("SHT4x Found!");
  } 
  else {
    Serial.println("No HTS221 or SHT4x humidity sensor found!");
    while (1) delay(10);
  }

  // ================= Start WiFi / ESP-NOW =================
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect();
  delay(100);

  esp_wifi_set_channel(GATEWAY_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t ch;
  wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);

  Serial.print("Node Channel: ");
  Serial.println(ch);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayAddress, 6);
  peerInfo.channel = GATEWAY_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add gateway peer");
    return;
  }

  Serial.println("Node Ready");
}

void loop() {
  // ================= Read Temp & Humidity =================
  sensors_event_t humidity, temp;

  if (activeTempHumidSensor == SENSOR_HTS221) {
    hts.getEvent(&humidity, &temp);
  } 
  else if (activeTempHumidSensor == SENSOR_SHT4X) {
    sht4.getEvent(&humidity, &temp);
  }

  // ================= Read MPU6050 =================
  sensors_event_t a, g, mpuTemp;
  mpu.getEvent(&a, &g, &mpuTemp);

  // ================= Put Real Sensor Data into Structure =================
  dataToSend.node_id = NODE_ID;

  dataToSend.temp = temp.temperature;
  dataToSend.humi = humidity.relative_humidity;

  dataToSend.pres = bmp.readPressure();
  dataToSend.alti = bmp.readAltitude(1013.25);

  dataToSend.acx = a.acceleration.x;
  dataToSend.acy = a.acceleration.y;
  dataToSend.acz = a.acceleration.z;

  dataToSend.gyx = g.gyro.x;
  dataToSend.gyy = g.gyro.y;
  dataToSend.gyz = g.gyro.z;

  // ================= Print Data for Checking =================
  Serial.println("----- Real Sensor Data -----");

  Serial.print("Node ID: ");
  Serial.println(dataToSend.node_id);

  Serial.print("Temperature: ");
  Serial.print(dataToSend.temp);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(dataToSend.humi);
  Serial.println(" %");

  Serial.print("Pressure: ");
  Serial.print(dataToSend.pres);
  Serial.println(" Pa");

  Serial.print("Altitude: ");
  Serial.print(dataToSend.alti);
  Serial.println(" m");

  Serial.print("Acceleration X/Y/Z: ");
  Serial.print(dataToSend.acx);
  Serial.print(", ");
  Serial.print(dataToSend.acy);
  Serial.print(", ");
  Serial.println(dataToSend.acz);

  Serial.print("Gyro X/Y/Z: ");
  Serial.print(dataToSend.gyx);
  Serial.print(", ");
  Serial.print(dataToSend.gyy);
  Serial.print(", ");
  Serial.println(dataToSend.gyz);

  // ================= Send Data via ESP-NOW =================
  esp_err_t result = esp_now_send(
    gatewayAddress,             // Gateway MAC address
    (uint8_t *)&dataToSend,     // Convert the data structure to bytes before sending
    sizeof(dataToSend)          // Size of the data packet
  );

  if (result == ESP_OK) {
    Serial.println("Data sent");
  } else {
    Serial.println("Send error");
  }

  Serial.println("----------------------------");
  delay(2000 + NODE_ID * 300);
}