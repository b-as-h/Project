#ifndef UI_H
#define UI_H

#include "bash_ui_config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t UI_PageId;
typedef uint16_t UI_EventId;

#define UI_PAGE_NONE  ((UI_PageId)0U)
#define UI_EVENT_NONE ((UI_EventId)0U)

typedef enum {
    UI_OK = 0,
    UI_INVALID_ARGUMENT,
    UI_NOT_INITIALIZED,
    UI_CONFIG_ERROR,
    UI_BUSY,
    UI_QUEUE_FULL,
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
    UI_PAGE_MENU,
    UI_PAGE_INFO,
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

typedef enum {
    UI_MENU_PAGE = 0,
    UI_MENU_ACTION,
    UI_MENU_INT,
    UI_MENU_BOOL,
    UI_MENU_CONFIRM
} UI_MenuItemType;

typedef struct {
    const char *label;
    UI_MenuItemType type;
    uint16_t ref;
} UI_MenuItem;

typedef struct {
    const char *title;
    const UI_MenuItem *items;
    uint8_t *item_states; /* Optional packed 2-bit state table. */
    uint16_t item_count;
} UI_MenuPage;

typedef enum {
    UI_ITEM_NORMAL = 0,
    UI_ITEM_HIDDEN = 1,
    UI_ITEM_DISABLED = 2,
    UI_ITEM_RESERVED = 3
} UI_ItemState;

typedef struct {
    const char *name;
    const char *value;
} UI_InfoRow;

typedef struct {
    const char *title; /* NULL or empty omits the title bar. */
    const UI_InfoRow *rows;
    uint16_t row_count;
} UI_InfoPage;

typedef struct {
    const char *title;
    int32_t *value;
    int32_t minimum;
    int32_t maximum;
    uint32_t step;
    const char *unit;
    UI_EventId changed_event;
} UI_IntBinding;

typedef struct {
    const char *title;
    bool *value;
    UI_EventId changed_event;
} UI_BoolBinding;

typedef struct {
    const char *text;
    UI_EventId confirmed_event;
    UI_EventId cancelled_event;
} UI_ConfirmDesc;

typedef struct {
    const uint8_t *home_font;
    const uint8_t *title_font;
    const uint8_t *body_font;
} UI_Fonts;

typedef struct {
    const char *return_text;
    const char *cancel_text;
    const char *confirm_text;
    const char *on_text;
    const char *off_text;
    const char *message_title;
} UI_Texts;

typedef struct {
    UI_PageId root_page;
    const UI_PageRoute *routes;
    uint16_t route_count;
    const UI_HomePage *home_pages;
    uint16_t home_page_count;
    const UI_MenuPage *menu_pages;
    uint16_t menu_page_count;
    const UI_InfoPage *info_pages;
    uint16_t info_page_count;
    uint16_t custom_page_count;
    const UI_IntBinding *int_bindings;
    uint16_t int_binding_count;
    const UI_BoolBinding *bool_bindings;
    uint16_t bool_binding_count;
    const UI_ConfirmDesc *confirm_descs;
    uint16_t confirm_desc_count;
    UI_Fonts fonts;
    UI_Texts texts;
} UI_App;

UI_Status UI_Init(const UI_App *app);
UI_Status UI_Update(uint32_t now_ms, UI_Input input);
void UI_Invalidate(void);
UI_Status UI_RecoverDisplay(void);

bool UI_PollEvent(UI_EventId *out_event);
bool UI_PollError(UI_ErrorInfo *out_error);
UI_Status UI_SetMenuItemState(UI_PageId page,
                                    uint16_t item_index,
                                    UI_ItemState state);

UI_Status UI_OpenIntEditor(uint16_t binding_index);
UI_Status UI_OpenBoolEditor(uint16_t binding_index);
UI_Status UI_OpenConfirm(uint16_t confirm_index);
UI_Status UI_ShowMessage(const char *text, UI_EventId acknowledged_event);
UI_Status UI_ShowToast(const char *text, uint32_t duration_ms);

UI_Status UI_CustomRequestClose(void);
UI_Status UI_CustomFinish(UI_EventId event);

#if UI_ENABLE_CUSTOM
/* Fixed application adapter. The application defines these five symbols. */
void UI_CustomOnEnter(UI_PageId page);
void UI_CustomOnLeave(UI_PageId page);
void UI_CustomOnInput(UI_PageId page, UI_InputEvent event);
bool UI_CustomOnTick(UI_PageId page, uint32_t now_ms);
void UI_CustomOnDraw(UI_PageId page, int16_t x_offset,
                        int16_t clip_x, uint16_t clip_width);
#endif

#ifdef __cplusplus
}
#endif

#endif
