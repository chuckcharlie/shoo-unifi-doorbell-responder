#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <HTTPClient.h>
#include <Bounce2.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLE2902.h>
#include <FastLED.h>
#include <ArduinoJson.h>

// LED Configuration
#define LED_PIN     21  // GPIO pin for LED data
#define NUM_LEDS    6   // Number of LEDs in the strip
#define BRIGHTNESS  128 // 50% of 255
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Preset Colors
const CRGB warmWhite = CRGB(255, 147, 41);    // Warm white (2700K)
const CRGB softWhite = CRGB(255, 197, 143);   // Soft white (3000K)
const CRGB coolWhite = CRGB(255, 244, 229);   // Cool white (4000K)
const CRGB daylight = CRGB(255, 255, 255);    // Daylight (5000K)
const CRGB blue = CRGB(0, 0, 255);            // Blue
const CRGB red = CRGB(255, 0, 0);             // Red
const CRGB green = CRGB(0, 255, 0);           // Green
const CRGB purple = CRGB(128, 0, 128);        // Purple
const CRGB orange = CRGB(255, 165, 0);        // Orange
const CRGB off = CRGB(0, 0, 0);               // Off

// Wi-Fi credentials - REPLACE WITH YOUR OWN
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT broker info - REPLACE WITH YOUR OWN
const char* mqtt_server = "YOUR_MQTT_SERVER";
const char* mqtt_topic_prefix = "arduino/1/";
const char* topic = mqtt_topic_prefix "status";
const char* ipad_topic = mqtt_topic_prefix "ipad";
const char* lights_topic = mqtt_topic_prefix "lights";
bool ipad_topic_subscribed = false;  // Track subscription status
bool lights_topic_subscribed = false;  // Track lights subscription status

// MQTT client ID - will be set to unique value in setup()
char mqtt_client_id[32];

// BLE Device name
const char* deviceName = "ipad_controller_frontdoor";

// Webhook URLs for each key - REPLACE WITH YOUR OWN
const char* webhookUrls[4] = {
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_1",  // Key 1
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_2",  // Key 2
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_3",  // Key 3
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_4"   // Key 4
};

// Webhook retry configuration
const int MAX_WEBHOOK_RETRIES = 3;
const int INITIAL_RETRY_DELAY = 1000;  // 1 second
const int MAX_RETRY_DELAY = 10000;     // 10 seconds

WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;

// BLE objects
BLEServer* pServer = NULL;
BLEHIDDevice* hid = NULL;
BLECharacteristic* input = NULL;
bool deviceConnected = false;

// NTP config
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

// Heartbeat settings
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 10000;

// Key switch config
const int keyPins[4] = {12, 14, 26, 27};  // Safe input pins on most ESP32 boards
Bounce2::Button buttons[4];  // Array of Bounce objects for each button
bool lastButtonStates[4] = {false, false, false, false};  // Track last button states

// WiFi reconnection settings
unsigned long lastWifiCheck = 0;
const unsigned long wifiCheckInterval = 30000;  // Check WiFi every 30 seconds

// MQTT reconnection settings
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 10000;  // Try to reconnect every 10 seconds instead of 5
bool mqttConnected = false;  // Track MQTT connection state

// Color cycling variables
bool colorCycleEnabled = false;
unsigned long lastColorChange = 0;
const unsigned long colorChangeInterval = 1000; // Change color every second
int currentColorIndex = 0;

// Array of main colors to cycle through
const CRGB mainColors[] = {
  CRGB(255, 0, 0),    // Red
  CRGB(0, 255, 0),    // Green
  CRGB(0, 0, 255),    // Blue
  CRGB(255, 255, 0),  // Yellow
  CRGB(255, 0, 255),  // Magenta
  CRGB(0, 255, 255)   // Cyan
};
const int numMainColors = sizeof(mainColors) / sizeof(mainColors[0]);

// BLE connection callback
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("iPad connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("iPad disconnected!");
      // Restart advertising
      pServer->getAdvertising()->start();
    }
};

