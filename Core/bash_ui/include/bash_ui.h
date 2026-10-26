#ifndef UI_H
#define UI_H

#include "bash_ui_config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t UI_PageId;

#define UI_PAGE_NONE  ((UI_PageId)0U)

typedef enum {
    UI_OK = 0,
    UI_INVALID_ARGUMENT,
    UI_NOT_INITIALIZED,
    UI_CONFIG_ERROR,
    UI_BUSY,
    UI_NAVIGATION_FULL,
    UI_UNSUPPORTED,
    UI_DISPLAY_ERROR,
    UI_DISPLAY_TIMEOUT,
    UI_NOT_ALLOWED
} UI_Status;

typedef struct {
    UI_Status code;
    UI_PageId page;
    uint16_t index;
} UI_ErrorInfo;

enum {
    UI_KEY_UP = 1U << 0,
    UI_KEY_DOWN = 1U << 1,
    UI_KEY_OK = 1U << 2
};

typedef struct {
    uint8_t keys;
    int16_t encoder_delta; /* Positive is Down; negative is Up. */
} UI_Input;

typedef enum {
    UI_INPUT_UP = 0,
    UI_INPUT_DOWN,
    UI_INPUT_OK
} UI_InputAction;

typedef enum {
    UI_INPUT_PRESS = 0,
    UI_INPUT_REPEAT,
    UI_INPUT_RELEASE,
    UI_INPUT_ENCODER
} UI_InputSource;

typedef struct {
    UI_InputAction action;
    UI_InputSource source;
    uint16_t steps;
} UI_InputEvent;

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_CUSTOM
} UI_PageType;

typedef struct {
    UI_PageType type;
    uint16_t index;
} UI_PageRoute;

typedef struct {
    const char *label;
    const uint8_t *icon_xbm_32x32;
    UI_PageId target_page;
} UI_HomeItem;

typedef struct {
    const UI_HomeItem *items;
    uint16_t item_count;
} UI_HomePage;

typedef struct {
    UI_PageId root_page;
    const UI_PageRoute *routes;
    uint16_t route_count;
    const UI_HomePage *home_pages;
    uint16_t home_page_count;
    uint16_t custom_page_count;
} UI_App;

UI_Status UI_Init(const UI_App *app);
UI_Status UI_Update(uint32_t now_ms, UI_Input input);
void UI_Invalidate(void);
UI_Status UI_RecoverDisplay(void);

bool UI_PollError(UI_ErrorInfo *out_error);

UI_Status UI_CustomRequestClose(void);

/* Fixed application adapter. The application defines these five symbols. */
void UI_CustomOnEnter(UI_PageId page);
void UI_CustomOnLeave(UI_PageId page);
void UI_CustomOnInput(UI_PageId page, UI_InputEvent event);
bool UI_CustomOnTick(UI_PageId page, uint32_t now_ms);
void UI_CustomOnDraw(UI_PageId page, int16_t x_offset,
                        int16_t clip_x, uint16_t clip_width);

#ifdef __cplusplus
}
#endif

#endif
