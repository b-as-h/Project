#include "bash_ui_internal.h"

#include <string.h>

UI_Runtime ui_state;

void UI_RecordError(UI_Status code, UI_PageId page, uint16_t index)
{
    ui_state.error.code = code;
    ui_state.error.page = page;
    ui_state.error.index = index;
    ui_state.error_valid = 1U;
}

static UI_Status ui_config_error(UI_PageId page, uint16_t index)
{
    UI_RecordError(UI_CONFIG_ERROR, page, index);
    return UI_CONFIG_ERROR;
}

static UI_Status ui_validate_route(const UI_App *app,
                                         UI_PageId page)
{
    const UI_PageRoute *route = &app->routes[page - 1U];
    switch (route->type) {
    case UI_PAGE_HOME:
        if (route->index >= app->home_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
    case UI_PAGE_CUSTOM:
        if (route->index >= app->custom_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
    default:
        return ui_config_error(page, route->index);
    }
    return UI_OK;
}

static UI_Status ui_validate_pages(const UI_App *app)
{
    uint16_t i;
    for (i = 0U; i < app->home_page_count; ++i) {
        const UI_HomePage *page = &app->home_pages[i];
        uint16_t j;
        if (page->items == NULL || page->item_count == 0U) {
            return ui_config_error(0U, i);
        }
        for (j = 0U; j < page->item_count; ++j) {
            if (page->items[j].label == NULL ||
                page->items[j].icon_xbm_32x32 == NULL ||
                page->items[j].target_page == UI_PAGE_NONE ||
                page->items[j].target_page > app->route_count) {
                return ui_config_error(0U, j);
            }
        }
    }
    return UI_OK;
}

static UI_Status ui_validate(const UI_App *app)
{
    uint16_t i;
    UI_Status status;
    if (app == NULL || app->routes == NULL || app->route_count == 0U ||
        app->root_page == UI_PAGE_NONE ||
        app->root_page > app->route_count) {
        return ui_config_error(0U, 0U);
    }
    if (app->home_page_count != 0U && app->home_pages == NULL) {
        return ui_config_error(0U, 0U);
    }
    for (i = 0U; i < app->route_count; ++i) {
        status = ui_validate_route(app, (UI_PageId)(i + 1U));
        if (status != UI_OK) {
            return status;
        }
    }
    return ui_validate_pages(app);
}

UI_Status UI_Init(const UI_App *app)
{
    UI_Status status;
    memset(&ui_state, 0, sizeof(ui_state));
    ui_state.app = app;
    status = ui_validate(app);
    if (status != UI_OK) {
        return status;
    }
    if (OLED_GetWidth() != UI_SCREEN_WIDTH ||
        OLED_GetHeight() != UI_SCREEN_HEIGHT) {
        return ui_config_error(0U, 0U);
    }
    UI_ResetPageState(&ui_state.current, app->root_page);
    ui_state.initialized = 1U;
    ui_state.dirty = 1U;
    if (UI_CurrentPageType() == UI_PAGE_CUSTOM) {
        ui_state.in_callback = 1U;
        UI_CustomOnEnter(ui_state.current.page);
        ui_state.in_callback = 0U;
    }
    return UI_OK;
}

void UI_Invalidate(void)
{
    if (ui_state.initialized != 0U) {
        ui_state.dirty = 1U;
    }
}

bool UI_PollError(UI_ErrorInfo *out_error)
{
    if (out_error == NULL || ui_state.error_valid == 0U) {
        return false;
    }
    *out_error = ui_state.error;
    ui_state.error_valid = 0U;
    return true;
}

UI_Status UI_CustomRequestClose(void)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
    if (UI_CurrentPageType() != UI_PAGE_CUSTOM) return UI_NOT_ALLOWED;
    if (ui_state.transition.active != 0U) return UI_BUSY;
    if (ui_state.stack_count == 0U) return UI_NOT_ALLOWED;
    if (ui_state.in_callback != 0U) {
        /* 回调里发出的关闭请求，等回调返回后再执行。 */
        ui_state.close_pending = 1U;
        return UI_OK;
    }
    return UI_NavigateBack(ui_state.last_update);
}

static void ui_apply_close(uint32_t now)
{
    if (ui_state.close_pending == 0U) {
        return;
    }
    ui_state.close_pending = 0U;
    if (UI_CurrentPageType() == UI_PAGE_CUSTOM &&
        ui_state.stack_count != 0U && ui_state.transition.active == 0U) {
        (void)UI_NavigateBack(now);
    }
}

void UI_DispatchInput(UI_InputEvent event, uint32_t now)
{
    switch (UI_CurrentPageType()) {
    case UI_PAGE_HOME:
        UI_HomeInput(event, now);
        break;
    case UI_PAGE_CUSTOM:
        UI_CustomInput(event);
        break;
    default:
        break;
    }
}

static uint32_t ui_repeat_interval(uint32_t held)
{
    if (held >= 2000U) return 100U;
    if (held >= 1000U) return 160U;
    return 240U;
}

static uint32_t ui_initial_repeat_delay(uint8_t key_index)
{
    return key_index == 2U ? UI_KEY_OK_LONG_PRESS_MS : UI_KEY_REPEAT_DELAY_MS;
}

static void ui_process_input(uint32_t now, UI_Input input)
{
    uint8_t press = 0U;
    uint8_t repeat = 0U;
    uint8_t release = 0U;
    uint8_t stable = 0U;
    uint8_t i;
    UI_InputEvent event;

    input.keys &= (uint8_t)(UI_KEY_UP | UI_KEY_DOWN | UI_KEY_OK);
    if (ui_state.keys_initialized == 0U) {
        for (i = 0U; i < 3U; ++i) {
            uint8_t down = (input.keys & (1U << i)) != 0U;
            ui_state.keys[i].changed_at = now;
            ui_state.keys[i].pressed_at = now;
            ui_state.keys[i].repeat_at = now;
            ui_state.keys[i].candidate = down;
            ui_state.keys[i].stable = down;
            ui_state.keys[i].armed = (uint8_t)!down;
        }
        ui_state.keys_initialized = 1U;
    } else {
        for (i = 0U; i < 3U; ++i) {
            UI_KeyState *key = &ui_state.keys[i];
            uint8_t down = (input.keys & (1U << i)) != 0U;
            if (down != key->candidate) {
                key->candidate = down;
                key->changed_at = now;
            }
            if (key->candidate != key->stable &&
                now - key->changed_at >= UI_KEY_DEBOUNCE_MS) {
                key->stable = key->candidate;
                if (key->stable == 0U) {
                    key->armed = 1U;
                    /* A long-press repeat already consumed this OK press. */
                    if (i != 2U || key->repeat_sent == 0U) {
                        release |= (uint8_t)(1U << i);
                    }
                    key->repeat_sent = 0U;
                } else if (key->armed != 0U) {
                    key->pressed_at = now;
                    key->repeat_at = now + ui_initial_repeat_delay(i);
                    key->repeat_sent = 0U;
                    press |= (uint8_t)(1U << i);
                }
            }
            if (key->stable != 0U && key->candidate != 0U &&
                key->armed != 0U &&
                (int32_t)(now - key->repeat_at) >= 0) {
                repeat |= (uint8_t)(1U << i);
                if (i == 2U) {
                    key->repeat_sent = 1U;
                }
                key->repeat_at = now +
                    ui_repeat_interval(now - key->pressed_at);
            }
        }
    }
    for (i = 0U; i < 3U; ++i) {
        if (ui_state.keys[i].stable != 0U) stable |= (uint8_t)(1U << i);
    }
    ui_state.blocked_keys &= stable;
    if (ui_state.transition.active != 0U) {
        ui_state.blocked_keys |= stable;
        return;
    }
    press &= (uint8_t)~ui_state.blocked_keys;
    repeat &= (uint8_t)~ui_state.blocked_keys;
    release &= (uint8_t)~ui_state.blocked_keys;

    event.steps = 1U;
    if (((press | repeat) & UI_KEY_OK) != 0U) {
        event.action = UI_INPUT_OK;
        event.source = (repeat & UI_KEY_OK) != 0U ?
                       UI_INPUT_REPEAT : UI_INPUT_PRESS;
        UI_DispatchInput(event, now);
        return;
    }
    if ((release & UI_KEY_OK) != 0U) {
        event.action = UI_INPUT_OK;
        event.source = UI_INPUT_RELEASE;
        UI_DispatchInput(event, now);
        return;
    }
    if (input.encoder_delta != 0) {
        int32_t delta = input.encoder_delta;
        event.action = delta < 0 ? UI_INPUT_UP : UI_INPUT_DOWN;
        event.source = UI_INPUT_ENCODER;
        event.steps = (uint16_t)(delta < 0 ? -delta : delta);
        UI_DispatchInput(event, now);
        return;
    }
    if ((stable & (UI_KEY_UP | UI_KEY_DOWN)) ==
        (UI_KEY_UP | UI_KEY_DOWN)) {
        return;
    }
    if (((press | repeat) & UI_KEY_UP) != 0U) {
        event.action = UI_INPUT_UP;
        event.source = (repeat & UI_KEY_UP) != 0U ?
                       UI_INPUT_REPEAT : UI_INPUT_PRESS;
        UI_DispatchInput(event, now);
    } else if (((press | repeat) & UI_KEY_DOWN) != 0U) {
        event.action = UI_INPUT_DOWN;
        event.source = (repeat & UI_KEY_DOWN) != 0U ?
                       UI_INPUT_REPEAT : UI_INPUT_PRESS;
        UI_DispatchInput(event, now);
    }
}

static UI_Status ui_latch_display(UI_Status status)
{
    ui_state.display_fault = 1U;
    ui_state.frame_ready = 0U;
    UI_RecordError(status, ui_state.current.page, 0U);
    return status;
}

static UI_Status ui_check_transfer(uint32_t now)
{
    if (ui_state.transfer_active == 0U) {
        return UI_OK;
    }
    if (OLED_IsBusy()) {
#if UI_DISPLAY_TIMEOUT_MS > 0U
        if (now - ui_state.transfer_started >= UI_DISPLAY_TIMEOUT_MS) {
            return ui_latch_display(UI_DISPLAY_TIMEOUT);
        }
#endif
        return UI_OK;
    }
    ui_state.transfer_active = 0U;
    if (OLED_GetLastStatus() != OLED_OK) {
        return ui_latch_display(UI_DISPLAY_ERROR);
    }
    return UI_OK;
}

static UI_Status ui_submit(uint32_t now)
{
    OLED_Status oled_status;
#if UI_REFRESH_MODE == UI_REFRESH_BLOCKING
    oled_status = OLED_Update();
#elif UI_REFRESH_MODE == UI_REFRESH_IT
    oled_status = OLED_UpdateIT();
#elif UI_REFRESH_MODE == UI_REFRESH_DMA
    oled_status = OLED_UpdateDMA();
#else
#error "Invalid UI_REFRESH_MODE"
#endif
    if (oled_status == OLED_BUSY) {
        return UI_OK;
    }
    if (oled_status != OLED_OK) {
        return ui_latch_display(UI_DISPLAY_ERROR);
    }
    ui_state.frame_ready = 0U;
#if UI_REFRESH_MODE != UI_REFRESH_BLOCKING
    ui_state.transfer_active = 1U;
    ui_state.transfer_started = now;
#else
    (void)now;
#endif
    return UI_OK;
}

UI_Status UI_Update(uint32_t now, UI_Input input)
{
    UI_Status status;
    if (ui_state.initialized == 0U) {
        return UI_NOT_INITIALIZED;
    }
    if (ui_state.keys_initialized == 0U) {
        ui_state.next_frame = now;
    }
    ui_state.last_update = now;
    status = ui_check_transfer(now);
    if (status != UI_OK || ui_state.display_fault != 0U) {
        return status != UI_OK ? status : UI_DISPLAY_ERROR;
    }

    UI_AnimateNavigation(now);
    UI_HomeAnimate(now);
    ui_process_input(now, input);
    ui_apply_close(now);
    UI_CustomTick(now);
    ui_apply_close(now);

    if (ui_state.dirty != 0U && (int32_t)(now - ui_state.next_frame) >= 0) {
        uint32_t missed = (now - ui_state.next_frame) / UI_FRAME_INTERVAL_MS;
        ui_state.next_frame += (missed + 1U) * UI_FRAME_INTERVAL_MS;
        ui_state.dirty = 0U;
        UI_DrawScene(now);
        ui_state.frame_ready = 1U;
    }
    if (ui_state.frame_ready != 0U && !OLED_IsBusy()) {
        status = ui_submit(now);
        if (status != UI_OK) {
            return status;
        }
    }
    return UI_OK;
}

UI_Status UI_RecoverDisplay(void)
{
    if (ui_state.initialized == 0U) {
        return UI_NOT_INITIALIZED;
    }
    if (OLED_IsBusy()) {
        return UI_BUSY;
    }
    /*
     * 异步完成失败后 BASH_OLED 会保留错误状态，并把下一次正常刷新标记为
     * 强制全屏。恢复入口的职责正是允许这次重画与重发，因此不能要求
     * OLED_GetLastStatus() 已经先变回 OLED_OK，否则错误将无法恢复。
     */
    ui_state.display_fault = 0U;
    ui_state.transfer_active = 0U;
    ui_state.frame_ready = 0U;
    ui_state.dirty = 1U;
    ui_state.next_frame = ui_state.last_update;
    ui_state.blocked_keys |= (uint8_t)(
        (ui_state.keys[0].stable ? UI_KEY_UP : 0U) |
        (ui_state.keys[1].stable ? UI_KEY_DOWN : 0U) |
        (ui_state.keys[2].stable ? UI_KEY_OK : 0U));
    return UI_OK;
}
