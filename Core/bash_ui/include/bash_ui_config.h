#ifndef UI_CONFIG_H
#define UI_CONFIG_H

/* Fixed-capacity runtime configuration. Projects may override any value from
 * their build system or from a project-local copy of this file. */
#ifndef UI_NAV_DEPTH
#define UI_NAV_DEPTH 4U
#endif

#ifndef UI_EVENT_QUEUE_LENGTH
#define UI_EVENT_QUEUE_LENGTH 2U
#endif

#ifndef UI_KEY_DEBOUNCE_MS
#define UI_KEY_DEBOUNCE_MS 20U
#endif

#ifndef UI_KEY_REPEAT_DELAY_MS
#define UI_KEY_REPEAT_DELAY_MS 450U
#endif

#ifndef UI_KEY_OK_LONG_PRESS_MS
#define UI_KEY_OK_LONG_PRESS_MS 700U
#endif

#ifndef UI_FRAME_INTERVAL_MS
#define UI_FRAME_INTERVAL_MS 20U
#endif

#ifndef UI_DISPLAY_TIMEOUT_MS
#define UI_DISPLAY_TIMEOUT_MS 250U
#endif

#define UI_REFRESH_BLOCKING 0
#define UI_REFRESH_IT       1
#define UI_REFRESH_DMA      2

#ifndef UI_REFRESH_MODE
#define UI_REFRESH_MODE UI_REFRESH_BLOCKING
#endif

#ifndef UI_ENABLE_HOME
#define UI_ENABLE_HOME 1
#endif
#ifndef UI_ENABLE_MENU
#define UI_ENABLE_MENU 0
#endif
#ifndef UI_ENABLE_INFO
#define UI_ENABLE_INFO 0
#endif
#ifndef UI_ENABLE_CUSTOM
#define UI_ENABLE_CUSTOM 1
#endif
#ifndef UI_ENABLE_INT_EDITOR
#define UI_ENABLE_INT_EDITOR 0
#endif
#ifndef UI_ENABLE_BOOL_EDITOR
#define UI_ENABLE_BOOL_EDITOR 0
#endif
#ifndef UI_ENABLE_CONFIRM
#define UI_ENABLE_CONFIRM 0
#endif
#ifndef UI_ENABLE_MESSAGE
#define UI_ENABLE_MESSAGE 0
#endif
#ifndef UI_ENABLE_TOAST
#define UI_ENABLE_TOAST 0
#endif
#ifndef UI_ENABLE_DRAW_HELPERS
#define UI_ENABLE_DRAW_HELPERS 0
#endif

#ifndef UI_TOAST_DEFAULT_MS
#define UI_TOAST_DEFAULT_MS 1500U
#endif

#if UI_NAV_DEPTH < 1U
#error "UI_NAV_DEPTH must be at least 1"
#endif
#if UI_NAV_DEPTH > 255U
#error "UI_NAV_DEPTH must fit the runtime uint8_t stack count"
#endif
#if UI_EVENT_QUEUE_LENGTH < 1U
#error "UI_EVENT_QUEUE_LENGTH must be at least 1"
#endif
#if UI_EVENT_QUEUE_LENGTH > 255U
#error "UI_EVENT_QUEUE_LENGTH must fit the runtime uint8_t queue count"
#endif
#if UI_FRAME_INTERVAL_MS < 1U
#error "UI_FRAME_INTERVAL_MS must be at least 1"
#endif

#endif
