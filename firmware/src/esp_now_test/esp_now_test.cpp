/*
 * ESPets: Minimal ESP-NOW Broadcast Test (PlatformIO)
 * This sketch demonstrates peer-to-peer communication between two ESP32-S3 boards.
 * It uses Serial logs ONLY to verify communication.
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Structure to send data
typedef struct struct_message {
    char msg[64];
    int count;
} struct_message;

struct_message myData;
struct_message incomingReadings;

esp_now_peer_info_t peerInfo;
int packetCount = 0;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  Serial.println("\n-------------------------");
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("From (MAC): ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X%s", mac[i], (i < 5) ? ":" : "");
  }
  Serial.println();
  Serial.print("Message: ");
  Serial.println(incomingReadings.msg);
  Serial.print("Count: ");
  Serial.println(incomingReadings.count);
  Serial.println("-------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for Serial to settle

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  Serial.println("\n--- ESP-NOW PlatformIO Test ---");
  Serial.print("Local MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Register broadcast peer (FF:FF:FF:FF:FF:FF)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // Use current channel
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  Serial.println("ESP-NOW Initialized. Sending broadcast pings every 2s...");
}

void loop() {
  // Update data packet
  packetCount++;
  snprintf(myData.msg, sizeof(myData.msg), "Pet-Ping from %s", WiFi.macAddress().c_str());
  myData.count = packetCount;

  // Send message via ESP-NOW to broadcast address
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.print("Sent Packet #");
    Serial.println(packetCount);
  } else {
    Serial.println("Error sending the data");
  }
  
  delay(2000);
}
