#include "bash_ui_internal.h"
#include "bash_ui_draw.h"

#include <limits.h>

#define UI_MODAL_ENABLED (UI_ENABLE_INT_EDITOR || \
                             UI_ENABLE_BOOL_EDITOR || \
                             UI_ENABLE_CONFIRM || \
                             UI_ENABLE_MESSAGE)

void UI_DrawCentered(int16_t x, int16_t y, uint16_t width,
                        const char *text)
{
    int16_t text_x;
    if (text == NULL) {
        return;
    }
    text_x = (int16_t)(x + ((int32_t)width - OLED_GetUTF8Width(text)) / 2);
    OLED_DrawUTF8(text_x, y, text);
}

void UI_FormatInt(int32_t value, const char *unit, char *buffer,
                     size_t capacity)
{
    char reverse[11];
    uint32_t magnitude;
    size_t digits = 0U;
    size_t out = 0U;

    if (buffer == NULL || capacity == 0U) {
        return;
    }
    if (value < 0) {
        magnitude = (uint32_t)(-(value + 1)) + 1U;
        if (out + 1U < capacity) {
            buffer[out++] = '-';
        }
    } else {
        magnitude = (uint32_t)value;
    }
    do {
        reverse[digits++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U && digits < sizeof(reverse));
    while (digits != 0U && out + 1U < capacity) {
        buffer[out++] = reverse[--digits];
    }
    if (unit != NULL) {
        size_t i = 0U;
        while (unit[i] != '\0' && out + 1U < capacity) {
            buffer[out++] = unit[i++];
        }
    }
    buffer[out] = '\0';
}

bool UI_DialogActive(void)
{
#if UI_MODAL_ENABLED
    return ui_state.overlay.type != UI_OVERLAY_NONE;
#else
    return false;
#endif
}

#if UI_MODAL_ENABLED
static int16_t ui_dialog_origin(void)
{
    if (UI_CurrentPageType() == UI_PAGE_MENU) {
        return (int16_t)(UI_MENU_HEADER_HEIGHT +
                         UI_RoundQ8(ui_state.current.focus_q8));
    }
    return 24;
}
#endif

#if UI_MODAL_ENABLED
static void ui_dialog_focus_rect(uint8_t focus, int16_t *x, int16_t *y,
                                    int16_t *width, int16_t *height)
{
    if (ui_state.overlay.type == UI_OVERLAY_MESSAGE) {
        *x = 46; *y = 41; *width = 36; *height = 16;
    } else if (focus == UI_FOCUS_VALUE) {
        *x = ui_state.overlay.type == UI_OVERLAY_BOOL ? 29 : 39;
        *y = 21;
        *width = ui_state.overlay.type == UI_OVERLAY_BOOL ? 70 : 46;
        *height = 16;
    } else if (focus == UI_FOCUS_CONFIRM) {
        *x = 77; *y = 41; *width = 36; *height = 16;
    } else {
        *x = 15; *y = 41; *width = 36; *height = 16;
    }
}

static void ui_dialog_current_focus(uint32_t now, int32_t *x, int32_t *y,
                                       int32_t *width, int32_t *height)
{
    if (ui_state.overlay.focus_moving != 0U) {
        uint16_t p = UI_EaseQ12(now - ui_state.overlay.focus_started,
                                   UI_FOCUS_MS);
        *x = UI_LerpQ12(ui_state.overlay.focus_from_x_q8,
                          ui_state.overlay.focus_x_q8, p);
        *y = UI_LerpQ12(ui_state.overlay.focus_from_y_q8,
                          ui_state.overlay.focus_y_q8, p);
        *width = UI_LerpQ12(ui_state.overlay.focus_from_w_q8,
                              ui_state.overlay.focus_w_q8, p);
        *height = UI_LerpQ12(ui_state.overlay.focus_from_h_q8,
                               ui_state.overlay.focus_h_q8, p);
    } else {
        *x = ui_state.overlay.focus_x_q8;
        *y = ui_state.overlay.focus_y_q8;
        *width = ui_state.overlay.focus_w_q8;
        *height = ui_state.overlay.focus_h_q8;
    }
}

static void ui_dialog_set_focus(uint8_t focus, uint32_t now)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    ui_dialog_current_focus(now, &ui_state.overlay.focus_from_x_q8,
                               &ui_state.overlay.focus_from_y_q8,
                               &ui_state.overlay.focus_from_w_q8,
                               &ui_state.overlay.focus_from_h_q8);
    ui_state.overlay.focus = focus;
    ui_dialog_focus_rect(focus, &x, &y, &width, &height);
    ui_state.overlay.focus_x_q8 = (int32_t)x * 256;
    ui_state.overlay.focus_y_q8 = (int32_t)y * 256;
    ui_state.overlay.focus_w_q8 = (int32_t)width * 256;
    ui_state.overlay.focus_h_q8 = (int32_t)height * 256;
    ui_state.overlay.focus_started = now;
    ui_state.overlay.focus_moving = 1U;
    ui_state.dirty = 1U;
}

