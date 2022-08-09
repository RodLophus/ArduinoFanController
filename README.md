# ArduinoFanController

Controls and monitors up to 4 PC-like fans based on the readings of an NTC temperature sensor.

The device can be used in two fashions:

- As a standalone module.  In this case, it will need to be connected to a computer via USB port only to perform the initial configuration.

- "Online". The module is connected to a USB port, allowing the temperature, PWM duty cycle and fan speed to be monitored and the configuration to be changed at any time.


## Specifications

4 4-pin fan connections.  Each fan speed is monitored individually.
2 configurable PWM channels (1 channel for each 2 fans).
1 NTC temperature sensor.

Each PWM channel have two temperature thresholds (low and high) and two values for the PWM duty cycle (minimum and maximum).  The controller will set the duty cycle as follows:

- Temperature < low temperature threshold -> Minimum PWM duty cycle
- Temperature > high temperature threshold -> Maximum PWM duty cycle
- Other cases -> PWM duty cycle varies linearly according to the temperature


## Getting Started

Connect the controller to a USB port.  It should show on the computer as a serial port.

Access the controller's serial port using a terminal program (9600bps, 8-bits, no parity, 1 stop bit)

Type <ENTER> until the message bellow shows up:

```
Press "c" to change the configuration or "m" to monitor
```

Press "c" to enter the configuration menu.

Note: you must configure **all** the parameters **before using the controller** for the first time!

```
Configuration

NTC temperature sensor:

(a) Resistance at 25 C..........: 46800 Ohm
(b) B factor....................: 3950

PWM channel 1 (fans 1 and 2):
(c) Low temperature threshold...: 40 C
(d) High temperature threshold..: 55 C
(e) Minimum PWM duty cycle......: 40 %
(f) Maximum PWM duty cycle......: 100 %

PWM channel 2 (fans 3 and 4):
(g) Low temperature threshold...: 40 C
(h) High temperature threshold..: 55 C
(i) Minimum PWM duty cycle......: 40 %
(j) Maximum PWM duty cycle......: 100 %

(z) Save and exit

```

Type the letter corresponding to the parameter you want to change.  Type the new value and press <ENTER>.

Once you finish configuring the controller, type "z" to save your changes.

Be sure to exit the configuration menu when you are done as the temperature will not be monitored when the controller is in configuration mode!


## Monitoring

Press "m" on the initial screen.  The controller will show the temperature read by the NTC sensor, the PWM duty cycle for both channels (PWM1 controls fans 1 and 2, and PWM2 controls fans 3 and 4) and the speed reading (in RPM) for all fans.  Press any key to return to the initial screen.

```
Real-time monitor.  Press any key to exit.

Temp(C) PWM1(%) PWM2(%)  RPM1  RPM2  RPM3  RPM4
26.81     40      40      210     0  5475     0
26.81     40      40      210     0  5475     0
26.83     40      40      210     0  5460     0
26.83     40      40      210     0  5460     0
26.85     40      40      210     0  5475     0
26.85     40      40      210     0  5475     0
26.86     40      40      210     0  5475     0
26.86     40      40      210     0  5475     0
26.89     40      40      210     0  5475     0
26.89     40      40      210     0  5475     0
Temp(C) PWM1(%) PWM2(%)  RPM1  RPM2  RPM3  RPM4
26.92     40      40      210     0  5475     0
26.92     40      40      210     0  5475     0
26.94     40      40      210     0  5475     0
26.94     40      40      210     0  5475     0
```


## Installation Notes

For standalone usage, close the jumper P7, so the controller can get power from the power supply cable and work with no USB connection.  In this case, you may omit all the zener diodes and replace all 150Ohm resistors with wire jumpers, as those components are intended to protect the computer's USB port from (unlikely) damages caused by fan failures.

For "online" usage, remove the jumper P7.  This way, the controller will be powered via the USB port, preventing the 5V from the power supply from being fed back to the computer's USB port.

Also for "online" usage, if your PC does not has an internal type-A USB port, you can make a USB-mini to USB-internal-header cable by cutting an USB-mini cable and installing a 5-pin (or 4-pin) "DuPont" connector on the other end.  The wires on USB cables are usually color-coded:

| Wire color        | Function       | DuPont connector pin |
| ----------------- | -------------- | -------------------- |
| Red               | +5V            | 1                    |
| White             | Data +         | 2                    |
| Green             | Data -         | 3                    |
| Black             | GND            | 4                    |
| (shielding braid) | Chassis ground | 5 (optional)         |

The cable should look like this:

![USB Cable](/img/usb_cable.png)

