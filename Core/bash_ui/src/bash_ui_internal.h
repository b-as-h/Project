#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "bash_ui.h"
#include "bash_oled.h"

#include <stddef.h>

#define UI_Q12_ONE 4096U

#define UI_HOME_MS             300U
#define UI_MENU_MS             200U
#define UI_PAGE_MS             220U
#define UI_INFO_SCROLL_MS      180U
#define UI_DIALOG_OPEN_MS      180U
#define UI_DIALOG_CLOSE_MS     150U
#define UI_FOCUS_MS            160U
#define UI_INT_VALUE_MS        100U
#define UI_BOOL_VALUE_MS       120U
#define UI_TOAST_MS            120U

#define UI_SCREEN_WIDTH        128
#define UI_SCREEN_HEIGHT       64
#define UI_MENU_HEADER_HEIGHT  14
#define UI_INFO_HEADER_HEIGHT  16
#define UI_ROW_HEIGHT          16
#define UI_VISIBLE_ROWS        3
#define UI_PAGE_PARALLAX       32

typedef struct {
    UI_PageId page;
    uint16_t selected;
    uint16_t top;
    int32_t scroll_q8;
    int32_t focus_q8;
} UI_PageState;

typedef struct {
    uint32_t changed_at;
    uint32_t pressed_at;
    uint32_t repeat_at;
    uint8_t candidate;
    uint8_t stable;
    uint8_t armed;
    uint8_t repeat_sent;
} UI_KeyState;

typedef struct {
    int32_t scroll_from_q8;
    int32_t focus_from_q8;
    uint32_t started;
    uint16_t duration;
    uint8_t active;
} UI_ListAnimation;

typedef struct {
    int32_t offset_from_q8;
    int32_t label_from_q8;
    int32_t label_to_q8;
    uint32_t started;
    uint8_t active;
} UI_HomeAnimation;

typedef struct {
    UI_PageState outgoing;
    uint32_t started;
    uint8_t active;
    uint8_t reverse;
    uint8_t leave_custom;
} UI_PageTransition;

typedef enum {
    UI_OVERLAY_NONE = 0,
    UI_OVERLAY_INT,
    UI_OVERLAY_BOOL,
    UI_OVERLAY_CONFIRM,
    UI_OVERLAY_MESSAGE
} UI_OverlayType;

typedef enum {
    UI_FOCUS_VALUE = 0,
    UI_FOCUS_CONFIRM,
    UI_FOCUS_CANCEL
} UI_DialogFocus;

typedef struct {
    UI_OverlayType type;
    uint16_t ref;
    const char *text;
    UI_EventId event;
    int32_t draft;
    int32_t original;
    int32_t previous;
    int32_t focus_x_q8;
    int32_t focus_y_q8;
    int32_t focus_w_q8;
    int32_t focus_h_q8;
    int32_t focus_from_x_q8;
    int32_t focus_from_y_q8;
    int32_t focus_from_w_q8;
    int32_t focus_from_h_q8;
    uint32_t phase_started;
    uint32_t focus_started;
    uint32_t value_started;
    int16_t origin_y;
    uint16_t phase_q12;
    uint16_t phase_from_q12;
    uint16_t value_q12;
    uint8_t focus;
    int8_t value_direction;
    uint8_t editing;
    uint8_t closing;
    uint8_t focus_moving;
    uint8_t value_moving;
} UI_Overlay;

typedef struct {
    const char *text;
    uint32_t shown_at;
    uint32_t expires_at;
    uint16_t phase_q12;
    uint8_t visible;
    uint8_t closing;
} UI_Toast;

typedef enum {
    UI_DEFER_NONE = 0,
    UI_DEFER_CLOSE,
    UI_DEFER_FINISH,
    UI_DEFER_INT,
    UI_DEFER_BOOL,
    UI_DEFER_CONFIRM,
    UI_DEFER_MESSAGE,
    UI_DEFER_TOAST
} UI_DeferredType;