void updateColorCycle() {
  if (!colorCycleEnabled) return;
  
  unsigned long now = millis();
  if (now - lastColorChange >= colorChangeInterval) {
    lastColorChange = now;
    
    // Fade to the next color
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = mainColors[currentColorIndex];
    }
    FastLED.show();
    
    // Move to next color
    currentColorIndex = (currentColorIndex + 1) % numMainColors;
  }
}

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, ipad_topic) == 0) {
    // Convert payload to string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.printf("Received iPad command: %s\n", message);
    
    // Check for lock commands
    if (strstr(message, "\"lock\": true") != NULL) {
      Serial.println("Lock command detected");
      
      if (!deviceConnected) {
        Serial.println("Error: iPad not connected via BLE");
        sendStatusUpdate("ipad_command_failed", "BLE not connected");
        return;
      }

      // Safety check for BLE input
      if (input == nullptr) {
        Serial.println("Error: BLE input not initialized");
        sendStatusUpdate("ipad_command_failed", "BLE input not initialized");
        return;
      }

      // Send CMD+W keyboard shortcut first
      uint8_t report[8] = {0};
      report[0] = 0x08;  // CMD (0x08)
      report[2] = 0x1A;  // 'w' key code
      
      Serial.println("Sending keyboard command: CMD+W");
      input->setValue(report, sizeof(report));
      input->notify();
      
      // Release keys
      report[0] = 0;
      report[2] = 0;
      input->setValue(report, sizeof(report));
      input->notify();

      // Then send CMD+CTRL+Q keyboard shortcut
      report[0] = 0x08 | 0x01;  // CMD (0x08) + CTRL (0x01)
      report[2] = 0x14;  // 'q' key code
      
      Serial.println("Sending keyboard command: CMD+CTRL+Q");
      input->setValue(report, sizeof(report));
      input->notify();
      
      // Release all keys
      report[0] = 0;
      report[2] = 0;
      input->setValue(report, sizeof(report));
      input->notify();
      
      Serial.println("Keyboard command sent successfully");
      sendStatusUpdate("ipad_command_success", "Lock screen command sent");
    }
    else if (strstr(message, "\"lock\": false") != NULL) {
      Serial.println("Unlock command detected");
      
      if (!deviceConnected) {
        Serial.println("Error: iPad not connected via BLE");
        sendStatusUpdate("ipad_command_failed", "BLE not connected");
        return;
      }

      // Safety check for BLE input
      if (input == nullptr) {
        Serial.println("Error: BLE input not initialized");
        sendStatusUpdate("ipad_command_failed", "BLE input not initialized");
        return;
      }

      // Send spacebar press
      uint8_t report[8] = {0};
      report[2] = 0x2C;  // Spacebar key code
      
      Serial.println("Sending keyboard command: SPACE");
      input->setValue(report, sizeof(report));
      input->notify();
      
      // Release key
      report[2] = 0;
      input->setValue(report, sizeof(report));
      input->notify();

      // Send CMD+SHIFT+P
      report[0] = 0x08 | 0x20;  // CMD (0x08) + SHIFT (0x20)
      report[2] = 0x13;  // 'p' key code
      
      Serial.println("Sending keyboard command: CMD+SHIFT+P");
      input->setValue(report, sizeof(report));
      input->notify();
      
      // Release keys
      report[0] = 0;
      report[2] = 0;
      input->setValue(report, sizeof(report));
      input->notify();
      
      Serial.println("Keyboard command sent successfully");
      sendStatusUpdate("ipad_command_success", "Unlock command sent");
    }
  }
  else if (strcmp(topic, lights_topic) == 0) {
    // Convert payload to string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.printf("Received lights command: %s\n", message);
    
    // Parse JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
      Serial.printf("JSON parsing failed: %s\n", error.c_str());
      return;
    }
    
    // Handle color cycle command
    if (doc.containsKey("color_cycle")) {
      colorCycleEnabled = doc["color_cycle"];
      if (!colorCycleEnabled) {
        // If turning off color cycle, set to warm white
        fill_solid(leds, NUM_LEDS, warmWhite);
        FastLED.show();
      }
      sendStatusUpdate("lights_updated", colorCycleEnabled ? "Color cycle started" : "Color cycle stopped");
      return;
    }
    
    // Handle brightness command
    if (doc.containsKey("brightness")) {
      int brightness = doc["brightness"];
      brightness = constrain(brightness, 0, 255);  // Ensure value is between 0-255
      FastLED.setBrightness(brightness);
      Serial.printf("Set brightness to: %d\n", brightness);
    }
    
    // Handle color command
    if (doc.containsKey("color")) {
      JsonObject color = doc["color"];
      if (color.containsKey("r") && color.containsKey("g") && color.containsKey("b")) {
        int r = color["r"];
        int g = color["g"];
        int b = color["b"];
        
        // Ensure values are between 0-255
        r = constrain(r, 0, 255);
        g = constrain(g, 0, 255);
        b = constrain(b, 0, 255);
        
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        Serial.printf("Set color to: R:%d G:%d B:%d\n", r, g, b);
      }
    }
    
    // Handle preset command
    if (doc.containsKey("preset")) {
      const char* preset = doc["preset"];
      if (strcmp(preset, "warm_white") == 0) {
        fill_solid(leds, NUM_LEDS, warmWhite);
        Serial.println("Set preset: warm_white");
      }
      else if (strcmp(preset, "soft_white") == 0) {
        fill_solid(leds, NUM_LEDS, softWhite);
        Serial.println("Set preset: soft_white");
      }
      else if (strcmp(preset, "cool_white") == 0) {
        fill_solid(leds, NUM_LEDS, coolWhite);
        Serial.println("Set preset: cool_white");
      }
      else if (strcmp(preset, "daylight") == 0) {
        fill_solid(leds, NUM_LEDS, daylight);
        Serial.println("Set preset: daylight");
      }
      else if (strcmp(preset, "blue") == 0) {
        fill_solid(leds, NUM_LEDS, blue);
        Serial.println("Set preset: blue");
      }
      else if (strcmp(preset, "red") == 0) {
        fill_solid(leds, NUM_LEDS, red);
        Serial.println("Set preset: red");
      }
      else if (strcmp(preset, "green") == 0) {
        fill_solid(leds, NUM_LEDS, green);
        Serial.println("Set preset: green");
      }
      else if (strcmp(preset, "purple") == 0) {
        fill_solid(leds, NUM_LEDS, purple);
        Serial.println("Set preset: purple");
      }
      else if (strcmp(preset, "orange") == 0) {
        fill_solid(leds, NUM_LEDS, orange);
        Serial.println("Set preset: orange");
      }
      else if (strcmp(preset, "off") == 0) {
        fill_solid(leds, NUM_LEDS, off);
        Serial.println("Set preset: off");
      }
    }
    
    FastLED.show();
    sendStatusUpdate("lights_updated", "LED settings updated");
  }
}

