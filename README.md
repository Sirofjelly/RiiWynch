# RiiWynch - Advanced Surfboard Engine Control System

A sophisticated wireless engine management system for motorized surfboards and watercraft, featuring dual ESP32 devices with LoRa communication, multiple riding modes, and comprehensive safety features.

## 🌊 System Overview

RiiWynch consists of two main components that work together to provide safe and intuitive control of your motorized surfboard:

- **Main Display Unit**: Engine-mounted ESP32 controller that manages throttle, starter, safety systems, and provides a web interface
- **Remote Control**: Waterproof handheld ESP32 device for wireless throttle control and system monitoring

## ✨ Key Features

### 🎮 Remote Control

- **Wireless LoRa Communication** (868MHz) with up to several kilometers range
- **Intuitive Button Interface** for throttle adjustment (0-100%)
- **OLED Display** showing battery level, connection status, and throttle percentage
- **Emergency Safety Features** with automatic connection monitoring

### 🏄‍♂️ Multiple Riding Modes

- **SURF Mode**: Optimized for wakeboarding riding with responsive throttle
- **SKIM Mode**: Smooth acceleration for skimboarding
- **SMOOTH Mode**: Gentle throttle response for beginners
- **MANUAL Mode**: Direct throttle control for experienced riders

### 🛡️ Safety Systems

- **Heartbeat Monitoring**: Automatic engine shutdown if remote connection is lost
- **Emergency Stop**: Instant motor cutoff via remote or physical buttons
- **Dead Man's Switch**: Configurable auto-stop when remote buttons aren't pressed
- **Connection Status**: Real-time display of signal strength and connectivity

### 🌐 Web Interface

- **WiFi Access Point** for configuration and monitoring
- **Real-time Statistics** including ride time and performance metrics
- **Settings Management** for LoRa parameters and safety timeouts
- **System Diagnostics** and troubleshooting information

### ⚙️ Engine Control

- **Servo-Controlled Throttle**: Precise gas control (0-100%)
- **Electric Starter**: Automated engine starting sequence
- **Choke Control**: Servo-operated choke for cold starts
- **Brake System**: Optional brake servo integration

## 🔧 Hardware Requirements

### Main Display Unit

- **ESP32**: Heltec WiFi LoRa 32 V3
- **LoRa Module**: SX1262 (868MHz)
- **Display**: OLED display for system status
- **Servos**: Gas throttle, choke, and optional brake servos
- **Relays**: Starter and safety relay systems
- **Buttons**: Physical start/stop/mode controls

### Remote Control Unit

- **ESP32**: Heltec WiFi LoRa 32 V3
- **LoRa Module**: SX1262 (868MHz)
- **Display**: OLED for status and menu
- **Buttons**: Up/Down for throttle control
- **Battery**: Rechargeable LiPo with voltage monitoring
- **Case**: Waterproof enclosure for marine use

## 🚀 Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32 development knowledge
- Basic electronics and servo wiring skills

## 📋 Configuration

### LoRa Settings

Configure radio parameters in the web interface or via code:

- **Frequency**: 868.0 MHz (adjustable)
- **Power**: 14 dBm (adjustable 2-20 dBm)
- **Spreading Factor**: 8 (balance of range vs. speed)
- **Bandwidth**: 125 kHz
- **Coding Rate**: 4/5

### Safety Parameters

- **Heartbeat Timeout**: 2000ms (connection loss threshold)
- **Stop Delay**: 5000ms (remote stop timeout)
- **Emergency Stop**: Immediate motor cutoff

## 🎯 Usage

### Starting the System

1. Power on both main and remote units
2. Wait for LoRa connection establishment
3. Use physical start button or web interface to begin
4. Select desired riding mode (SURF/SKIM/SMOOTH/MANUAL)

### Remote Operation

