# SHOO - Solicitor Harassment Offloading Orchestrator

A sophisticated ESP32-based smart door management system that uses an iPad as its display, controlled via an Arduino board. The system combines physical button inputs (salvaged from an old Razer mechanical keyboard), LED lighting control, Bluetooth Low Energy (BLE) keyboard functionality, and MQTT communication to create an intelligent home automation system. SHOO is designed to help manage unwanted solicitors at your front door through automated responses and smart home integration.

## 🚪 What is SHOO?

**SHOO** (Solicitor Harassment Offloading Orchestrator) is an intelligent door management system that helps you handle unwanted solicitors without having to answer the door. The system uses an iPad mounted at your door as the display, which is automatically controlled by an ESP32 Arduino board. The system provides multiple response options through physical buttons, each triggering different automated actions. Here's an example configuration:

- **Button 1-3**: "No Soliciting" responses with different levels of firmness
- **Button 4**: "Thank you" response for people dropping off packages or polite interactions

Each button press can trigger webhooks to your smart home system, send MQTT messages, and provide visual feedback through LED indicators. The Arduino board acts as a Bluetooth keyboard to control the iPad, automatically waking it up and opening the UniFi Protect app when someone approaches, then locking it when they leave. SHOO integrates seamlessly with Home Assistant, MQTT brokers, and other home automation platforms.

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

SHOO integrates with Home Assistant and UniFi Protect to automatically control your iPad screen based on person detection. This creates a seamless experience where the screen turns on when someone approaches and turns off when they leave.

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

### iPad Setup Requirements

For the screen control workflow to work properly, your iPad needs specific configuration:

#### iOS Shortcut Setup
1. **Create a Simple Shortcut**: The only action should be to open the UniFi Protect app
2. **Naming**: Name it something like "Open Protect App" for easy identification
3. **Purpose**: This shortcut will be triggered by the Arduino's BLE keyboard commands

#### iOS Accessibility Settings
1. **Enable Keyboard Shortcuts**: Go to **Settings > Accessibility > Keyboard > Full Keyboard Access** and turn it ON
2. **Shortcut Assignment**: In **Settings > Accessibility > Keyboard > Commands**, assign a keyboard shortcut to your "Open Protect App" shortcut. In our code, we use `CMD+SHIFT+P` as the shortcut combination.
3. **Keyboard Permissions**: Ensure your BLE keyboard has permission to control the iPad
4. **Testing**: Test the keyboard shortcut manually before relying on the Arduino automation

#### UniFi Protect App Configuration
1. **Last Viewed Camera**: In the UniFi Protect app settings, ensure "Open to last viewed camera" is enabled
2. **Camera Selection**: View the doorbell camera feed before closing the Protect app. This ensures the app opens directly to your doorbell feed when activated

#### Keyboard Shortcuts
The Arduino uses specific keyboard shortcuts to control the iPad:
- **Wake Screen**: `Space` followed by `CMD+SHIFT+P` (wakes iPad and opens Protect app)
- **Lock Screen**: `CMD+W` followed by `CMD+CTRL+Q` (closes app and locks screen)

#### Complete Workflow
1. **Person Detected**: Arduino sends wake command → iPad wakes → Shortcut opens Protect app → Doorbell camera feed displays
2. **Person Not Detected**: Arduino sends lock command → Protect app closes → iPad locks → Screen turns off

## 🛠️ Hardware Requirements

### ESP32 Development Board
- Any ESP32-based board (ESP32 DevKit, ESP32-WROOM, etc.)
- WiFi and Bluetooth capabilities required

### Components
- **WS2812B LED Strip**: 6 LEDs (configurable)
- **4 Push Buttons**: Momentary switches for input
- **Resistors**: 10kΩ pull-up resistors (if not using internal pull-ups)
- **Power Supply**: 5V for LEDs, 3.3V for ESP32

**Important**: Pin selection varies by ESP32 board. Avoid pins with special functions (boot, flash, etc.) and refer to your board's pinout diagram for safe GPIO pins.

### Pin Configuration
```cpp
#define LED_PIN     YOUR_LED_DATA_PIN    // WS2812B data pin (configurable)
#define keyPins[4] = {YOUR_BUTTON_PIN_1, YOUR_BUTTON_PIN_2, 
                      YOUR_BUTTON_PIN_3, YOUR_BUTTON_PIN_4};  // Button input pins (configurable)
```

**Note**: Pin numbers vary by ESP32 board. Choose pins that are safe for your specific board and avoid pins with special functions (like boot, flash, etc.).

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

### Button Functions Example

Here's an example of how you might configure the buttons for different response levels:

1. **Button 1**: "No Soliciting" (Gentle) - Triggers webhook 1, publishes MQTT status
2. **Button 2**: "No Soliciting" (Moderate) - Triggers webhook 2, publishes MQTT status  
3. **Button 3**: "No Soliciting" (Firm) - Triggers webhook 3, publishes MQTT status
4. **Button 4**: "Thank you" - Triggers webhook 4, publishes MQTT status

**Note**: This is just one example configuration. Buttons 1-3 could provide escalating levels of firmness for persistent solicitors, while Button 4 could offer a simple "thank you" response for courteous visitors. You can customize the button functions and responses to match your specific needs and preferences.

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

#### WiFi & MQTT
- **Connection failures**: Check credentials and network connectivity
- **Reconnection loops**: Device auto-reconnects every 30s (WiFi) and 10s (MQTT)
- **Serial output**: Monitor for connection status and error codes

#### BLE Keyboard
- **iPad not responding**: Verify Bluetooth enabled and HID permissions granted
- **Multiple devices**: Check for conflicting BLE device names
- **Connection status**: Monitor Serial output for "iPad connected!" / "iPad disconnected!" messages

#### Screen Control
- **Shortcut not opening**: Verify iOS Accessibility > Keyboard > Full Keyboard Access is ON
- **Wrong camera**: Ensure doorbell camera was last viewed before closing Protect app
- **Keyboard shortcuts**: Test `CMD+SHIFT+P` manually before automation

#### Hardware
- **LEDs not working**: Check 5V power supply and data pin connection
- **Buttons unresponsive**: Verify button pin connections and pull-up resistors
- **Webhook failures**: Check HTTP endpoints and network connectivity

### Debug Commands
Monitor Serial output (115200 baud) for:
- Connection status: WiFi, MQTT, BLE
- Button presses and webhook triggers
- MQTT message processing
- LED control commands
