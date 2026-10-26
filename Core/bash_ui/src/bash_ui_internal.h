#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "bash_ui.h"
#include "bash_oled.h"

#include <stddef.h>

#define UI_Q12_ONE 4096U

#define UI_HOME_MS             300U
#define UI_PAGE_MS             220U

#define UI_SCREEN_WIDTH        128
#define UI_SCREEN_HEIGHT       64
#define UI_PAGE_PARALLAX       32

typedef struct {
    UI_PageId page;
    uint16_t selected;
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

typedef struct {
    const UI_App *app;
    UI_PageState current;
    UI_PageState stack[UI_NAV_DEPTH];
    UI_PageTransition transition;
    UI_HomeAnimation home_anim;
    UI_KeyState keys[3];
    UI_ErrorInfo error;
    uint32_t last_update;
    uint32_t next_frame;
    uint32_t transfer_started;
    uint8_t stack_count;
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
    uint8_t close_pending;
} UI_Runtime;

extern UI_Runtime ui_state;

uint16_t UI_EaseQ12(uint32_t elapsed, uint32_t duration);
int32_t UI_LerpQ12(int32_t from, int32_t to, uint16_t progress);
int16_t UI_RoundQ8(int32_t value);

void UI_RecordError(UI_Status code, UI_PageId page, uint16_t index);
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

void UI_CustomInput(UI_InputEvent event);
void UI_CustomTick(uint32_t now);

void UI_DispatchInput(UI_InputEvent event, uint32_t now);

#endif
