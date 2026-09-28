#include "bash_ui_internal.h"

const UI_PageRoute *UI_GetRoute(UI_PageId page)
{
    if (ui_state.app == NULL || page == UI_PAGE_NONE ||
        page > ui_state.app->route_count) {
        return NULL;
    }
    return &ui_state.app->routes[page - 1U];
}

UI_PageType UI_CurrentPageType(void)
{
    const UI_PageRoute *route = UI_GetRoute(ui_state.current.page);
    return route != NULL ? route->type : UI_PAGE_HOME;
}

void UI_ResetPageState(UI_PageState *state, UI_PageId page)
{
    const UI_PageRoute *route;

    state->page = page;
    state->selected = 0U;
    state->top = 0U;
    state->scroll_q8 = 0;
    state->focus_q8 = 0;
    route = UI_GetRoute(page);
    if (route != NULL && route->type == UI_PAGE_MENU) {
        UI_MenuRepairFocus(state);
    }
}

static void ui_start_transition(const UI_PageState *outgoing,
                                   bool reverse, uint32_t now)
{
    ui_state.transition.outgoing = *outgoing;
    ui_state.transition.started = now;
    ui_state.transition.active = 1U;
    ui_state.transition.reverse = reverse ? 1U : 0U;
    ui_state.transition.leave_custom = 0U;
    ui_state.blocked_keys |= (uint8_t)((ui_state.keys[0].stable ? UI_KEY_UP : 0U) |
                           (ui_state.keys[1].stable ? UI_KEY_DOWN : 0U) |
                           (ui_state.keys[2].stable ? UI_KEY_OK : 0U));
#if UI_ENABLE_CUSTOM
    {
        const UI_PageRoute *route = UI_GetRoute(outgoing->page);
        if (route != NULL && route->type == UI_PAGE_CUSTOM) {
            ui_state.transition.leave_custom = 1U;
        }
    }
#endif
    ui_state.dirty = 1U;
}

UI_Status UI_NavigateTo(UI_PageId page, uint32_t now)
{
    const UI_PageRoute *route = UI_GetRoute(page);
    UI_PageState outgoing;

    if (route == NULL) {
        return UI_INVALID_ARGUMENT;
    }
    if (ui_state.transition.active != 0U) {
        return UI_BUSY;
    }
    if (ui_state.stack_count >= UI_NAV_DEPTH) {
        UI_RecordError(UI_NAVIGATION_FULL, ui_state.current.page, page);
        ui_state.reject_flash = 2U;
        ui_state.dirty = 1U;
        return UI_NAVIGATION_FULL;
    }
#if UI_ENABLE_CUSTOM
    if (UI_CurrentPageType() == UI_PAGE_CUSTOM &&
        route->type == UI_PAGE_CUSTOM) {
        return UI_NOT_ALLOWED;
    }
#endif
    outgoing = ui_state.current;
    ui_state.stack[ui_state.stack_count++] = outgoing;
    UI_ResetPageState(&ui_state.current, page);
    ui_state.home_anim.active = 0U;
    ui_state.list_anim.active = 0U;
    ui_start_transition(&outgoing, false, now);
#if UI_ENABLE_CUSTOM
    if (route->type == UI_PAGE_CUSTOM) {
        ui_state.in_callback = 1U;
        UI_CustomOnEnter(page);
        ui_state.in_callback = 0U;
    }
#endif
    return UI_OK;
}

UI_Status UI_NavigateBack(uint32_t now)
{
    UI_PageState outgoing;
    const UI_PageRoute *route;

    if (ui_state.transition.active != 0U) {
        return UI_BUSY;
    }
    if (ui_state.stack_count == 0U) {
        return UI_NOT_ALLOWED;
    }
    outgoing = ui_state.current;
    ui_state.current = ui_state.stack[--ui_state.stack_count];
    if (UI_CurrentPageType() == UI_PAGE_MENU) {
        UI_MenuRepairFocus(&ui_state.current);
    }
    ui_state.home_anim.active = 0U;
    ui_state.list_anim.active = 0U;
    ui_start_transition(&outgoing, true, now);
#if UI_ENABLE_CUSTOM
    route = UI_GetRoute(ui_state.current.page);
    if (route != NULL && route->type == UI_PAGE_CUSTOM) {
        ui_state.in_callback = 1U;
        UI_CustomOnEnter(ui_state.current.page);
        ui_state.in_callback = 0U;
    }
#else
    (void)route;
#endif
    return UI_OK;
}

void UI_AnimateNavigation(uint32_t now)
{
    if (ui_state.transition.active != 0U) {
        uint32_t elapsed = now - ui_state.transition.started;
        ui_state.dirty = 1U;
        if (elapsed >= UI_PAGE_MS) {
            ui_state.transition.active = 0U;
#if UI_ENABLE_CUSTOM
            if (ui_state.transition.leave_custom != 0U) {
                ui_state.in_callback = 1U;
                UI_CustomOnLeave(ui_state.transition.outgoing.page);
                ui_state.in_callback = 0U;
            }
#endif
        }
    }
}

