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

static const int VERSION = 6;

#define BAUDRATE            115200
#define CYCLE_COUNT             10 // The number of 60 Hz cycles to sample.
#define SAMPLES_PER_SECOND  100000
#define SAMPLE_COUNT        ((CYCLE_COUNT * SAMPLES_PER_SECOND) / 60)

struct Waveform {
    float rms;
    int crossings[SAMPLE_COUNT];
    int crossings_count;
};

struct Power {
    float voltage;
    float current;
    float frequency;
};

// GPIO pins.
static const uint SYSTEM_LED_PIN = 25;
static const uint DEBUG_PIN = 22;

// ADC channels.
static const uint POWER_VOLTAGE = 0;
static const uint POWER_CURRENT = 1;
static const uint BATTERY_VOLTAGE = 2;

static const float VOLTAGE_SCALE = 0.170079;
static const float CURRENT_SCALE = 30.4387e-3;

static const uint32_t EXPIRED = 1 << 31;

static const uint32_t SAMPLE_PERIOD = 1000000 / SAMPLES_PER_SECOND; // In microseconds.
static const uint32_t UPDATE_PERIOD = 10000000; // In microseconds.

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
    adc_gpio_init(26);  // ADC channel 0.
    adc_gpio_init(27);  // ADC channel 1.
    adc_gpio_init(28);  // ADC channel 2.
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

/* Capture a sample of the waveform at adc_channel and compute the RMS value
 * and zero crossings. Returned rms value is scaled by scale_factor.
 */
struct Waveform rms(uint adc_channel,  float scale_factor) {
    struct Waveform waveform;
    int32_t total = 0, base_line, prev_value, buffer[SAMPLE_COUNT + 1];
    int32_t crossing_sum = 0;
    float squares = 0.0, period;
    int i, crossing_index = 0, crossings_count = 0;
    uint32_t t;

    // Capture a sample of the waveform.
    adc_select_input(adc_channel);
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        t = time_us_32();
        buffer[i] = adc_read();
        while(time_us_32() - t < SAMPLE_PERIOD) ;
    }

    // Compute base line.
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        total += buffer[i];
    }
    base_line = total / SAMPLE_COUNT;
    // printf("Base line = %d\n", base_line);

    // Subtract base line from all ADC values;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        buffer[i] = buffer[i] - base_line;
        // printf("%d\n", buffer[i]);
    }

    // printf("Crossings: ");
    prev_value = buffer[0];
    waveform.crossings_count = 0;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        // Accumulate the square of each voltage sample.
        squares += pow((float)buffer[i] * scale_factor, 2.0);

        // Test for zero crossing.
        if ((prev_value * buffer[i]) < 0) {
            waveform.crossings[crossing_index++] = i;
            prev_value = buffer[i];
            // printf("%d ", i);
            waveform.crossings_count += 1;
        }
    }
    // printf("\n");

    waveform.rms = sqrt(squares / (float)SAMPLE_COUNT);
    return waveform;
}

/* Capture a sample of the generator voltage output and compute RMS voltage
 * and frequency. Generator output voltage is 230 volt RMS. The corresponding
 * peak-to_peak voltage is 650 volts. The input range at the ADC is 0 to
 * 3.3 volts. ADC values are 0 to 4095. The 650 volt input is scaled to
 * a count of 3000. The sample period is 10 usecs. The sampling captures
 * CYCLE_COUNT cycles of the 60 Hz generator output.
 */
struct Power get_power_parameters (void) {
    struct Power power;
    struct Waveform waveform;
    int32_t total = 0, base_line, prev_value;
    int32_t crossing_sum = 0;
    float squares = 0.0, period;
    int i;
    uint32_t t;

    // Sample the voltage channel.
    waveform = rms(POWER_VOLTAGE, VOLTAGE_SCALE);
    power.voltage = waveform.rms;

    // Calculate the frequency by computing the average distance between zero
    // crossings.
    if (waveform.crossings_count > 10 && waveform.crossings_count < 100) {
        prev_value = waveform.crossings[0];
        for (i = 1; i < waveform.crossings_count; ++i) {
            crossing_sum += waveform.crossings[i] - prev_value;
            prev_value = waveform.crossings[i];
        }
        period = 1.053 * (float)SAMPLE_PERIOD * 1e-6 * (float)crossing_sum
            / (float)waveform.crossings_count;

        power.frequency = 1.0 / (2.0 * period);
    } else {
        power.frequency = 0.0;
    }

    // Sample the current channel.
    waveform = rms(POWER_CURRENT, CURRENT_SCALE);
    power.current = waveform.rms;

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
    return (float)sum * 251.49e-6;
}

int send_parameters(struct Power power_parameters, float battery_voltage) {
    char buffer[80];

    // Send generator voltage, generator frequency, generator current and battery voltage.
    // Values have a '!' prefix.
    sprintf(buffer, "!%0.2f %0.2f %0.2f %0.2f\n", power_parameters.voltage,
            power_parameters.frequency, power_parameters.current, battery_voltage);
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
            printf("Power voltage = %0.2f, current = %0.2f, frequency = %0.2f\n",
                    power.voltage, power.current, power.frequency);
            send_parameters(power, battery_voltage);
            gpio_put(SYSTEM_LED_PIN, 0);
        }
    }
    return 0;
}