void sendStatusUpdate(const char* status, const char* message) {
  if (client.connected()) {
    time_t epoch;
    time(&epoch);
    String payload = "{\"status\":\"" + String(status) + "\",\"message\":\"" + String(message) + "\",\"timestamp\":" + String(epoch) + "}";
    Serial.println("Sending status update: " + payload);
    client.publish(topic, payload.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32 STARTING UP ===");
  Serial.printf("BLE Device name will be: %s\n", deviceName);
  Serial.flush();
  
  // Create unique device identifier without changing MAC addresses
  uint8_t original_mac[6];
  WiFi.macAddress(original_mac);
  
  // Create custom BLE MAC (same as original but with different last bytes)
  uint8_t custom_ble_mac[6];
  memcpy(custom_ble_mac, original_mac, 6);
  custom_ble_mac[4] = 0xCD;  // Custom last two bytes for BLE
  custom_ble_mac[5] = 0xEF;
  
  Serial.printf("Original MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                original_mac[0], original_mac[1], original_mac[2], 
                original_mac[3], original_mac[4], original_mac[5]);
  Serial.printf("Custom BLE MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                custom_ble_mac[0], custom_ble_mac[1], custom_ble_mac[2], 
                custom_ble_mac[3], custom_ble_mac[4], custom_ble_mac[5]);
  Serial.flush();

  // Initialize LEDs
  Serial.println("Initializing LEDs...");
  Serial.printf("LED_PIN: %d, NUM_LEDS: %d, BRIGHTNESS: %d\n", LED_PIN, NUM_LEDS, BRIGHTNESS);
  
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Set all LEDs to blue
  fill_solid(leds, NUM_LEDS, blue);
  FastLED.show();
  Serial.println("LEDs initialized and set to blue");

  // Generate unique client ID and device name
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  sprintf(mqtt_client_id, "ESP32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.printf("ESP32 MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("MQTT Client ID: %s\n", mqtt_client_id);
  
  // Create unique BLE device name with custom BLE MAC address suffix
  char unique_device_name[64];
  sprintf(unique_device_name, "ipad_controller_frontdoor_%02X%02X", custom_ble_mac[4], custom_ble_mac[5]);
  Serial.printf("Unique BLE Device Name: %s\n", unique_device_name);
  Serial.flush();

  // Initialize BLE
  Serial.printf("Initializing BLE with device name: %s\n", unique_device_name);
  BLEDevice::init(unique_device_name);
  
  // Set custom BLE MAC address
  esp_ble_gap_set_device_name(unique_device_name);
  esp_ble_gap_config_local_privacy(true);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Initialize HID device
  hid = new BLEHIDDevice(pServer);
  input = hid->inputReport(1);  // Report ID 1
  
  // Set up HID descriptor
  hid->manufacturer()->setValue("ESP32");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  
  // Set up keyboard report map
  const uint8_t reportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xa1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Keyboard)
    0x19, 0xe0,  // Usage Minimum (224)
    0x29, 0xe7,  // Usage Maximum (231)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable, Absolute)
    0x95, 0x01,  // Report Count (1)
    0x75, 0x08,  // Report Size (8)
    0x81, 0x01,  // Input (Constant)
    0x95, 0x06,  // Report Count (6)
    0x75, 0x08,  // Report Size (8)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x65,  // Logical Maximum (101)
    0x05, 0x07,  // Usage Page (Keyboard)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x65,  // Usage Maximum (101)
    0x81, 0x00,  // Input (Data, Array)
    0xc0         // End Collection
  };
  
  hid->reportMap((uint8_t*)reportMap, sizeof(reportMap));
  hid->startServices();
  
  // Start advertising
  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
  
  Serial.println("BLE Keyboard initialized");
  Serial.printf("Device should now appear as: %s\n", unique_device_name);
  Serial.printf("=== BLE DEVICE NAME SET TO: %s ===\n", unique_device_name);
  Serial.flush();
  delay(1000);  // Give BLE time to start advertising with new name

  // Initial WiFi connection
  connectToWiFi();

  // NTP sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synchronized!");

  // MQTT setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  // Key switch input setup with Bounce2
  for (int i = 0; i < 4; i++) {
    buttons[i].attach(keyPins[i], INPUT_PULLUP);
    buttons[i].interval(25);  // Debounce interval in milliseconds
    buttons[i].setPressedState(LOW);  // Button is pressed when pin is LOW
  }
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts > 30) {
      Serial.println("\nFailed to connect. Will retry in setup loop.");
      return;
    }
  }
  
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

