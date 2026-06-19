# Smart Irrigation System - IrrigaTech

Welcome to the **Smart Irrigation System (IrrigaTech)**! This project is a comprehensive, IoT-enabled automated irrigation solution built using C++ and the PlatformIO framework. 

## Features

The system is designed with a modular architecture to manage various components of an intelligent irrigation setup:

- **Sensor Management (`sensor_manager`)**: Reads and processes data from soil moisture, temperature, and humidity sensors.
- **Actuator Control (`actuator_manager`)**: Controls water pumps and valves based on real-time sensor feedback.
- **Logic & Control (`logic_controller`, `mist_controller`)**: Evaluates environmental conditions to determine optimal watering and misting schedules.
- **Connectivity & Alerts (`sim800l_manager`, `alert_manager`)**: Integrates SIM800L GSM to send SMS alerts and notifications to the user about system statuses and critical events.
- **Web Interface (`web_server_manager`)**: Hosts a local web server for remote monitoring and manual overrides.
- **Fault Monitoring (`fault_monitor`)**: Constantly checks system health and hardware to prevent damage from dry runs or component failures.
- **Local Display (`display_manager`)**: Provides real-time metrics on a physical display (e.g., OLED/LCD) at the installation site.

## Directory Structure

\`\`\`
├── include/             # Header files (.h) for all system modules
├── src/                 # Source files (.cpp) containing the implementation
├── platformio.ini       # PlatformIO configuration file (boards, framework, libraries)
└── README.md            # Project documentation
\`\`\`

## Getting Started

### Prerequisites

1. Download and install [VS Code](https://code.visualstudio.com/).
2. Install the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension inside VS Code.
3. Obtain the required hardware:
   - ESP32/ESP8266 or similar microcontroller (Check `platformio.ini` for exact board config)
   - SIM800L GSM Module
   - Soil Moisture & DHT sensors
   - Relays/Pumps for water and mist

### Installation & Flashing

1. Clone this repository:
   \`\`\`bash
   git clone https://github.com/Redwan-Ahmed241/Smart-Irrigation-System-IrrigaTech.git
   \`\`\`
2. Open the project folder in VS Code.
3. Allow PlatformIO to initialize and download the necessary dependencies as defined in `platformio.ini`.
4. Connect your microcontroller via USB.
5. Click the **PlatformIO: Upload** button (the right arrow `→` in the bottom status bar) to compile and flash the firmware.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