- **Single Press Up/Down**: Adjust throttle in 1% increments
- **Hold Up/Down**: Rapid throttle adjustment
- **Triple Press**: Enter menu mode for larger adjustments
- **Emergency**: Connection loss triggers automatic stop

### Mode Switching

- **Up + Down + Stop**: Cycle through riding modes
- Each mode has optimized throttle response curves
- Mode selection persists between power cycles

## 🏗️ Architecture

The system uses a multi-layered architecture:

```
┌─────────────────┐    LoRa 868MHz    ┌─────────────────┐
│  Remote Control │ ←─────────────────→ │  Main Display   │
│                 │                    │                 │
│ • Button Input  │                    │ • Engine Control│
│ • OLED Display  │                    │ • Web Interface │
│ • Battery Mon.  │                    │ • Safety Systems│
│ • LoRa Comm.    │                    │ • LoRa Comm.    │
└─────────────────┘                    └─────────────────┘
                                              │
                                              ▼
                                       ┌─────────────────┐
                                       │ Engine Hardware │
                                       │                 │
                                       │ • Gas Servo     │
                                       │ • Starter Relay │
                                       │ • Choke Servo   │
                                       │ • Safety Relays │
                                       └─────────────────┘
```

### Core Libraries

- **RiiWynchProtocol**: LoRa communication protocol and message handling
- **RiiWynchDisplay**: OLED display management and UI rendering
- **RiiWynchInput**: Button handling with debouncing and gesture recognition

### Communication Protocol

Messages use a structured format with priorities:

- **CRITICAL**: START_MOTOR, STOP_MOTOR (safety-critical)
- **HIGH**: ACK messages, MODE_UPDATE
- **NORMAL**: VALUE_SET, DISPLAY_UPDATE, KEEPALIVE
- **LOW**: HEARTBEAT, routine status

## 🛠️ Development

### Project Structure

```
RiiWynch/
├── main_display/          # Main engine controller
│   └── src/              # Core control logic
├── remote_display/        # Handheld remote
│   └── src/              # Remote interface logic
├── lib/                  # Custom libraries
│   ├── RiiWynchProtocol/ # LoRa communication
│   ├── RiiWynchDisplay/  # Display management
│   └── RiiWynchInput/    # Input handling
└── platformio.ini        # Build configuration
```

### Key Components

- **StateManager**: System state and throttle control
- **LoRaManager**: Wireless communication handling
- **DisplayManager**: OLED interface and status display
- **ProfileManager**: Riding mode management
- **HeartbeatManager**: Connection monitoring and safety
- **TaskManager**: FreeRTOS task coordination

## 🔒 Safety Considerations

⚠️ **IMPORTANT**: This system controls motorized watercraft. Always follow these safety guidelines:

1. **Test thoroughly** in controlled environments before water use
2. **Maintain redundant safety systems** (physical kill switches)
3. **Check LoRa connection** before each use
4. **Monitor battery levels** on both devices
5. **Follow local regulations** for motorized watercraft
6. **Use appropriate safety gear** (life jackets, etc.)

## 📊 Monitoring and Diagnostics

### Serial Output

Both units provide detailed logging:

```bash
pio device monitor -e main_display
pio device monitor -e remote_display
```

### Web Interface

Access via WiFi AP (192.168.4.1) for:

- Real-time system status
- LoRa signal strength
- Ride statistics and logs
- Configuration settings
- Troubleshooting information

## 🤝 Contributing

We welcome contributions! Please:

1. Fork the repository
2. Create a feature branch
3. Follow existing code style
4. Add appropriate documentation
5. Submit a pull request

## 🆘 Support

For issues, questions, or contributions:

- **Issues**: Use GitHub Issues for bug reports
- **Discussions**: GitHub Discussions for questions
- **Safety**: Always prioritize safety in modifications

---

**⚠️ Disclaimer**: This project involves controlling motorized watercraft. Users assume all responsibility for safe implementation, testing, and usage. Always follow local laws and safety regulations.
