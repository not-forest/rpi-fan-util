/* 
 *  file: main.c
 *
 *  The main C module that operates on Raspberry Pi fan by sending proper signals to
 *  rpi-fan-driver. This utility only works if drivers are included on the target device.
 *
 * */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define PWM_GPIOS 12:case 13:case 18:case 19
#define ADAPTIVE_PROCESS "adaptive_rpifan_pwm        "
#define KBUF_SIZE 4
#define TZ_BUF_SIZE 6
#define PWM_PERIOD 50000000

// Only prints in debug mode.
#define dfprintf(format, ...)           \
    if(debug) {                         \
        printf("\033[1;33m> "format"\033[0m", ##__VA_ARGS__);  \
    }

void adaptive(int, uint64_t, char*);
void usage(void);

/* 
 * Fan configuration structure
 *
 * Lower 5 bits for 26 available GPIO pins (the rest are reserved). Three bits for
 * PWM mode configuration.
 * */
union fan_config {
    uint8_t bytes;

    struct {
        uint8_t gpio_num: 5;
        uint8_t pwm_mode: 3;
    };
};

// Those are defined in rpi.h
#define WR_PWM_VALUE _IOW('r', 'a', uint64_t*)
#define R_PWM_VALUE _IOR('r', 'b', uint64_t*)

// FLAGS
static int debug = 0;

int main(int argc, char **argv) {
    union fan_config config, old_config;
    char value[KBUF_SIZE], old_value[KBUF_SIZE];
    char *pwm_value = NULL, *gpio_value = NULL, *duty_cycle = NULL;
    uint64_t adapt_ms = 0;
    int fd, opt = 0;

    while((opt = getopt(argc, argv, "dha:p:g:c:")) != -1) {
        switch(opt) {
            case 'd':
                debug = 1;
                dfprintf("Debug mode is on.\n");
                break;
            case 'h':
                usage();
                return 0;
            case 'a':
                adapt_ms = strtoull(optarg, NULL, 10); // Using adaptive PWM. Process will sleep for {optarg} ms.
                dfprintf("Adaptive PWM will be adjusted each %ld seconds\n", adapt_ms/1000);
                break;
            case 'p':
                pwm_value = optarg; // Argument for PWM.
                break;
            case 'g':
                gpio_value = optarg; // Argument for GPIO pin.
                break;
            case 'c':
                duty_cycle = optarg;
                break;
            case '?':
                if(optopt == 'p' || optopt == 'a' || optopt == 'c' || optopt == 'g') {
                    fprintf(stderr, "Option -%c requires an argument. Use -h for info.\n", optopt);
                    return -1;
                } 
                fprintf(stderr, "Unknown option argument %c. Use -h for the list of available flags.\n", optopt);
                return -1;
        }
    }

    // Opening the device.
    fd = open("/dev/rpifan", O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "Unable to open 'rpifan' device.\n");
        return -1;
    }
    dfprintf("Opened '/dev/rpifan' successfully.\n");

    if (read(fd, old_value, KBUF_SIZE) < 0) {
        perror("Error reading from device");
        close(fd);
        return -1;
    }
    old_config.bytes = (uint8_t) atoi(old_value);
    

    // Configuring GPIO pin.
    if(gpio_value != NULL) {
        int gpio_v = atoi(gpio_value);

        if (gpio_v < 2 || gpio_v > 30) {
            fprintf(stderr, "GPIO value must be between 2 and 30.\n");
            close(fd);
            return -1;
        }
        // If changing only the PWM, we have to get the current used GPIO. 
        config.pwm_mode = old_config.pwm_mode;
        config.gpio_num = (uint8_t) gpio_v;
    }

    // Configuring PWM value.
    if(pwm_value != NULL) {
        int pwm_v = atoi(pwm_value);
        if (pwm_v < 0 || pwm_v > 7) {
            fprintf(stderr, "PWM value must be between 0 and 7.\n");
            close(fd);
            return -1;
        }
        // If changing only the PWM, we have to get the current used GPIO. 
        config.gpio_num = old_config.gpio_num;
        config.pwm_mode = (uint8_t) pwm_v;
    }

    // Manual duty cycle.
    if(duty_cycle != NULL) {
        uint64_t duty_c = (atoi(duty_cycle) * PWM_PERIOD) / 100;
        
        if (duty_c < 0 || duty_c > PWM_PERIOD) {
            fprintf(stderr, "Custom PWM duty cycle must be between 1 and 100.\n");
            close(fd);
            return -1;
        }

        if(ioctl(fd, WR_PWM_VALUE, &duty_c)) {
            fprintf(stderr, "Unable to write value to the driver via IOCTL call.\n"); 
        }
        return 0;
    }

    // Configuring Both.
    if(gpio_value == NULL && pwm_value == NULL && adapt_ms == 0) {
        if(optind >= argc) {
            fprintf(stderr, "Value parameter must be provided. Use -h for more information.\n");
            close(fd);
            return -1;
        }

        // Parsing parameters and reading values.
        strncpy(value, argv[optind], KBUF_SIZE - 1);
        value[KBUF_SIZE - 1] = '\0';
        config.bytes = (uint8_t) atoi(value);

        if(config.bytes == 0 && (pwm_value == NULL || atoi(pwm_value) == 0)) {
            fprintf(stderr, "Provided value is not an integer of a valid type.\n");
            close(fd);
            return -1;
        }
    }


    // If adaptive PWM is set, creating a new process that handles it.
    if(adapt_ms) {
        switch(old_config.gpio_num) {
            case PWM_GPIOS: 
                int pid = fork();
                if(pid < 0) {
                    perror("Failed to fork process");
                    close(fd);
                    return -1;
                } else if (pid == 0) {
                    adaptive(fd, adapt_ms, argv[0]);
                    return 0;
                } else {
                    fprintf(stdout, "Adaptive PWM process started with PID: %d\n", pid);
                    goto _exit;  // Work of this process is done.
                }
            default:
                fprintf(stderr, "Current GPIO pin is not a PWM pin. Unable to use adaptive PWM.\n");    
        }
    }

    // Copying new data to the buffer.
    if(snprintf(value, KBUF_SIZE, "%d", config.bytes) < 0) {
        perror("Unable to write data in the buffer for writing");
        close(fd);
        return -1;
    }
 
    dfprintf("Current value: %d, writing value: %d.\n", old_config.bytes, config.bytes);
    // Writing new value to the fan driver.
    if(write(fd, value, KBUF_SIZE) < 0) {
        perror("Unable to write new data to the driver");
    }

_exit:
    close(fd);
    return 0;
}

