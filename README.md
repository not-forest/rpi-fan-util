# rpi-fan-util

`rpi_fan_util` is a user-space utility for managing the fan on a Raspberry Pi. It communicates with the `rpi-fan-driver` kernel module to control the fan's PWM settings based on GPIO inputs and CPU temperature.

## Features

- **Manual PWM Control**: Set the PWM mode directly.
- **Adaptive PWM**: Automatically adjust PWM based on CPU temperature.
- **GPIO Configuration**: Set the GPIO pin used for fan control.
- **Debugging**: Enable detailed debug messages.

## Prerequisites

- Raspberry Pi with the `rpi-fan-driver` kernel module installed and loaded.
- C compiler (e.g., `gcc`).
- Standard C library.

## Building the Utility

1. Clone the repository or download the source code.
2. Compile the utility with `./compile` bash script.

    ```bash
    ./compile
    ```
3. Additionaly the target device can be adjusted by changing the `COMPILER` variable within the bash script.

## Usage

```bash
./rpi_fan_util [flags] <value>
```

### Flags

- `-h`: Show the usage message.
- `-d`: Enable debug messages.
- `-p [mode]`: Set the PWM mode. Valid values are from 1 to 7.
- `-c [dc]`: Set a custom PWM duty cycle from 0 to 100%. The value must be a PWM duty cycle in percentage.
- `-g [gpio]`: Set the GPIO pin number. If the new GPIO is not a PWM pin, the PWM will be turned off.
- `-a [ms]`: Initialize adaptive PWM. This spawns a background process that adjusts the PWM based on CPU temperature every specified milliseconds.
- `-k`: Kill the existing running process with adaptive PWM.

### Notes

- The rpi_fan_util utility only works if the rpifan driver is included on the target device.
- When using adaptive PWM (`-a` flag), ensure the specified GPIO pin supports PWM.
- The `-c` flag allows setting the duty cycle as a percentage, where 0 means the fan is off and 100 means the fan runs at full speed.
- The `-k` flag terminates the background process managing adaptive PWM, if one is running.
