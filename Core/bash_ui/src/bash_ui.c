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

bool UI_QueueEvent(UI_EventId event)
{
    uint8_t tail;
    if (event == UI_EVENT_NONE) {
        return true;
    }
    if (ui_state.event_count >= UI_EVENT_QUEUE_LENGTH) {
        return false;
    }
    tail = (uint8_t)((ui_state.event_head + ui_state.event_count) %
                     UI_EVENT_QUEUE_LENGTH);
    ui_state.events[tail] = event;
    ++ui_state.event_count;
    return true;
}

static UI_Status ui_config_error(UI_PageId page, uint16_t index)
{
    UI_RecordError(UI_CONFIG_ERROR, page, index);
    return UI_CONFIG_ERROR;
}

static bool ui_valid_texts(const UI_App *app)
{
    return app->fonts.home_font != NULL && app->fonts.title_font != NULL &&
           app->fonts.body_font != NULL && app->texts.return_text != NULL &&
           app->texts.cancel_text != NULL && app->texts.confirm_text != NULL &&
           app->texts.on_text != NULL && app->texts.off_text != NULL &&
           app->texts.message_title != NULL;
}

static UI_Status ui_validate_route(const UI_App *app,
                                         UI_PageId page)
{
    const UI_PageRoute *route = &app->routes[page - 1U];
    switch (route->type) {
    case UI_PAGE_HOME:
#if UI_ENABLE_HOME
        if (route->index >= app->home_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
#else
        return ui_config_error(page, route->index);
#endif
    case UI_PAGE_MENU:
#if UI_ENABLE_MENU
        if (route->index >= app->menu_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
#else
        return ui_config_error(page, route->index);
#endif
    case UI_PAGE_INFO:
#if UI_ENABLE_INFO
        if (route->index >= app->info_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
#else
        return ui_config_error(page, route->index);
#endif
    case UI_PAGE_CUSTOM:
#if UI_ENABLE_CUSTOM
        if (route->index >= app->custom_page_count) {
            return ui_config_error(page, route->index);
        }
        break;
#else
        return ui_config_error(page, route->index);
#endif
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
    for (i = 0U; i < app->menu_page_count; ++i) {
        const UI_MenuPage *page = &app->menu_pages[i];
        uint16_t j;
        if (page->title == NULL || page->item_count == UINT16_MAX ||
            (page->item_count != 0U && page->items == NULL)) {
            return ui_config_error(0U, i);
        }
        for (j = 0U; j < page->item_count; ++j) {
            const UI_MenuItem *item = &page->items[j];
            if (item->label == NULL) {
                return ui_config_error(0U, j);
            }
            switch (item->type) {
            case UI_MENU_PAGE:
                if (item->ref == UI_PAGE_NONE ||
                    item->ref > app->route_count) {
                    return ui_config_error(0U, j);
                }
                break;
            case UI_MENU_ACTION:
                if (item->ref == UI_EVENT_NONE) {
                    return ui_config_error(0U, j);
                }
                break;
            case UI_MENU_INT:
#if UI_ENABLE_INT_EDITOR
                if (item->ref >= app->int_binding_count) {
                    return ui_config_error(0U, j);
                }
                break;
#else
                return ui_config_error(0U, j);
#endif
            case UI_MENU_BOOL:
#if UI_ENABLE_BOOL_EDITOR
                if (item->ref >= app->bool_binding_count) {
                    return ui_config_error(0U, j);
                }
                break;
#else
                return ui_config_error(0U, j);
#endif
            case UI_MENU_CONFIRM:
#if UI_ENABLE_CONFIRM
                if (item->ref >= app->confirm_desc_count) {
                    return ui_config_error(0U, j);
                }
                break;
#else
                return ui_config_error(0U, j);
#endif
            default:
                return ui_config_error(0U, j);
            }
        }
    }
    for (i = 0U; i < app->info_page_count; ++i) {
        const UI_InfoPage *page = &app->info_pages[i];
        uint16_t j;
        if (page->row_count != 0U && page->rows == NULL) {
            return ui_config_error(0U, i);
        }
        for (j = 0U; j < page->row_count; ++j) {
            if (page->rows[j].name == NULL) {
                return ui_config_error(0U, j);
            }
        }
    }
    return UI_OK;
}

static UI_Status ui_validate_bindings(const UI_App *app)
{
    uint16_t i;
    for (i = 0U; i < app->int_binding_count; ++i) {
        const UI_IntBinding *binding = &app->int_bindings[i];
        if (binding->title == NULL || binding->value == NULL ||
            binding->minimum > binding->maximum || binding->step == 0U ||
            *binding->value < binding->minimum ||
            *binding->value > binding->maximum) {
            return ui_config_error(0U, i);
        }
    }
    for (i = 0U; i < app->bool_binding_count; ++i) {
        if (app->bool_bindings[i].title == NULL ||
            app->bool_bindings[i].value == NULL) {
            return ui_config_error(0U, i);
        }
    }
    for (i = 0U; i < app->confirm_desc_count; ++i) {
        if (app->confirm_descs[i].text == NULL) {
            return ui_config_error(0U, i);
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
        app->root_page > app->route_count || !ui_valid_texts(app)) {
        return ui_config_error(0U, 0U);
    }
    if ((app->home_page_count != 0U && app->home_pages == NULL) ||
        (app->menu_page_count != 0U && app->menu_pages == NULL) ||
        (app->info_page_count != 0U && app->info_pages == NULL) ||
        (app->int_binding_count != 0U && app->int_bindings == NULL) ||
        (app->bool_binding_count != 0U && app->bool_bindings == NULL) ||
        (app->confirm_desc_count != 0U && app->confirm_descs == NULL)) {
        return ui_config_error(0U, 0U);
    }
    for (i = 0U; i < app->route_count; ++i) {
        status = ui_validate_route(app, (UI_PageId)(i + 1U));
        if (status != UI_OK) {
            return status;
        }
    }
    status = ui_validate_pages(app);
    if (status != UI_OK) {
        return status;
    }
    return ui_validate_bindings(app);
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
#if UI_ENABLE_CUSTOM
    if (UI_CurrentPageType() == UI_PAGE_CUSTOM) {
        ui_state.in_callback = 1U;
        UI_CustomOnEnter(ui_state.current.page);
        ui_state.in_callback = 0U;
    }
#endif
    return UI_OK;
}

void UI_Invalidate(void)
{
    if (ui_state.initialized != 0U) {
        ui_state.dirty = 1U;
    }
}

bool UI_PollEvent(UI_EventId *out_event)
{
    if (out_event == NULL || ui_state.event_count == 0U) {
        return false;
    }
    *out_event = ui_state.events[ui_state.event_head];
    ui_state.event_head = (uint8_t)((ui_state.event_head + 1U) %
                                 UI_EVENT_QUEUE_LENGTH);
    --ui_state.event_count;
    return true;
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

UI_Status UI_SetMenuItemState(UI_PageId page_id,
                                    uint16_t item_index,
                                    UI_ItemState state)
{
#if UI_ENABLE_MENU
    const UI_PageRoute *route;
    const UI_MenuPage *page;
    uint8_t shift;
    uint8_t mask;
    if (ui_state.initialized == 0U) {
        return UI_NOT_INITIALIZED;
    }
    if (state > UI_ITEM_RESERVED) {
        return UI_INVALID_ARGUMENT;
    }
    route = UI_GetRoute(page_id);
    if (route == NULL || route->type != UI_PAGE_MENU) {
        return UI_INVALID_ARGUMENT;
    }
    page = &ui_state.app->menu_pages[route->index];
    if (item_index >= page->item_count) {
        return UI_INVALID_ARGUMENT;
    }
    if (page->item_states == NULL) {
        return UI_UNSUPPORTED;
    }
    shift = (uint8_t)((item_index & 3U) * 2U);
    mask = (uint8_t)(3U << shift);
    page->item_states[item_index >> 2U] =
        (uint8_t)((page->item_states[item_index >> 2U] & (uint8_t)~mask) |
                  ((uint8_t)state << shift));
    if (ui_state.current.page == page_id) {
        ui_state.list_anim.active = 0U;
        UI_MenuRepairFocus(&ui_state.current);
    }
    ui_state.dirty = 1U;
    return UI_OK;
#else
    (void)page_id; (void)item_index; (void)state;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_Defer(UI_DeferredType type, uint16_t ref,
                         const char *text, UI_EventId event,
                         uint32_t duration)
{
    if (ui_state.deferred.type != UI_DEFER_NONE) {
        return UI_BUSY;
    }
    ui_state.deferred.type = type;
    ui_state.deferred.ref = ref;
    ui_state.deferred.text = text;
    ui_state.deferred.event = event;
    ui_state.deferred.duration = duration;
    return UI_OK;
}

UI_Status UI_OpenIntEditor(uint16_t binding_index)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
#if UI_ENABLE_INT_EDITOR
    if (binding_index >= ui_state.app->int_binding_count)
        return UI_INVALID_ARGUMENT;
    if (UI_DialogActive()) return UI_BUSY;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_INT, binding_index, NULL, 0U, 0U);
    return UI_DialogOpenInt(binding_index, ui_state.last_update);
#else
    (void)binding_index;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_OpenBoolEditor(uint16_t binding_index)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
#if UI_ENABLE_BOOL_EDITOR
    if (binding_index >= ui_state.app->bool_binding_count)
        return UI_INVALID_ARGUMENT;
    if (UI_DialogActive()) return UI_BUSY;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_BOOL, binding_index, NULL, 0U, 0U);
    return UI_DialogOpenBool(binding_index, ui_state.last_update);
#else
    (void)binding_index;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_OpenConfirm(uint16_t confirm_index)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
#if UI_ENABLE_CONFIRM
    if (confirm_index >= ui_state.app->confirm_desc_count)
        return UI_INVALID_ARGUMENT;
    if (UI_DialogActive()) return UI_BUSY;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_CONFIRM, confirm_index, NULL, 0U, 0U);
    return UI_DialogOpenConfirm(confirm_index, ui_state.last_update);
#else
    (void)confirm_index;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_ShowMessage(const char *text, UI_EventId event)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
    if (text == NULL) return UI_INVALID_ARGUMENT;
#if UI_ENABLE_MESSAGE
    if (UI_DialogActive()) return UI_BUSY;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_MESSAGE, 0U, text, event, 0U);
    return UI_DialogOpenMessage(text, event, ui_state.last_update);
#else
    (void)event;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_ShowToast(const char *text, uint32_t duration_ms)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
    if (text == NULL) return UI_INVALID_ARGUMENT;
#if UI_ENABLE_TOAST
    if (UI_DialogActive()) return UI_BUSY;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_TOAST, 0U, text, 0U, duration_ms);
    return UI_ToastOpen(text, duration_ms, ui_state.last_update);
#else
    (void)duration_ms;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_CustomRequestClose(void)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
    if (UI_CurrentPageType() != UI_PAGE_CUSTOM) return UI_NOT_ALLOWED;
    if (ui_state.transition.active != 0U || UI_DialogActive()) return UI_BUSY;
    if (ui_state.stack_count == 0U) return UI_NOT_ALLOWED;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_CLOSE, 0U, NULL, 0U, 0U);
    return UI_NavigateBack(ui_state.last_update);
}