void UI_DrawPage(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width, uint32_t now)
{
    const UI_PageRoute *route = UI_GetRoute(state->page);
    (void)now;

    if (route == NULL || clip_width == 0U) {
        return;
    }
    switch (route->type) {
#if UI_ENABLE_HOME
    case UI_PAGE_HOME:
        UI_HomeDraw(state, x_offset, clip_x, clip_width);
        break;
#endif
#if UI_ENABLE_MENU
    case UI_PAGE_MENU:
        UI_MenuDraw(state, x_offset, clip_x, clip_width);
        break;
#endif
#if UI_ENABLE_INFO
    case UI_PAGE_INFO:
        UI_InfoDraw(state, x_offset, clip_x, clip_width);
        break;
#endif
#if UI_ENABLE_CUSTOM
    case UI_PAGE_CUSTOM:
        /* Custom pages draw through the same transition clip as built-in pages. */
        OLED_SetClipWindow(clip_x, 0, clip_width, UI_SCREEN_HEIGHT);
        ui_state.in_callback = 1U;
        UI_CustomOnDraw(state->page, x_offset, clip_x, clip_width);
        ui_state.in_callback = 0U;
        break;
#endif
    default:
        break;
    }
}

void UI_DrawScene(uint32_t now)
{
    OLED_Clear();
    OLED_ResetClipWindow();
    OLED_SetDrawMode(OLED_DRAW_SET);
    OLED_SetBackgroundMode(OLED_BG_TRANSPARENT);
    OLED_SetFontDirection(OLED_ROTATION_0);
    OLED_SetFontPosition(OLED_FONT_POS_TOP);
    OLED_SetFontRefHeight(OLED_FONT_REF_ALL);

    if (ui_state.transition.active != 0U) {
        uint16_t progress = UI_EaseQ12(now - ui_state.transition.started,
                                         UI_PAGE_MS);
        int16_t boundary;
        int16_t outgoing_x;
        int16_t current_x;

        if (ui_state.transition.reverse == 0U) {
            outgoing_x = (int16_t)(-((int32_t)UI_PAGE_PARALLAX * progress /
                                     UI_Q12_ONE));
            current_x = (int16_t)((int32_t)UI_SCREEN_WIDTH *
                          (UI_Q12_ONE - progress) / UI_Q12_ONE);
            boundary = current_x;
            if (boundary > 0) {
                UI_DrawPage(&ui_state.transition.outgoing, outgoing_x, 0,
                               (uint16_t)boundary, now);
            }
            if (boundary < UI_SCREEN_WIDTH) {
                UI_DrawPage(&ui_state.current, current_x, boundary,
                               (uint16_t)(UI_SCREEN_WIDTH - boundary), now);
            }
            if (boundary >= 0 && boundary < UI_SCREEN_WIDTH) {
                OLED_ResetClipWindow();
                OLED_DrawVLine(boundary, 0, UI_SCREEN_HEIGHT);
            }
        } else {
            outgoing_x = (int16_t)(((int32_t)UI_PAGE_PARALLAX * progress) /
                                   UI_Q12_ONE);
            boundary = (int16_t)(((int32_t)UI_SCREEN_WIDTH * progress) /
                                 UI_Q12_ONE);
            current_x = (int16_t)(boundary - UI_SCREEN_WIDTH);
            if (boundary > 0) {
                UI_DrawPage(&ui_state.current, current_x, 0,
                               (uint16_t)boundary, now);
            }
            if (boundary < UI_SCREEN_WIDTH) {
                UI_DrawPage(&ui_state.transition.outgoing, outgoing_x,
                               boundary,
                               (uint16_t)(UI_SCREEN_WIDTH - boundary), now);
            }
            if (boundary >= 0 && boundary < UI_SCREEN_WIDTH) {
                OLED_ResetClipWindow();
                OLED_DrawVLine(boundary, 0, UI_SCREEN_HEIGHT);
            }
        }
    } else {
        UI_DrawPage(&ui_state.current, 0, 0, UI_SCREEN_WIDTH, now);
    }

    if (ui_state.reject_flash > 1U) {
        OLED_SetDrawMode(OLED_DRAW_XOR);
        OLED_DrawRFrame(1, 1, 126U, 62U, 3U);
        OLED_SetDrawMode(OLED_DRAW_SET);
        ui_state.reject_flash = 1U;
        ui_state.dirty = 1U;
    } else if (ui_state.reject_flash != 0U) {
        ui_state.reject_flash = 0U;
    }
    UI_DialogDraw();
    UI_ToastDraw();
    OLED_ResetClipWindow();
    OLED_SetDrawMode(OLED_DRAW_SET);
}
