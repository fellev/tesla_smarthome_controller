#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "bt_event.h"
#include "bt_gpio.h"
#include "gatt_svc.h"
// #include "ble_server.h"

static char button_event_msg[32];

char * get_button_event_msg(void) {
    return button_event_msg;
}

void update_button_event_msg(const char *msg) {
    snprintf(button_event_msg, sizeof(button_event_msg), "%s", msg);
}

#define TAG "BT_EVT"
#define EVENT_QUEUE_LEN 10

static QueueHandle_t event_queue;

void app_notify_button_event(const char* type, int button_number) {
    char msg[32];    
    snprintf(msg, sizeof(msg), "%s:%d", type, button_number);
    ESP_LOGI(TAG, "Sending BLE event: %s", msg);
    update_button_event_msg(msg);
    send_button_pressed_indication();
}

static void bt_event_task(void *arg) {
    button_event_t evt;
    
    while (1) {
        if (xQueueReceive(event_queue, &evt, portMAX_DELAY)) {
            const char* type_str = (evt.type == BUTTON_EVENT_SHORT) ? "short" : "long";
            int button_index = get_button_index(evt.button_number);
            if (button_index < 0) {
                ESP_LOGW(TAG, "Button index not found for GPIO %d", evt.button_number);
                continue;
            }
            button_index++; // Convert to 1-based index for user-friendly output
            app_notify_button_event(type_str, button_index);
        }
    }
}

void bt_event_task_start(void) {
    event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(button_event_t));
    if (event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    xTaskCreate(bt_event_task, "bt_event_task", 4096, NULL, 10, NULL);
}

bool bt_event_send(button_event_type_t type, int button_number) {
    if (!event_queue) return false;
    button_event_t evt = {
        .type = type,
        .button_number = button_number
    };
    return xQueueSend(event_queue, &evt, 0) == pdTRUE;
}