bool triggerWebhook(int keyIndex) {
  if (strlen(webhookUrls[keyIndex]) == 0) {
    return true; // Skip if no webhook URL is configured
  }

  int retryCount = 0;
  int retryDelay = INITIAL_RETRY_DELAY;
  bool success = false;

  while (retryCount < MAX_WEBHOOK_RETRIES && !success) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Reconnecting...");
      connectToWiFi();
      if (WiFi.status() != WL_CONNECTED) {
        return false;
      }
    }

    http.begin(webhookUrls[keyIndex]);
    int httpCode = http.POST(""); // Empty POST body
    
    if (httpCode > 0) {
      Serial.printf("Webhook triggered for key %d. HTTP code: %d\n", keyIndex + 1, httpCode);
      success = true;
    } else {
      Serial.printf("Webhook failed for key %d. Error: %s\n", keyIndex + 1, http.errorToString(httpCode).c_str());
      retryCount++;
      
      if (retryCount < MAX_WEBHOOK_RETRIES) {
        Serial.printf("Retrying in %d ms... (Attempt %d/%d)\n", retryDelay, retryCount + 1, MAX_WEBHOOK_RETRIES);
        delay(retryDelay);
        retryDelay = min(retryDelay * 2, MAX_RETRY_DELAY); // Exponential backoff
      }
    }
    
    http.end();
  }

  return success;
}