UI_Status UI_CustomFinish(UI_EventId event)
{
    if (ui_state.initialized == 0U) return UI_NOT_INITIALIZED;
    if (event == UI_EVENT_NONE) return UI_INVALID_ARGUMENT;
    if (UI_CurrentPageType() != UI_PAGE_CUSTOM) return UI_NOT_ALLOWED;
    if (ui_state.transition.active != 0U || UI_DialogActive()) return UI_BUSY;
    if (ui_state.stack_count == 0U) return UI_NOT_ALLOWED;
    if (ui_state.in_callback != 0U)
        return UI_Defer(UI_DEFER_FINISH, 0U, NULL, event, 0U);
    if (!UI_QueueEvent(event)) return UI_QUEUE_FULL;
    return UI_NavigateBack(ui_state.last_update);
}

void UI_ApplyDeferred(uint32_t now)
{
    UI_Deferred request = ui_state.deferred;
    ui_state.deferred.type = UI_DEFER_NONE;
    switch (request.type) {
    case UI_DEFER_CLOSE:
        if (UI_CurrentPageType() == UI_PAGE_CUSTOM &&
            ui_state.stack_count != 0U && ui_state.transition.active == 0U &&
            !UI_DialogActive()) {
            (void)UI_NavigateBack(now);
        }
        break;
    case UI_DEFER_FINISH:
        if (UI_CurrentPageType() != UI_PAGE_CUSTOM ||
            ui_state.stack_count == 0U || ui_state.transition.active != 0U ||
            UI_DialogActive()) {
            UI_RecordError(UI_NOT_ALLOWED, ui_state.current.page,
                              request.event);
        } else if (UI_QueueEvent(request.event)) {
            (void)UI_NavigateBack(now);
        } else {
            UI_RecordError(UI_QUEUE_FULL, ui_state.current.page,
                              request.event);
            ui_state.reject_flash = 2U;
            ui_state.dirty = 1U;
        }
        break;
    case UI_DEFER_INT:
        (void)UI_DialogOpenInt(request.ref, now);
        break;
    case UI_DEFER_BOOL:
        (void)UI_DialogOpenBool(request.ref, now);
        break;
    case UI_DEFER_CONFIRM:
        (void)UI_DialogOpenConfirm(request.ref, now);
        break;
    case UI_DEFER_MESSAGE:
        (void)UI_DialogOpenMessage(request.text, request.event, now);
        break;
    case UI_DEFER_TOAST:
        (void)UI_ToastOpen(request.text, request.duration, now);
        break;
    default:
        break;
    }
}

void UI_DispatchInput(UI_InputEvent event, uint32_t now)
{
    if (UI_DialogActive()) {
        UI_DialogInput(event, now);
        return;
    }
    switch (UI_CurrentPageType()) {
    case UI_PAGE_HOME:
        UI_HomeInput(event, now);
        break;
    case UI_PAGE_MENU:
        UI_MenuInput(event, now);
        break;
    case UI_PAGE_INFO:
        UI_InfoInput(event, now);
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
    UI_MenuAnimate(now);
    UI_InfoAnimate(now);
    UI_DialogAnimate(now);
    ui_process_input(now, input);
    UI_ApplyDeferred(now);
    UI_CustomTick(now);
    UI_ApplyDeferred(now);

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
