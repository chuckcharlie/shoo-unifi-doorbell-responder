# SHOO - Solicitor Harassment Offloading Orchestrator

A sophisticated ESP32-based smart door controller that combines physical button inputs, LED lighting control, Bluetooth Low Energy (BLE) keyboard functionality, and MQTT communication to create an intelligent home automation system. SHOO is designed to help manage unwanted solicitors at your front door through automated responses and smart home integration.

## 🚪 What is SHOO?

**SHOO** (Solicitor Harassment Offloading Orchestrator) is an intelligent door management system that helps you handle unwanted solicitors without having to answer the door. The system provides multiple response options through physical buttons, each triggering different automated actions:

- **Button 1-3**: "No Soliciting" responses with different levels of firmness
- **Button 4**: "Thank you, but no thank you" response for polite solicitors

Each button press can trigger webhooks to your smart home system, send MQTT messages, and provide visual feedback through LED indicators. SHOO integrates seamlessly with Home Assistant, MQTT brokers, and other home automation platforms.

## 🚀 Features

### Core Functionality
- **4 Physical Button Inputs**: Debounced button detection with configurable GPIO pins
- **WS2812B LED Strip Control**: 6 RGB LEDs with preset colors and custom color cycling
- **BLE HID Keyboard**: Acts as a Bluetooth keyboard to control connected iPads/devices
- **MQTT Integration**: Real-time communication for remote control and status updates
- **Webhook Support**: HTTP webhook triggers for each button press
- **WiFi Management**: Automatic reconnection with configurable retry logic

### LED Features
- **Preset Colors**: Warm white, soft white, cool white, daylight, and standard colors
- **Custom RGB Control**: Set any RGB color via MQTT commands
- **Brightness Control**: Adjustable brightness from 0-255
- **Color Cycling**: Animated color transitions with configurable timing
- **Status Indicators**: Visual feedback for system states

### BLE Keyboard Capabilities
- **Lock Screen**: Sends CMD+W followed by CMD+CTRL+Q to lock iPad
- **Unlock Screen**: Sends spacebar followed by CMD+SHIFT+P to wake iPad
- **HID Compliance**: Full HID keyboard implementation for broad device compatibility
- **Unique Device Naming**: Custom BLE MAC addressing for multiple device support

### MQTT Topics
- `arduino/1/status`: Device status and heartbeat messages
- `arduino/1/ipad`: iPad control commands (lock/unlock)
- `arduino/1/lights`: LED control commands

## 📺 Screen Control Workflow

SHOO integrates with Home Assistant and UniFi Protect to automatically control your door screen based on person detection. This creates a seamless experience where the screen turns on when someone approaches and turns off when they leave.

### System Architecture

```
UniFi Protect Camera → Home Assistant → MQTT → Arduino → Screen Control
```

### Workflow Steps

1. **Person Detection**: UniFi Protect cameras detect when a person is present at your door
2. **Home Assistant Automation**: A Home Assistant automation triggers when person detection changes
3. **MQTT Message**: Home Assistant publishes an MQTT status message indicating person presence
4. **Arduino Response**: The Arduino receives the MQTT message and controls the screen accordingly
5. **Screen Control**: 
   - **Person Detected**: Screen turns ON (iPad wakes up, displays content)
   - **Person Not Detected**: Screen turns OFF (iPad locks, conserves power)

### Home Assistant Configuration

