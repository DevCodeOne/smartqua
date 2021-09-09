# Top-Level functionality

Create devices, and set values maybe override value (which can be taken revoked later on, to return to previous value)
Create timers which execute actions or set plain values
Define actions, which set values to devices, or can execute functions with values
Have set of single purpose applications:
    - Schedule with values over the day (light, or temperature for example)
    - Trigger actions when in certain range or not, maybe custom functions, maybe be able to write trigger functions in lua
    - Co2 bottle scale with some extra features
    - Light application with channels and maybe colors even
Create custom functions in lua
Stats which accumulate over time with average values over a certain amount of time
General settings:
    - Timezone
    - WIFI (multiple access point, if all fails, be able to start hotspot)
    - Hostname
Remote backup functionality

JSON-API, which can be used by internal action handler and is used by the REST-API

REST-API

/api/v1/

<!-- All Drivers -->
/devices/
<!-- Stats, Devicevalues every x-seconds -->
/stats/
<!-- Functions, which control devices -->
/functions
<!-- -->
/timers
<!-- Custom functions, which also control devices -->
/custom_functions

Rules

1. Everything which is not a function, custom_function or timer is a device
2. Every device can be controlled by everything else

TODO:

- Write code for soft_timer_settings and related soft_timer code [X]
- Devices:
    - Device_info (implement retrieve_device_info [X] and retrieve_device_overview [X])
    - Device_options (with that custom actions and so on for the scale, and callibrating things) [X]
        - PWM  [ ]
        - DS18x20 [ ]
    - Retry creating devices [ ]
    - Add value which overrides current value, maybe with ring_buffer [ ]
    - New device ideas:
        - Support for pumps [ ]
        - Support for tds meter [ ]
- Log onto sd card [X]
    - Write logs to folder /logs [X]
- Central settings json [ ]
    - Write and read values to central json [ ]
    - Take needed actions to reflect changes e.g. rebooting [ ]
- Binary data stuff [X]
    - Write binary settings in seperate folder [X]
    - Write to tmp Files and mv them to the correct location [X]
- Put website on sd card [X]
- Make filenavigator [X] 
- Use https server [X]
- Use authentication for access [X]
- Create function safe-Write with tmp file and moving to safely store to a file [ ]
- Add Pagination for filenavigator [ ]
- Stop server, when connection drops [ ]
- Start access point, when there is not wireless connection [ ]
- Create file-upload functionality [ ]
- Make stack_string class [ ]
- Stream json output [ ]
- Fix partition table and remove webserver from there [X]
- Improve setting class [ ]
- Add possibility to unmount sd card [ ]
- Create remote backup function [ ]
- Resource system for gpios [X] and timers [X]
- Fix pwm code (timer stuff) [X]
- Write stuff to sd card [X]
- Use general logging framework [ ]
- Aggregate data for devices and make configurable [ ]
- Custom functions [ ]
- Check wording of drivers vs devices [ ]
- Convert scale related stuff to device_driver [ ]
- json_printf zero termination, what if the string is printed right to the end ? [ ]

# SmartAq-Board

- 2xOne-Wire Bus (3.3V and 5.5V both switchable)
- 2xI2C Bus (3.3V and 5.5V)
- 12 V Mosfet Co2-Output
- SD-Card Slot
- Analog-In/Out some 5V tolerant
- Digital-In/Out some 5V tolerant 
- Power Supply In 24-48V for LEDS -> 12v for Co2-Output + 5 and 3.3 V

Pin-Out

SD-Card:
    - GPIO14 - SCK
    - GPIO15 - MTDO
    - GPIO2 - D0
    - GPIO4 - D1
    - GPIO12 - D2
    - GPIO13 - D3

I2C
    - GPIO 21
    - GPIO 22
One-Wire
    - GPIO 10
    - GPIO 11

General Purpose
    - GPIO 15, 16, 17, 27, 32, 33, 34, 35