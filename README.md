# smartaq

SmartAQ is an ESP-IDF based automation and monitoring project for smart home / aquarium / sensor-control scenarios.  
It combines embedded device drivers, scheduling, REST APIs, and platform-independent utility code in a single codebase.

## Features

- ESP32 / ESP-IDF application support
- Modular device driver architecture
- Sensor and actuator drivers
- REST endpoints for device, settings, and stats management
- Time-based scheduling utilities
- Serialization helpers for device data
- Platform-independent core library for reuse and testing
- Unit tests for utility, container, and scheduling logic

## Project Structure

- `smartaq/` – main application source code
    - `actions/` – command/action logic
    - `drivers/` – hardware and device drivers
    - `rest/` – REST API handlers
    - `storage/` – filesystem and persistence code
    - `utils/` – shared helpers, containers, serialization, time, and ESP utilities
- `tests/` – unit tests
- `thirdparty/` – external embedded dependencies
- `header_only/` – header-only third-party utilities

## Requirements

### For ESP32 / embedded builds

- ESP-IDF installed and configured
- CMake 3.16+
- C++23 capable toolchain

### For local development and tests

- CMake 3.16+
- A C++23 compiler

## Configuration

The project uses CMake options and ESP-IDF configuration files for build settings.

Common project files:

- `sdkconfig`
- `partitions.csv`

## Main Components

### Drivers

The project includes drivers for a variety of devices and interfaces, such as:

- GPIO/pin control
- I2C-based sensors and peripherals
- ADC and DAC devices
- DHT sensors
- DS18B20 sensors
- Stepper motor control
- Dosing pump control
- pH probe handling
- Load cell / scale support
- Schedule-driven actuators

### REST API

REST modules provide endpoints for:

- Devices
- Settings – planned
- Statistics – planned

## Testing

The repository includes tests for:

- Bit utilities
- Stack strings
- Device values
- Schedule tracking
- Ring buffers
- Lookup tables
- Container helpers
- Time utilities

## Notes

- The project supports both embedded ESP-IDF builds and a local testable CMake build.
- Some optional components and libraries are included as submodules or external dependencies.
- Generated build artifacts should not be committed.

## License

See [`LICENSE`](LICENSE) for licensing details.