void loop() {
  // Check WiFi connection periodically
  unsigned long now = millis();
  if (now - lastWifiCheck > wifiCheckInterval) {
    lastWifiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      connectToWiFi();
    }
  }

  // Non-blocking MQTT reconnection
  if (!client.connected()) {
    if (!mqttConnected) {  // Only attempt reconnection if we're not already connected
      if (now - lastMqttReconnectAttempt > mqttReconnectInterval) {
        lastMqttReconnectAttempt = now;
        if (reconnectMQTT()) {
          lastMqttReconnectAttempt = 0;  // Reset timer on successful connection
          mqttConnected = true;
          ipad_topic_subscribed = false;  // Reset subscription status on reconnect
          lights_topic_subscribed = false;  // Reset lights subscription status on reconnect
        }
      }
    }
  } else {
    mqttConnected = true;  // Mark as connected when client reports connected
  }
  
  // Always call client.loop() to maintain MQTT connection
  client.loop();

  // Subscribe to topics if not already subscribed
  if (client.connected() && mqttConnected) {
    if (!ipad_topic_subscribed) {
      if (client.subscribe(ipad_topic)) {
        ipad_topic_subscribed = true;
        Serial.println("Subscribed to iPad control topic");
      } else {
        Serial.println("Failed to subscribe to iPad control topic");
      }
    }
    if (!lights_topic_subscribed) {
      if (client.subscribe(lights_topic)) {
        lights_topic_subscribed = true;
        Serial.println("Subscribed to lights control topic");
      } else {
        Serial.println("Failed to subscribe to lights control topic");
      }
    }
  }

  // Heartbeat with enhanced status
  if (now - lastHeartbeat > heartbeatInterval) {
    lastHeartbeat = now;
    if (client.connected()) {
      time_t epoch;
      time(&epoch);
      String payload = "{\"status\":\"alive\",\"ble_connected\":" + String(deviceConnected ? "true" : "false") + 
                      ",\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + 
                      ",\"timestamp\":" + String(epoch) + "}";
      Serial.println("Sending heartbeat: " + payload);
      client.publish(topic, payload.c_str());
    }
  }

  // Button press detection with Bounce2
  for (int i = 0; i < 4; i++) {
    buttons[i].update();  // Update the Bounce instance
    
    // Only trigger on initial press, not hold
    if (buttons[i].pressed() && !lastButtonStates[i]) {
      time_t epoch;
      time(&epoch);
      String payload = "{\"status\":\"key_" + String(i + 1) + "_pressed\",\"timestamp\":" + String(epoch) + "}";
      Serial.println("Sending key press: " + payload);
      client.publish(topic, payload.c_str());
      
      // Trigger webhook with retry logic
      triggerWebhook(i);
    }
    
    // Update last button state
    lastButtonStates[i] = buttons[i].pressed();
  }

  // Update color cycle if enabled
  updateColorCycle();
}

bool reconnectMQTT() {
  Serial.print("Connecting to MQTT...");
  if (client.connect(mqtt_client_id)) {
    Serial.println("connected.");
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" retrying later...");
    mqttConnected = false;  // Mark as disconnected on failure
    return false;
  }
}
