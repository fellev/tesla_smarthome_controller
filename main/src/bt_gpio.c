/**
 * @file bt_gpio.c
 * @brief Bluetooth GPIO control implementation.
 *
 * This file contains functions for controlling GPIOs for the Bluetooth door key project.
 */

#include "bt_gpio.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bt_event.h"



#define NUM_BUTTONS 4
#define LONG_PRESS_TIME_MS 1000  // Threshold for long press

// Define your GPIO numbers here
static const gpio_num_t button_gpios[NUM_BUTTONS] = {
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
};

static const char *TAG = "BUTTONS";

// Typedef for callback
typedef void (*button_cb_t)(gpio_num_t gpio, bool long_press);

// User-provided callback
static button_cb_t user_button_callback = NULL;

// Timer and press time tracking
typedef struct {
    TimerHandle_t timer;
    int64_t press_time;
    gpio_num_t gpio;
    bool long_press_triggered;
} button_state_t;

static button_state_t button_states[NUM_BUTTONS];

static void IRAM_ATTR button_isr_handler(void *arg) {
    gpio_num_t gpio = (gpio_num_t)(uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int index = -1;
    static int64_t last_isr_time[NUM_BUTTONS] = {0};
    int64_t now = esp_timer_get_time() / 1000; // ms
    const int DEBOUNCE_TIME_MS = 20;

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (button_states[i].gpio == gpio) {
            index = i;
            break;
        }
    }
    if (index < 0) return;
    
    // Debounce logic
    if ((now - last_isr_time[index]) < DEBOUNCE_TIME_MS) {
        return; // Ignore due to debounce
    }
    last_isr_time[index] = now;

    if (gpio_get_level(gpio) == 0) {  // Falling edge = press
        button_states[index].press_time = esp_timer_get_time() / 1000; // ms
        button_states[index].long_press_triggered = false;
        xTimerStartFromISR(button_states[index].timer, &xHigherPriorityTaskWoken);
    } else {  // Rising edge = release
        xTimerStopFromISR(button_states[index].timer, &xHigherPriorityTaskWoken);

        // Only call user_button_callback here (on release)
        int64_t now = esp_timer_get_time() / 1000;
        if (button_states[index].long_press_triggered) {
            if (user_button_callback) {
                user_button_callback(gpio, true); // long press
            }
        } else if ((now - button_states[index].press_time) < LONG_PRESS_TIME_MS) {
            if (user_button_callback) {
                user_button_callback(gpio, false); // short press
            }
        }
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void long_press_timer_callback(TimerHandle_t xTimer) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (xTimer == button_states[i].timer) {
            if (gpio_get_level(button_states[i].gpio) == 0) {  // Still pressed
                button_states[i].long_press_triggered = true;
            }
            break;
        }
    }
}
static void button_event_handler(gpio_num_t gpio, bool long_press) {
    // ESP_LOGI("BTN_EVT", "GPIO %d %s press", (uint16_t)gpio, long_press ? "LONG" : "SHORT");

    // Do something like send Bluetooth command
    if (long_press) {
        bt_event_send(BUTTON_EVENT_LONG, gpio);
    } else {
        bt_event_send(BUTTON_EVENT_SHORT, gpio);
    }
}

void init_buttons(void) {
    user_button_callback = button_event_handler; // Set the user callback

    // Install ISR service only once
    gpio_install_isr_service(0);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_num_t gpio = button_gpios[i];
        button_states[i].gpio = gpio;

        button_states[i].timer = xTimerCreate(
            "btn_timer", 
            pdMS_TO_TICKS(LONG_PRESS_TIME_MS), 
            pdFALSE, 
            NULL, 
            long_press_timer_callback
        );

        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << gpio,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE
        };
        gpio_config(&io_conf);

        gpio_isr_handler_add(gpio, button_isr_handler, (void *)(uint32_t)gpio);
    }

    ESP_LOGI(TAG, "Buttons initialized");
}
 
int get_button_index(gpio_num_t gpio) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (button_gpios[i] == gpio) {
            return i;
        }
    }
    return -1;
}