static UI_Status ui_dialog_begin(UI_OverlayType type, uint16_t ref,
                                       const char *text,
                                       UI_EventId event, uint32_t now)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    if (UI_DialogActive()) {
        return UI_BUSY;
    }
    ui_state.toast.visible = 0U;
    ui_state.overlay.type = type;
    ui_state.overlay.ref = ref;
    ui_state.overlay.text = text;
    ui_state.overlay.event = event;
    ui_state.overlay.origin_y = ui_dialog_origin();
    ui_state.overlay.phase_started = now;
    ui_state.overlay.phase_q12 = 0U;
    ui_state.overlay.phase_from_q12 = 0U;
    ui_state.overlay.value_q12 = UI_Q12_ONE;
    ui_state.overlay.closing = 0U;
    ui_state.overlay.focus_moving = 0U;
    ui_state.overlay.value_moving = 0U;
    if (type == UI_OVERLAY_CONFIRM) {
        ui_state.overlay.focus = UI_FOCUS_CANCEL;
        ui_state.overlay.editing = 0U;
    } else if (type == UI_OVERLAY_MESSAGE) {
        ui_state.overlay.focus = UI_FOCUS_CONFIRM;
        ui_state.overlay.editing = 0U;
    } else {
        ui_state.overlay.focus = UI_FOCUS_VALUE;
        ui_state.overlay.editing = 1U;
    }
    ui_dialog_focus_rect(ui_state.overlay.focus, &x, &y, &width, &height);
    ui_state.overlay.focus_x_q8 = ui_state.overlay.focus_from_x_q8 =
        (int32_t)x * 256;
    ui_state.overlay.focus_y_q8 = ui_state.overlay.focus_from_y_q8 =
        (int32_t)y * 256;
    ui_state.overlay.focus_w_q8 = ui_state.overlay.focus_from_w_q8 =
        (int32_t)width * 256;
    ui_state.overlay.focus_h_q8 = ui_state.overlay.focus_from_h_q8 =
        (int32_t)height * 256;
    ui_state.dirty = 1U;
    return UI_OK;
}
#endif

