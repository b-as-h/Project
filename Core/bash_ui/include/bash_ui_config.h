#ifndef UI_CONFIG_H
#define UI_CONFIG_H

/* Fixed-capacity runtime configuration. Projects may override any value from
 * their build system or from a project-local copy of this file. */

#ifndef UI_NAV_DEPTH
#define UI_NAV_DEPTH 4U
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

#if UI_NAV_DEPTH < 1U
#error "UI_NAV_DEPTH must be at least 1"
#endif
#if UI_NAV_DEPTH > 255U
#error "UI_NAV_DEPTH must fit the runtime uint8_t stack count"
#endif
#if UI_FRAME_INTERVAL_MS < 1U
#error "UI_FRAME_INTERVAL_MS must be at least 1"
#endif

#endif
