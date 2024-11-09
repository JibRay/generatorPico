/* generatorPico.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/time.h"

static const int VERSION = 3;

#define BAUDRATE 115200

// GPIO pins.
static const uint SYSTEM_LED_PIN = 25;
static const uint DEBUG_PIN = 22;

// ADC channels.
static const uint POWER_VOLTAGE = 0;
static const uint BATTERY_VOLTAGE = 1;

static const float VOLTAGE_SCALE = 0.173043;

static const uint32_t EXPIRED = 1 << 31;

static const int POWER_SAMPLE_COUNT = 16667;
static const int POWER_SAMPLE_PERIOD = 10; // In microsends.

static const float UPDATE_PERIOD = 10e6;

struct Power {
    float voltage;
    float frequency;
};

int init_serial2(void) {
    // Initialize UART0.
    uart_init(uart0, BAUDRATE);

    // Set the GPIO pin mux to the UART-0 TX/RX.
    // GPIO 0 and 1 are the default UART pins on the Raspberry Pi Pico.
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    return 0;
}

int init_adc(void) {
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    return 0;
}

int init_io(void) {
    // Initialize serial port 1 (main serial port). Default baudrate is
    // 115200. This port is used only for debugging.
    stdio_init_all();

    // Initialize the system LED pin. This is not used for normal operation.
    gpio_init(SYSTEM_LED_PIN);
    gpio_set_dir(SYSTEM_LED_PIN, GPIO_OUT);

    // Initialize debug pin.
    gpio_init(DEBUG_PIN);
    gpio_set_dir(DEBUG_PIN, GPIO_OUT);

    init_serial2();
    init_adc();

    return 0;
}

/* Capture a sample of the generator voltage output and compute RMS voltage
 * and frequency. Generator output voltage is 230 volt RMS. The corresponding
 * peak-to_peak voltage is 650 volts. The input range at the ADC is 0 to 
 * 3.3 volts. ADC values are 0 to 4095. The 650 volt input is scaled to
 * a count of 3000. The sample period is 10 usecs. The sampling captures
 * 10 cycles of the 60 Hz generator output.
 */
struct Power get_power_parameters (void) {
    struct Power power;
    int32_t total = 0, base_line, prev_value, buffer[POWER_SAMPLE_COUNT + 1];
    int32_t crossing_sum = 0;
    float squares = 0.0, period;
    int i, crossings[POWER_SAMPLE_COUNT], crossing_index = 0, crossing_count = 0;
    uint32_t t;

    adc_select_input(POWER_VOLTAGE);
    for (i = 0; i < POWER_SAMPLE_COUNT; ++i) {
        t = time_us_32();
        buffer[i] = adc_read();
        while(time_us_32() - t < POWER_SAMPLE_PERIOD) ;
    }

    // Compute base line.
    for (i = 0; i < POWER_SAMPLE_COUNT; ++i) {
        total += buffer[i];
    }
    base_line = total / POWER_SAMPLE_COUNT;
    // printf("Base line = %d\n", base_line);

    // Subtract base line from all ADC values;
    for (i = 0; i < POWER_SAMPLE_COUNT; ++i) {
        buffer[i] = buffer[i] - base_line;
        // printf("%d\n", buffer[i]);
    }

    // Convert ADC values to voltage squared and compute the total (sum of squares).
    // printf("Crossings: ");
    prev_value = buffer[0];
    for (i = 0; i < POWER_SAMPLE_COUNT; ++i) {
        squares += pow((float)buffer[i] * VOLTAGE_SCALE, 2.0);

        // Test for zero crossing.
        if ((prev_value * buffer[i]) < 0) {
            crossings[crossing_index++] = i;
            prev_value = buffer[i];
            // printf("%d ", i);
            crossing_count += 1;
        }
    }
    // printf("\n");

    // Compute the RMS voltage.
    power.voltage = sqrt(squares / (float)POWER_SAMPLE_COUNT);

    // Calculated the frequency by computing the average distance between zero
    // crossings.
    if (crossing_count > 10 && crossing_count < 100) {
        prev_value = crossings[0];
        for (i = 1; i < crossing_index; ++i) {
            crossing_sum += crossings[i] - prev_value;
            prev_value = crossings[i];
        }
        period = 1.053 * (float)POWER_SAMPLE_PERIOD * 1e-6 * (float)crossing_sum
            / (float)crossing_index;

        power.frequency = 1.0 / (2.0 * period);
    } else {
        power.frequency = 0.0;
    }

    return power;
}

float get_battery_voltage(void) {
    uint16_t sum = 0;

    adc_select_input(BATTERY_VOLTAGE);
    for (int i = 0; i < 16; ++i) {
        sum += adc_read();
        sleep_ms(1);
    }

    // Return ADC value scaled to 20 volts.
    return (float)sum * 305.18e-6;
}

int send_parameters(struct Power power_parameters, float battery_voltage) {
    char buffer[80];

    // Send generator voltage, generator frequency and battery voltage.
    // Values have a '!' prefix.
    sprintf(buffer, "!%0.2f %0.2f %0.2f\n", power_parameters.voltage,
            power_parameters.frequency, battery_voltage);
    uart_puts(uart0, buffer);

    return 0;
}

int main(void) {
    float battery_voltage;
    uint32_t update_timer = time_us_32() + UPDATE_PERIOD;
    struct Power power;

    init_io();

    printf("generatorPico version %d\n", VERSION);

    while(1) {
        if ((update_timer - time_us_32()) > EXPIRED) {
            update_timer = time_us_32() + UPDATE_PERIOD;
            gpio_put(SYSTEM_LED_PIN, 1);
            battery_voltage = get_battery_voltage();
            printf("Battery voltage  = %0.2f, ", battery_voltage);
            power = get_power_parameters();
            printf("Power voltage = %0.2f, frequency = %0.2f\n", power.voltage, power.frequency);
            send_parameters(power, battery_voltage);
            gpio_put(SYSTEM_LED_PIN, 0);
        }
    }
    return 0;
}