UI_Status UI_DialogOpenInt(uint16_t index, uint32_t now)
{
#if UI_ENABLE_INT_EDITOR
    UI_Status status;
    if (index >= ui_state.app->int_binding_count) {
        return UI_INVALID_ARGUMENT;
    }
    status = ui_dialog_begin(UI_OVERLAY_INT, index, NULL,
                                UI_EVENT_NONE, now);
    if (status == UI_OK) {
        ui_state.overlay.draft = *ui_state.app->int_bindings[index].value;
        ui_state.overlay.original = ui_state.overlay.draft;
        ui_state.overlay.previous = ui_state.overlay.draft;
    }
    return status;
#else
    (void)index; (void)now;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_DialogOpenBool(uint16_t index, uint32_t now)
{
#if UI_ENABLE_BOOL_EDITOR
    UI_Status status;
    if (index >= ui_state.app->bool_binding_count) {
        return UI_INVALID_ARGUMENT;
    }
    status = ui_dialog_begin(UI_OVERLAY_BOOL, index, NULL,
                                UI_EVENT_NONE, now);
    if (status == UI_OK) {
        ui_state.overlay.draft = *ui_state.app->bool_bindings[index].value ? 1 : 0;
        ui_state.overlay.original = ui_state.overlay.draft;
        ui_state.overlay.previous = ui_state.overlay.draft;
    }
    return status;
#else
    (void)index; (void)now;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_DialogOpenConfirm(uint16_t index, uint32_t now)
{
#if UI_ENABLE_CONFIRM
    if (index >= ui_state.app->confirm_desc_count) {
        return UI_INVALID_ARGUMENT;
    }
    return ui_dialog_begin(UI_OVERLAY_CONFIRM, index,
                              ui_state.app->confirm_descs[index].text,
                              UI_EVENT_NONE, now);
#else
    (void)index; (void)now;
    return UI_UNSUPPORTED;
#endif
}

UI_Status UI_DialogOpenMessage(const char *text, UI_EventId event,
                                     uint32_t now)
{
#if UI_ENABLE_MESSAGE
    if (text == NULL) {
        return UI_INVALID_ARGUMENT;
    }
    return ui_dialog_begin(UI_OVERLAY_MESSAGE, 0U, text, event, now);
#else
    (void)text; (void)event; (void)now;
    return UI_UNSUPPORTED;
#endif
}

#if UI_MODAL_ENABLED
static void ui_dialog_close(uint32_t now)
{
    ui_state.overlay.phase_from_q12 = ui_state.overlay.phase_q12;
    ui_state.overlay.phase_started = now;
    ui_state.overlay.closing = 1U;
    ui_state.overlay.editing = 0U;
    ui_state.dirty = 1U;
}

static bool ui_dialog_event_or_reject(UI_EventId event)
{
    if (event == UI_EVENT_NONE || UI_QueueEvent(event)) {
        return true;
    }
    UI_RecordError(UI_QUEUE_FULL, ui_state.current.page, event);
    ui_state.reject_flash = 2U;
    ui_state.dirty = 1U;
    return false;
}

static int32_t ui_int_adjust(const UI_IntBinding *binding,
                                int32_t value, bool increase,
                                uint16_t steps)
{
    int64_t next = value;
    int64_t amount = (int64_t)binding->step * steps;
    if (increase) {
        next += amount;
        return next > binding->maximum ? binding->maximum : (int32_t)next;
    }
    next -= amount;
    return next < binding->minimum ? binding->minimum : (int32_t)next;
}

static void ui_dialog_change_value(UI_InputEvent event, uint32_t now)
{
    int32_t next = ui_state.overlay.draft;
    bool increase = event.action == UI_INPUT_UP;

    if (ui_state.overlay.type == UI_OVERLAY_INT) {
        const UI_IntBinding *binding =
            &ui_state.app->int_bindings[ui_state.overlay.ref];
        next = ui_int_adjust(binding, next, increase, event.steps);
    } else if (ui_state.overlay.type == UI_OVERLAY_BOOL) {
        /* A switch has no directional meaning: either direction toggles it.
         * Ignore key auto-repeat so one long press cannot oscillate the value.
         * Batched encoder detents are equivalent to that many toggles. */
        if (event.source == UI_INPUT_REPEAT || (event.steps & 1U) == 0U) {
            return;
        }
        next = ui_state.overlay.draft == 0 ? 1 : 0;
        increase = next != 0;
    }
    if (next != ui_state.overlay.draft) {
        ui_state.overlay.previous = ui_state.overlay.draft;
        ui_state.overlay.draft = next;
        ui_state.overlay.value_direction = increase ? 1 : -1;
        ui_state.overlay.value_q12 = 0U;
        ui_state.overlay.value_started = now;
        ui_state.overlay.value_moving = 1U;
        ui_state.dirty = 1U;
    }
}

static void ui_dialog_commit(uint32_t now)
{
    UI_EventId event = UI_EVENT_NONE;
    if (ui_state.overlay.type == UI_OVERLAY_INT) {
        const UI_IntBinding *binding =
            &ui_state.app->int_bindings[ui_state.overlay.ref];
        if (ui_state.overlay.draft != ui_state.overlay.original) {
            event = binding->changed_event;
            if (!ui_dialog_event_or_reject(event)) {
                return;
            }
            *binding->value = ui_state.overlay.draft;
        }
    } else if (ui_state.overlay.type == UI_OVERLAY_BOOL) {
        const UI_BoolBinding *binding =
            &ui_state.app->bool_bindings[ui_state.overlay.ref];
        if (ui_state.overlay.draft != ui_state.overlay.original) {
            event = binding->changed_event;
            if (!ui_dialog_event_or_reject(event)) {
                return;
            }
            *binding->value = ui_state.overlay.draft != 0;
        }
    }
    ui_dialog_close(now);
}

static void ui_dialog_confirm_action(uint32_t now)
{
    const UI_ConfirmDesc *desc =
        &ui_state.app->confirm_descs[ui_state.overlay.ref];
    UI_EventId event = ui_state.overlay.focus == UI_FOCUS_CONFIRM ?
                          desc->confirmed_event : desc->cancelled_event;
    if (ui_dialog_event_or_reject(event)) {
        ui_dialog_close(now);
    }
}

void UI_DialogInput(UI_InputEvent event, uint32_t now)
{
    int16_t next;

    if (!UI_DialogActive() || ui_state.overlay.closing != 0U) {
        return;
    }
    if (ui_state.overlay.type == UI_OVERLAY_MESSAGE) {
        if (event.action == UI_INPUT_OK &&
            ui_dialog_event_or_reject(ui_state.overlay.event)) {
            ui_dialog_close(now);
        }
        return;
    }
    if (ui_state.overlay.type == UI_OVERLAY_CONFIRM) {
        if (event.action == UI_INPUT_OK) {
            ui_dialog_confirm_action(now);
        } else {
            ui_dialog_set_focus(
                ui_state.overlay.focus == UI_FOCUS_CANCEL ?
                UI_FOCUS_CONFIRM : UI_FOCUS_CANCEL, now);
        }
        return;
    }
    if (ui_state.overlay.editing != 0U) {
        if (event.action == UI_INPUT_OK) {
            ui_state.overlay.editing = 0U;
            ui_dialog_set_focus(UI_FOCUS_CONFIRM, now);
        } else {
            ui_dialog_change_value(event, now);
        }
        return;
    }
    if (event.action == UI_INPUT_OK) {
        if (ui_state.overlay.focus == UI_FOCUS_VALUE) {
            ui_state.overlay.editing = 1U;
            ui_state.dirty = 1U;
        } else if (ui_state.overlay.focus == UI_FOCUS_CONFIRM) {
            ui_dialog_commit(now);
        } else {
            ui_dialog_close(now);
        }
        return;
    }
    next = (int16_t)ui_state.overlay.focus +
           (event.action == UI_INPUT_UP ? -1 : 1);
    if (next < UI_FOCUS_VALUE) {
        next = UI_FOCUS_CANCEL;
    } else if (next > UI_FOCUS_CANCEL) {
        next = UI_FOCUS_VALUE;
    }
    ui_dialog_set_focus((uint8_t)next, now);
}

#else

void UI_DialogInput(UI_InputEvent event, uint32_t now)
{
    (void)event;
    (void)now;
}

#endif

static void ui_toast_animate(uint32_t now)
{
#if UI_ENABLE_TOAST
    if (ui_state.toast.visible == 0U) {
        return;
    }
    if (ui_state.toast.closing == 0U &&
        (int32_t)(now - ui_state.toast.expires_at) >= 0) {
        ui_state.toast.closing = 1U;
        ui_state.toast.shown_at = now;
    }
    if (ui_state.toast.closing != 0U) {
        uint16_t p = UI_EaseQ12(now - ui_state.toast.shown_at, UI_TOAST_MS);
        ui_state.toast.phase_q12 = (uint16_t)(UI_Q12_ONE - p);
        ui_state.dirty = 1U;
        if (p == UI_Q12_ONE) {
            ui_state.toast.visible = 0U;
        }
    } else if (ui_state.toast.phase_q12 < UI_Q12_ONE) {
        ui_state.toast.phase_q12 = UI_EaseQ12(now - ui_state.toast.shown_at,
                                              UI_TOAST_MS);
        ui_state.dirty = 1U;
    }
#else
    (void)now;
#endif
}

void UI_DialogAnimate(uint32_t now)
{
#if UI_MODAL_ENABLED
    if (UI_DialogActive()) {
        uint32_t elapsed = now - ui_state.overlay.phase_started;
        bool redraw = false;
        if (ui_state.overlay.closing != 0U) {
            uint16_t p = UI_EaseQ12(elapsed, UI_DIALOG_CLOSE_MS);
            ui_state.overlay.phase_q12 = (uint16_t)UI_LerpQ12(
                ui_state.overlay.phase_from_q12, 0, p);
            redraw = true;
            if (p == UI_Q12_ONE) {
                ui_state.overlay.type = UI_OVERLAY_NONE;
                ui_state.overlay.closing = 0U;
            }
        } else if (ui_state.overlay.phase_q12 < UI_Q12_ONE) {
            ui_state.overlay.phase_q12 = UI_EaseQ12(elapsed,
                                                    UI_DIALOG_OPEN_MS);
            redraw = true;
        }
        if (ui_state.overlay.focus_moving != 0U) {
            redraw = true;
            if (now - ui_state.overlay.focus_started >= UI_FOCUS_MS) {
                ui_state.overlay.focus_moving = 0U;
            }
        }
        if (ui_state.overlay.value_moving != 0U) {
            uint32_t duration = ui_state.overlay.type == UI_OVERLAY_BOOL ?
                                UI_BOOL_VALUE_MS : UI_INT_VALUE_MS;
            redraw = true;
            ui_state.overlay.value_q12 = UI_EaseQ12(
                now - ui_state.overlay.value_started, duration);
            if (ui_state.overlay.value_q12 == UI_Q12_ONE) {
                ui_state.overlay.value_moving = 0U;
            }
        }
        if (redraw) {
            ui_state.dirty = 1U;
        }
    }
#endif
    ui_toast_animate(now);
}

UI_Status UI_ToastOpen(const char *text, uint32_t duration,
                             uint32_t now)
{
#if UI_ENABLE_TOAST
    if (text == NULL) {
        return UI_INVALID_ARGUMENT;
    }
    if (UI_DialogActive()) {
        return UI_BUSY;
    }
    if (duration == 0U) {
        duration = UI_TOAST_DEFAULT_MS;
    }
    if (duration > (uint32_t)INT32_MAX) {
        duration = (uint32_t)INT32_MAX;
    }
    ui_state.toast.text = text;
    ui_state.toast.shown_at = now;
    ui_state.toast.expires_at = now + duration;
    ui_state.toast.phase_q12 = 0U;
    ui_state.toast.visible = 1U;
    ui_state.toast.closing = 0U;
    ui_state.dirty = 1U;
    return UI_OK;
#else
    (void)text; (void)duration; (void)now;
    return UI_UNSUPPORTED;
#endif
}

#if UI_MODAL_ENABLED
static const char *ui_dialog_title(void)
{
    if (ui_state.overlay.type == UI_OVERLAY_INT) {
        return ui_state.app->int_bindings[ui_state.overlay.ref].title;
    }
    if (ui_state.overlay.type == UI_OVERLAY_BOOL) {
        return ui_state.app->bool_bindings[ui_state.overlay.ref].title;
    }
    return ui_state.app->texts.message_title;
}

static void ui_draw_dialog_text(const char *text)
{
    const char *newline;
    char first[32];
    size_t length = 0U;

    if (text == NULL) {
        return;
    }
    newline = text;
    while (*newline != '\0' && *newline != '\n') {
        ++newline;
    }
    if (*newline == '\n') {
        while (text[length] != '\n' && length + 1U < sizeof(first)) {
            first[length] = text[length];
            ++length;
        }
        first[length] = '\0';
        UI_DrawCentered(10, 4, 108U, first);
        UI_DrawCentered(10, 20, 108U, newline + 1);
    } else {
        UI_DrawCentered(10, 13, 108U, text);
    }
}

static void ui_draw_int_value(void)
{
    const UI_IntBinding *binding =
        &ui_state.app->int_bindings[ui_state.overlay.ref];
    char old_text[20];
    char new_text[20];

    UI_FormatInt(ui_state.overlay.previous, binding->unit,
                    old_text, sizeof(old_text));
    UI_FormatInt(ui_state.overlay.draft, binding->unit,
                    new_text, sizeof(new_text));
    OLED_SetClipWindow(39, 21, 46U, 16U);
    if (ui_state.overlay.value_moving != 0U) {
        int16_t offset = (int16_t)(16 * ui_state.overlay.value_q12 /
                                   UI_Q12_ONE);
        if (ui_state.overlay.value_direction > 0) {
            UI_DrawCentered(39, (int16_t)(21 - offset), 46U, old_text);
            UI_DrawCentered(39, (int16_t)(37 - offset), 46U, new_text);
        } else {
            UI_DrawCentered(39, (int16_t)(21 + offset), 46U, old_text);
            UI_DrawCentered(39, (int16_t)(5 + offset), 46U, new_text);
        }
    } else {
        UI_DrawCentered(39, 21, 46U, new_text);
    }
}

static void ui_draw_bool_value(void)
{
    bool on = ui_state.overlay.draft != 0;
    uint16_t p = ui_state.overlay.value_moving != 0U ?
                 ui_state.overlay.value_q12 : UI_Q12_ONE;
    UI_DrawCentered(31, 21, 14U, ui_state.app->texts.off_text);
    UI_DrawCentered(83, 21, 14U, ui_state.app->texts.on_text);
    UI_DrawSwitch(48, 22, 33U, 14U, on, p);
}

void UI_DialogDraw(void)
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int32_t focus_x;
    int32_t focus_y;
    int32_t focus_w;
    int32_t focus_h;
    uint16_t p;

    if (!UI_DialogActive()) {
        return;
    }
    p = ui_state.overlay.phase_q12;
    x = (int16_t)(3 + 3 * p / UI_Q12_ONE);
    y = (int16_t)(ui_state.overlay.origin_y +
                  ((int32_t)1 - ui_state.overlay.origin_y) * p /
                  UI_Q12_ONE);
    width = (int16_t)(118 - 2 * p / UI_Q12_ONE);
    height = (int16_t)(16 + 45 * p / UI_Q12_ONE);
    OLED_ResetClipWindow();
    OLED_SetDrawMode(OLED_DRAW_CLEAR);
    OLED_DrawRBox(x, y, (uint16_t)width, (uint16_t)height, 3U);
    OLED_SetDrawMode(OLED_DRAW_SET);
    OLED_DrawRFrame(x, y, (uint16_t)width, (uint16_t)height, 3U);
    if (width <= 2 || height <= 2) {
        return;
    }
    OLED_SetClipWindow((int16_t)(x + 1), (int16_t)(y + 1),
                       (uint16_t)(width - 2), (uint16_t)(height - 2));
    OLED_SetFont(ui_state.app->fonts.body_font);
    if (ui_state.overlay.type == UI_OVERLAY_CONFIRM) {
        ui_draw_dialog_text(ui_state.overlay.text);
        UI_DrawCentered(15, 41, 36U, ui_state.app->texts.cancel_text);
        UI_DrawCentered(77, 41, 36U, ui_state.app->texts.confirm_text);
    } else {
        UI_DrawCentered(10, 3, 108U, ui_dialog_title());
        OLED_DrawHLine(12, 19, 104U);
        if (ui_state.overlay.type == UI_OVERLAY_INT) {
            ui_draw_int_value();
            OLED_SetClipWindow((int16_t)(x + 1), (int16_t)(y + 1),
                               (uint16_t)(width - 2),
                               (uint16_t)(height - 2));
            UI_DrawCentered(15, 41, 36U, ui_state.app->texts.cancel_text);
            UI_DrawCentered(77, 41, 36U, ui_state.app->texts.confirm_text);
            if (ui_state.overlay.editing != 0U) {
                const UI_IntBinding *binding =
                    &ui_state.app->int_bindings[ui_state.overlay.ref];
                if (ui_state.overlay.draft < binding->maximum) {
                    OLED_DrawLine(91, 26, 95, 22);
                    OLED_DrawLine(95, 22, 99, 26);
                }
                if (ui_state.overlay.draft > binding->minimum) {
                    OLED_DrawLine(91, 31, 95, 35);
                    OLED_DrawLine(95, 35, 99, 31);
                }
            }
        } else if (ui_state.overlay.type == UI_OVERLAY_BOOL) {
            ui_draw_bool_value();
            UI_DrawCentered(15, 41, 36U, ui_state.app->texts.cancel_text);
            UI_DrawCentered(77, 41, 36U, ui_state.app->texts.confirm_text);
        } else {
            UI_DrawCentered(8, 23, 112U, ui_state.overlay.text);
            UI_DrawCentered(46, 41, 36U, ui_state.app->texts.confirm_text);
        }
    }
    ui_dialog_current_focus(ui_state.last_update, &focus_x, &focus_y,
                               &focus_w, &focus_h);
    OLED_SetDrawMode(OLED_DRAW_XOR);
    OLED_DrawRBox(UI_RoundQ8(focus_x), UI_RoundQ8(focus_y),
                  (uint16_t)UI_RoundQ8(focus_w),
                  (uint16_t)UI_RoundQ8(focus_h), 3U);
    OLED_SetDrawMode(OLED_DRAW_SET);
    OLED_ResetClipWindow();
}

#else

void UI_DialogDraw(void)
{
}

#endif

void UI_ToastDraw(void)
{
#if UI_ENABLE_TOAST
    uint16_t width;
    int16_t x;
    int16_t y;
    if (ui_state.toast.visible == 0U || UI_DialogActive()) {
        return;
    }
    OLED_SetFont(ui_state.app->fonts.body_font);
    width = (uint16_t)(OLED_GetUTF8Width(ui_state.toast.text) + 12U);
    if (width > 120U) {
        width = 120U;
    }
    if ((width & 1U) != 0U) {
        ++width;
    }
    x = (int16_t)((UI_SCREEN_WIDTH - width) / 2);
    y = (int16_t)(UI_SCREEN_HEIGHT -
                  (16 * ui_state.toast.phase_q12 / UI_Q12_ONE));
    OLED_SetClipWindow(x, y, width, 16U);

    /* Make the overlay independent of the page beneath it. Clearing the full
     * bounding box also leaves clean black corner pixels around the rounded
     * white label instead of allowing page glyphs to touch its silhouette. */
    OLED_SetDrawMode(OLED_DRAW_CLEAR);
    OLED_DrawBox(x, y, width, 16U);
    OLED_SetDrawMode(OLED_DRAW_SET);
    OLED_DrawRBox(x, y, width, 16U, 3U);

    /* Draw black text into the opaque white label. */
    OLED_SetDrawMode(OLED_DRAW_CLEAR);
    UI_DrawCentered(x, (int16_t)(y + 1), width, ui_state.toast.text);
    OLED_SetDrawMode(OLED_DRAW_SET);
    OLED_ResetClipWindow();
#endif
}
