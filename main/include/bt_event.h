#ifndef BT_EVENT_H
#define BT_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUTTON_EVENT_SHORT,
    BUTTON_EVENT_LONG
} button_event_type_t;

typedef struct {
    button_event_type_t type;
    int button_number;
} button_event_t;

void bt_event_task_start(void);
bool bt_event_send(button_event_type_t type, int button_number);

#ifdef __cplusplus
}
#endif

#endif // BT_EVENT_H