// This function will only be executed from a child process. The fd would be provided to child.
void adaptive(int fd, uint64_t timeout, char *proc_name) {
    uint64_t curr_temp, max_temp = 0, new_dc;
    char tzbuf[TZ_BUF_SIZE];

    setsid();

    int tz_fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if(tz_fd < 0) {
        fprintf(stderr, "Unable to open 'thermal_zone' device, aborting...\n");
        close(fd);
        exit(-1);
    }

    sprintf(proc_name, ADAPTIVE_PROCESS);  // This changes the name of the child process.

    /* 
     * Until a proper signal is received, would work under the curtains. 
     *
     * Checks the current CPU 
     * */
    for(;;) { 
        if (lseek(tz_fd, 0, SEEK_SET) < 0 || read(tz_fd, tzbuf, TZ_BUF_SIZE) < 0) {
            fprintf(stderr, "Error reading from thermal zone device");
            close(fd);
            close(tz_fd);
            exit(-1);
        }
        curr_temp = (uint64_t) atoi(tzbuf);
        if(curr_temp < 0) {
            fprintf(stderr, "Unable to read data from thermal zone sensor. Retrying in %lu seconds.\n", timeout / 1000);
        }

        // This part is adaptive i.e defined the maximum dynamically.
        if(curr_temp > max_temp) {
            max_temp = curr_temp;
            dfprintf("New maximum temperature found. Remembering: %ld C.\n", curr_temp / 1000);
        }

        // Calculating new duty cycle based on maximal and current temperature.
        new_dc = (curr_temp * PWM_PERIOD) / max_temp; 

        dfprintf("CPU temperature: %ld C. Writing new duty cycle: %ld\n", curr_temp / 1000, new_dc);
        
        if(ioctl(fd, WR_PWM_VALUE, &new_dc)) {    //   Writing calibrated value.
            fprintf(stderr, "Unable to write value to the driver via IOCTL call.\n"); 
        }

        usleep(timeout * 1000);
    }
}

// Prints the usage methods.
void usage(void) {
    fprintf(stdout,
            "Usage: rpi_fan_util [flags] <value>\n"
            "Argument flags are defined as so:\n"
            "\t-h       \t\t Shows this message.\n"
            "\t-d       \t\t Enables debug messages.\n"
            "\t-p [mode]\t\t Only changes the PWM state with a provided value. This value can be from 1 to 7\n"
            "\t-c [dc]  \t\t Changes the PWM to a custom value from 0 to 100. The value must be a PWM duty cycle in percents %%"
            "\t-g [gpio]\t\t Only changes the current GPIO number. If new GPIO is not a PWM pin, the PWM would be off."
            "\t-a [ms.] \t\t Initializes an adaptive PWM. This flag will spawn a process that works in background and tracks the current temperature of the CPU. Based on this temperature it adjusts the PWM. Only one process can be spawned this way. The ms is amount of time that the process will sleep before checking the temperature again.\n"
            "\t-k       \t\t Kills the existing running process with adaptive PWM.\n");
}