#### UniFi Protect Integration
- Install the [UniFi Protect integration](https://www.home-assistant.io/integrations/unifiprotect/) in Home Assistant
- Configure with your UniFi Protect NVR credentials
- Ensure person detection is enabled on your doorbell camera

#### Real-World Automation Example
Here's an actual Home Assistant automation that wakes up doorbell iPads when motion is detected:

```yaml
alias: "Wake up doorbell ipad"
description: "Wakes up doorbell iPads when motion is detected"
triggers:
  - type: turned_on
    device_id: YOUR_DEVICE_ID_HERE
    entity_id: YOUR_ENTITY_ID_HERE
    domain: binary_sensor
    trigger: device
conditions: []
actions:
  - action: mqtt.publish
    metadata: {}
    data:
      evaluate_payload: false
      qos: 0
      retain: false
      topic: arduino/1/ipad
      payload: "{\"lock\": false}"
mode: single
```

**Note**: Replace `YOUR_DEVICE_ID_HERE` and `YOUR_ENTITY_ID_HERE` with your actual UniFi Protect device and entity IDs. Device ID is your camera device ID, and Entity ID is the `Person detected` sensor/boolean. It is easier to build the automation in the UI if you don't know these.


## 🛠️ Hardware Requirements

### ESP32 Development Board
- Any ESP32-based board (ESP32 DevKit, ESP32-WROOM, etc.)
- WiFi and Bluetooth capabilities required

### Components
- **WS2812B LED Strip**: 6 LEDs (configurable)
- **4 Push Buttons**: Momentary switches for input
- **Resistors**: 10kΩ pull-up resistors (if not using internal pull-ups)
- **Power Supply**: 5V for LEDs, 3.3V for ESP32

### Pin Configuration
```cpp
#define LED_PIN     21  // WS2812B data pin
#define keyPins[4] = {12, 14, 26, 27};  // Button input pins
```

## 📚 Dependencies

### Required Libraries
- **WiFi**: Built-in ESP32 WiFi library
- **PubSubClient**: MQTT client for ESP32
- **Bounce2**: Button debouncing library
- **FastLED**: WS2812B LED control library
- **ArduinoJson**: JSON parsing and generation
- **BLE Libraries**: Built-in ESP32 BLE libraries

### Installation
```bash
# Install via Arduino Library Manager
# Search for and install:
# - PubSubClient
# - Bounce2
# - FastLED
# - ArduinoJson
```

## ⚙️ Configuration

### WiFi Settings
```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### MQTT Configuration
```cpp
const char* mqtt_server = "YOUR_MQTT_SERVER";
const char* mqtt_topic_prefix = "arduino/1/";
```

### Webhook URLs
```cpp
const char* webhookUrls[4] = {
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_1",  // Key 1
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_2",  // Key 2
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_3",  // Key 3
  "http://YOUR_SERVER:PORT/api/webhook/YOUR_WEBHOOK_4"   // Key 4
};
```

## 🔧 Usage

### Typical Scenarios

SHOO is designed for various door interaction scenarios:

- **Aggressive Solicitors**: Use Button 1 for immediate, firm "No Soliciting" response
- **Persistent Salespeople**: Use Button 2 for moderate "No Soliciting" with follow-up
- **Friendly Solicitors**: Use Button 3 for gentle "No Soliciting" response
- **Polite Requests**: Use Button 4 for "Thank you, but no thank you" response

### Integration Examples

- **Home Assistant**: Each button can trigger different automations (doorbell sounds, announcements, camera recordings)
- **Smart Speakers**: Automated voice responses through your smart home system
- **Security Systems**: Log all door interactions and trigger appropriate security measures
- **Lighting**: Use LED indicators to show system status and button press feedback

### Button Functions
1. **Button 1**: "No Soliciting" (Firm) - Triggers webhook 1, publishes MQTT status
2. **Button 2**: "No Soliciting" (Moderate) - Triggers webhook 2, publishes MQTT status
3. **Button 3**: "No Soliciting" (Gentle) - Triggers webhook 3, publishes MQTT status
4. **Button 4**: "Thank you, but no thank you" - Triggers webhook 4, publishes MQTT status

### LED Control via MQTT

#### Set Preset Color
```json
{
  "preset": "warm_white"
}
```

#### Set Custom RGB Color
```json
{
  "color": {
    "r": 255,
    "g": 100,
    "b": 50
  }
}
```

#### Adjust Brightness
```json
{
  "brightness": 128
}
```

#### Enable/Disable Color Cycling
```json
{
  "color_cycle": true
}
```

### iPad Control via MQTT

#### Lock Screen
```json
{
  "lock": true
}
```

#### Unlock Screen
```json
{
  "lock": false
}
```

## 📡 MQTT Message Format

### Status Updates
```json
{
  "status": "key_1_pressed",
  "message": "Button 1 was pressed",
  "timestamp": 1640995200
}
```

### Heartbeat Messages
```json
{
  "status": "alive",
  "ble_connected": true,
  "wifi_connected": true,
  "timestamp": 1640995200
}
```

## 🔒 Security Features

- **Unique Device IDs**: MAC address-based client identification
- **Custom BLE MAC**: Separate BLE addressing to avoid conflicts
- **Configurable Topics**: MQTT topic structure can be customized
- **Webhook Authentication**: Supports authenticated webhook endpoints

## 🚨 Troubleshooting

### Common Issues

#### WiFi Connection Problems
- Verify SSID and password are correct
- Check WiFi signal strength
- Ensure ESP32 is within range of router

#### MQTT Connection Issues
- Verify MQTT broker address and port
- Check network connectivity
- Review MQTT broker logs for connection attempts

#### BLE Connection Problems
- Ensure iPad Bluetooth is enabled
- Check for multiple devices with similar names
- Verify HID keyboard permissions on iPad

#### LED Issues
- Check power supply (5V for LEDs)
- Verify data pin connection
- Ensure FastLED library is properly installed

### Debug Information
The device provides extensive Serial output for debugging:
- WiFi connection status
- MQTT connection attempts
- BLE device initialization
- Button press detection
- LED control commands

## 🔄 Updates and Maintenance

### Firmware Updates
- Upload new code via Arduino IDE
- Monitor Serial output for initialization status
- Verify all connections after updates

### Configuration Changes
- Modify constants in the code
- Restart device to apply changes
- Test functionality after modifications

## 📋 License

This project is open source. Please ensure you comply with the licenses of all included libraries.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## 📞 Support

For questions or support:
1. Check the troubleshooting section
2. Review Serial output for error messages
3. Verify all hardware connections
4. Test individual components separately

---

**Note**: This is a sanitized version of the original code. Replace all placeholder values (YOUR_WIFI_SSID, YOUR_MQTT_SERVER, etc.) with your actual configuration before uploading to your ESP32.