typedef struct {
    UI_DeferredType type;
    uint16_t ref;
    UI_EventId event;
    const char *text;
    uint32_t duration;
} UI_Deferred;

typedef struct {
    const UI_App *app;
    UI_PageState current;
    UI_PageState stack[UI_NAV_DEPTH];
    UI_PageTransition transition;
    UI_HomeAnimation home_anim;
    UI_ListAnimation list_anim;
    UI_Overlay overlay;
    UI_Toast toast;
    UI_Deferred deferred;
    UI_KeyState keys[3];
    UI_EventId events[UI_EVENT_QUEUE_LENGTH];
    UI_ErrorInfo error;
    uint32_t last_update;
    uint32_t next_frame;
    uint32_t transfer_started;
    uint8_t stack_count;
    uint8_t event_head;
    uint8_t event_count;
    uint8_t initialized;
    uint8_t keys_initialized;
    uint8_t transfer_active;
    uint8_t frame_ready;
    uint8_t dirty;
    uint8_t display_fault;
    uint8_t error_valid;
    uint8_t in_callback;
    uint8_t reject_flash;
    uint8_t blocked_keys;
} UI_Runtime;

extern UI_Runtime ui_state;

uint16_t UI_EaseQ12(uint32_t elapsed, uint32_t duration);
int32_t UI_LerpQ12(int32_t from, int32_t to, uint16_t progress);
int16_t UI_RoundQ8(int32_t value);

void UI_RecordError(UI_Status code, UI_PageId page, uint16_t index);
bool UI_QueueEvent(UI_EventId event);
const UI_PageRoute *UI_GetRoute(UI_PageId page);
UI_PageType UI_CurrentPageType(void);

void UI_ResetPageState(UI_PageState *state, UI_PageId page);
UI_Status UI_NavigateTo(UI_PageId page, uint32_t now);
UI_Status UI_NavigateBack(uint32_t now);
void UI_AnimateNavigation(uint32_t now);
void UI_DrawScene(uint32_t now);
void UI_DrawPage(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width, uint32_t now);

void UI_HomeAnimate(uint32_t now);
void UI_HomeInput(UI_InputEvent event, uint32_t now);
void UI_HomeDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);

UI_ItemState UI_MenuGetItemState(const UI_MenuPage *page,
                                       uint16_t item_index);
void UI_MenuRepairFocus(UI_PageState *state);
void UI_MenuAnimate(uint32_t now);
void UI_MenuInput(UI_InputEvent event, uint32_t now);
void UI_MenuDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);

void UI_InfoAnimate(uint32_t now);
void UI_InfoInput(UI_InputEvent event, uint32_t now);
void UI_InfoDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width);
void UI_CustomInput(UI_InputEvent event);
void UI_CustomTick(uint32_t now);

UI_Status UI_DialogOpenInt(uint16_t index, uint32_t now);
UI_Status UI_DialogOpenBool(uint16_t index, uint32_t now);
UI_Status UI_DialogOpenConfirm(uint16_t index, uint32_t now);
UI_Status UI_DialogOpenMessage(const char *text, UI_EventId event,
                                     uint32_t now);
UI_Status UI_ToastOpen(const char *text, uint32_t duration,
                             uint32_t now);
void UI_DialogInput(UI_InputEvent event, uint32_t now);
void UI_DialogAnimate(uint32_t now);
void UI_DialogDraw(void);
void UI_ToastDraw(void);
bool UI_DialogActive(void);

void UI_DispatchInput(UI_InputEvent event, uint32_t now);
void UI_ApplyDeferred(uint32_t now);
UI_Status UI_Defer(UI_DeferredType type, uint16_t ref,
                         const char *text, UI_EventId event,
                         uint32_t duration);

void UI_FormatInt(int32_t value, const char *unit, char *buffer,
                     size_t capacity);
void UI_DrawCentered(int16_t x, int16_t y, uint16_t width,
                        const char *text);

#endif
