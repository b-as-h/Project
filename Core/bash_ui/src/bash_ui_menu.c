#include "bash_ui_internal.h"
#include "bash_ui_draw.h"

#if UI_ENABLE_MENU

static const UI_MenuPage *ui_menu_page(const UI_PageState *state)
{
    const UI_PageRoute *route = UI_GetRoute(state->page);
    return &ui_state.app->menu_pages[route->index];
}

UI_ItemState UI_MenuGetItemState(const UI_MenuPage *page,
                                       uint16_t item_index)
{
    uint8_t packed;
    if (page->item_states == NULL) {
        return UI_ITEM_NORMAL;
    }
    packed = page->item_states[item_index >> 2U];
    return (UI_ItemState)((packed >> ((item_index & 3U) * 2U)) & 3U);
}

static uint16_t ui_menu_visible_count(const UI_MenuPage *page)
{
    uint16_t count = 1U; /* Virtual Return. */
    uint16_t i;
    for (i = 0U; i < page->item_count; ++i) {
        if (UI_MenuGetItemState(page, i) != UI_ITEM_HIDDEN) {
            ++count;
        }
    }
    return count;
}

static uint16_t ui_menu_selectable_count(const UI_MenuPage *page)
{
    uint16_t count = 1U;
    uint16_t i;
    for (i = 0U; i < page->item_count; ++i) {
        if (UI_MenuGetItemState(page, i) == UI_ITEM_NORMAL) {
            ++count;
        }
    }
    return count;
}

static uint16_t ui_menu_visible_to_item(const UI_MenuPage *page,
                                           uint16_t visible)
{
    uint16_t i;
    for (i = 0U; i < page->item_count; ++i) {
        if (UI_MenuGetItemState(page, i) == UI_ITEM_HIDDEN) {
            continue;
        }
        if (visible == 0U) {
            return i;
        }
        --visible;
    }
    return page->item_count;
}

static uint16_t ui_menu_item_to_visible(const UI_MenuPage *page,
                                           uint16_t item)
{
    uint16_t visible = 0U;
    uint16_t i;
    for (i = 0U; i < page->item_count; ++i) {
        if (i == item) {
            return visible;
        }
        if (UI_MenuGetItemState(page, i) != UI_ITEM_HIDDEN) {
            ++visible;
        }
    }
    return visible;
}

static bool ui_menu_item_selectable(const UI_MenuPage *page,
                                       uint16_t item)
{
    return item == page->item_count ||
           UI_MenuGetItemState(page, item) == UI_ITEM_NORMAL;
}

void UI_MenuRepairFocus(UI_PageState *state)
{
    const UI_MenuPage *page = ui_menu_page(state);
    uint16_t original;
    uint16_t i;
    uint16_t visible;

    if (state->selected > page->item_count) {
        state->selected = page->item_count;
    }
    if (ui_menu_item_selectable(page, state->selected)) {
        visible = ui_menu_item_to_visible(page, state->selected);
    } else {
        original = state->selected;
        state->selected = page->item_count;
        for (i = (uint16_t)(original + 1U); i < page->item_count; ++i) {
            if (ui_menu_item_selectable(page, i)) {
                state->selected = i;
                break;
            }
        }
        if (state->selected == page->item_count) {
            i = original;
            while (i > 0U) {
                --i;
                if (ui_menu_item_selectable(page, i)) {
                    state->selected = i;
                    break;
                }
            }
        }
        visible = ui_menu_item_to_visible(page, state->selected);
    }
    if (visible < state->top) {
        state->top = visible;
    } else if (visible >= state->top + UI_VISIBLE_ROWS) {
        state->top = (uint16_t)(visible - (UI_VISIBLE_ROWS - 1U));
    }
    {
        uint16_t count = ui_menu_visible_count(page);
        uint16_t max_top = count > UI_VISIBLE_ROWS ?
                           (uint16_t)(count - UI_VISIBLE_ROWS) : 0U;
        if (state->top > max_top) {
            state->top = max_top;
        }
    }
    state->scroll_q8 = (int32_t)state->top * UI_ROW_HEIGHT * 256;
    state->focus_q8 = (int32_t)(visible - state->top) *
                      UI_ROW_HEIGHT * 256;
}

static void ui_menu_animated_values(uint32_t now, int32_t *scroll,
                                       int32_t *focus)
{
    if (ui_state.list_anim.active != 0U) {
        uint16_t p = UI_EaseQ12(now - ui_state.list_anim.started,
                                   ui_state.list_anim.duration);
        *scroll = UI_LerpQ12(ui_state.list_anim.scroll_from_q8,
                               ui_state.current.scroll_q8, p);
        *focus = UI_LerpQ12(ui_state.list_anim.focus_from_q8,
                              ui_state.current.focus_q8, p);
    } else {
        *scroll = ui_state.current.scroll_q8;
        *focus = ui_state.current.focus_q8;
    }
}

void UI_MenuAnimate(uint32_t now)
{
    if (UI_CurrentPageType() != UI_PAGE_MENU ||
        ui_state.list_anim.active == 0U) {
        return;
    }
    ui_state.dirty = 1U;
    if (now - ui_state.list_anim.started >= ui_state.list_anim.duration) {
        ui_state.list_anim.active = 0U;
    }
}

static void ui_menu_move(int16_t direction, uint16_t steps, uint32_t now)
{
    const UI_MenuPage *page = ui_menu_page(&ui_state.current);
    uint16_t count = ui_menu_visible_count(page);
    uint16_t selectable = ui_menu_selectable_count(page);
    uint16_t visible = ui_menu_item_to_visible(page, ui_state.current.selected);
    uint16_t remaining;
    int32_t old_scroll;
    int32_t old_focus;

    if (steps == 0U || selectable == 0U) {
        return;
    }
    ui_menu_animated_values(now, &old_scroll, &old_focus);
    remaining = (uint16_t)(steps % selectable);
    if (remaining == 0U) {
        return;
    }
    while (remaining != 0U) {
        uint16_t item;
        do {
            if (direction < 0) {
                visible = visible == 0U ? (uint16_t)(count - 1U) :
                          (uint16_t)(visible - 1U);
            } else {
                visible = (uint16_t)((visible + 1U) % count);
            }
            item = ui_menu_visible_to_item(page, visible);
        } while (!ui_menu_item_selectable(page, item));
        ui_state.current.selected = item;
        --remaining;
    }
    if (visible < ui_state.current.top) {
        ui_state.current.top = visible;
    } else if (visible >= ui_state.current.top + UI_VISIBLE_ROWS) {
        ui_state.current.top = (uint16_t)(visible - (UI_VISIBLE_ROWS - 1U));
    }
    {
        uint16_t max_top = count > UI_VISIBLE_ROWS ?
                           (uint16_t)(count - UI_VISIBLE_ROWS) : 0U;
        if (ui_state.current.top > max_top) {
            ui_state.current.top = max_top;
        }
    }
    ui_state.current.scroll_q8 = (int32_t)ui_state.current.top * 4096;
    ui_state.current.focus_q8 = (int32_t)(visible - ui_state.current.top) * 4096;
    ui_state.list_anim.scroll_from_q8 = old_scroll;
    ui_state.list_anim.focus_from_q8 = old_focus;
    ui_state.list_anim.started = now;
    ui_state.list_anim.duration = UI_MENU_MS;
    ui_state.list_anim.active = 1U;
    ui_state.dirty = 1U;
}

static void ui_menu_reject(UI_Status status, uint16_t index)
{
    UI_RecordError(status, ui_state.current.page, index);
    ui_state.reject_flash = 2U;
    ui_state.dirty = 1U;
}

static void ui_menu_activate(uint32_t now)
{
    const UI_MenuPage *page = ui_menu_page(&ui_state.current);
    const UI_MenuItem *item;
    UI_Status status = UI_OK;

    if (ui_state.current.selected == page->item_count) {
        (void)UI_NavigateBack(now);
        return;
    }
    item = &page->items[ui_state.current.selected];
    switch (item->type) {
    case UI_MENU_PAGE:
        status = UI_NavigateTo((UI_PageId)item->ref, now);
        break;
    case UI_MENU_ACTION:
        if (!UI_QueueEvent((UI_EventId)item->ref)) {
            status = UI_QUEUE_FULL;
        }
        break;
    case UI_MENU_INT:
        status = UI_DialogOpenInt(item->ref, now);
        break;
    case UI_MENU_BOOL:
        status = UI_DialogOpenBool(item->ref, now);
        break;
    case UI_MENU_CONFIRM:
        status = UI_DialogOpenConfirm(item->ref, now);
        break;
    default:
        status = UI_CONFIG_ERROR;
        break;
    }
    if (status != UI_OK && status != UI_BUSY) {
        ui_menu_reject(status, ui_state.current.selected);
    }
}

void UI_MenuInput(UI_InputEvent event, uint32_t now)
{
    if (event.action == UI_INPUT_OK) {
        ui_menu_activate(now);
    } else if (event.action == UI_INPUT_UP) {
        ui_menu_move(-1, event.steps, now);
    } else {
        ui_menu_move(1, event.steps, now);
    }
}

static void ui_draw_enter_arrow(int16_t x, int16_t y, bool back)
{
    int16_t point = (int16_t)(x + (back ? -3 : 3));
    OLED_DrawLine(x, (int16_t)(y + 5), point, (int16_t)(y + 8));
    OLED_DrawLine(point, (int16_t)(y + 8), x, (int16_t)(y + 11));
}

static void ui_draw_disabled(int16_t x, int16_t y)
{
    OLED_DrawCircle(x, y, 4U);
    OLED_DrawLine((int16_t)(x - 3), (int16_t)(y + 3),
                  (int16_t)(x + 3), (int16_t)(y - 3));
}

static void ui_menu_value(const UI_MenuItem *item, char *buffer,
                             size_t capacity)
{
    buffer[0] = '\0';
    if (item->type == UI_MENU_INT) {
        const UI_IntBinding *binding = &ui_state.app->int_bindings[item->ref];
        UI_FormatInt(*binding->value, binding->unit, buffer, capacity);
    } else if (item->type == UI_MENU_BOOL) {
        const UI_BoolBinding *binding = &ui_state.app->bool_bindings[item->ref];
        const char *text = *binding->value ? ui_state.app->texts.on_text :
                                             ui_state.app->texts.off_text;
        size_t i = 0U;
        while (i + 1U < capacity && text[i] != '\0') {
            buffer[i] = text[i];
            ++i;
        }
        buffer[i] = '\0';
    }
}

static void ui_menu_content_clip(int16_t left, int16_t right,
                                    int16_t clip_x, int16_t clip_right)
{
    if (left < clip_x) left = clip_x;
    if (right > clip_right) right = clip_right;
    if (right < left) right = left;
    OLED_SetClipWindow(left, UI_MENU_HEADER_HEIGHT,
                       (uint16_t)(right - left),
                       UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
}

void UI_MenuDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{
    const UI_MenuPage *page = ui_menu_page(state);
    int32_t scroll = state->scroll_q8;
    int32_t focus = state->focus_q8;
    uint16_t visible_count = ui_menu_visible_count(page);
    uint16_t visible;
    int16_t clip_right = (int16_t)(clip_x + clip_width);

    if (state == &ui_state.current && ui_state.list_anim.active != 0U) {
        ui_menu_animated_values(ui_state.last_update, &scroll, &focus);
    }
    OLED_SetFont(ui_state.app->fonts.title_font);
    OLED_SetClipWindow(clip_x, 0, clip_width, UI_MENU_HEADER_HEIGHT);
    UI_DrawCentered(x_offset, 1, UI_SCREEN_WIDTH, page->title);

    OLED_SetFont(ui_state.app->fonts.body_font);
    OLED_SetClipWindow(clip_x, UI_MENU_HEADER_HEIGHT, clip_width,
                       UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
    for (visible = 0U; visible < visible_count; ++visible) {
        uint16_t item_index = ui_menu_visible_to_item(page, visible);
        int16_t y = (int16_t)(UI_MENU_HEADER_HEIGHT + visible *
                              UI_ROW_HEIGHT - scroll / 256);
        char value[20];

        if (y >= UI_MENU_HEADER_HEIGHT + UI_VISIBLE_ROWS *
                 UI_ROW_HEIGHT || y + UI_ROW_HEIGHT <=
            UI_MENU_HEADER_HEIGHT) {
            continue;
        }
        if (item_index == page->item_count) {
            ui_menu_content_clip((int16_t)(8 + x_offset),
                                    (int16_t)(106 + x_offset),
                                    clip_x, clip_right);
            OLED_DrawUTF8((int16_t)(8 + x_offset), y,
                          ui_state.app->texts.return_text);
            OLED_SetClipWindow(clip_x, UI_MENU_HEADER_HEIGHT, clip_width,
                               UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
            ui_draw_enter_arrow((int16_t)(110 + x_offset), y, true);
            continue;
        }
        value[0] = '\0';
        ui_menu_value(&page->items[item_index], value, sizeof(value));
        if (value[0] != '\0') {
            int16_t right = UI_MenuGetItemState(page, item_index) ==
                            UI_ITEM_NORMAL ? 112 : 106;
            int16_t value_x = (int16_t)(right - OLED_GetUTF8Width(value) +
                                        x_offset);
            ui_menu_content_clip((int16_t)(8 + x_offset),
                                    (int16_t)(value_x - 2),
                                    clip_x, clip_right);
            OLED_DrawUTF8((int16_t)(8 + x_offset), y,
                          page->items[item_index].label);
            OLED_SetClipWindow(clip_x, UI_MENU_HEADER_HEIGHT, clip_width,
                               UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
            OLED_DrawUTF8(value_x, y, value);
        } else if (page->items[item_index].type == UI_MENU_PAGE) {
            ui_menu_content_clip((int16_t)(8 + x_offset),
                                    (int16_t)(106 + x_offset),
                                    clip_x, clip_right);
            OLED_DrawUTF8((int16_t)(8 + x_offset), y,
                          page->items[item_index].label);
            OLED_SetClipWindow(clip_x, UI_MENU_HEADER_HEIGHT, clip_width,
                               UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
            ui_draw_enter_arrow((int16_t)(110 + x_offset), y, false);
        } else {
            int16_t right = UI_MenuGetItemState(page, item_index) ==
                            UI_ITEM_NORMAL ? 121 : 110;
            ui_menu_content_clip((int16_t)(8 + x_offset),
                                    (int16_t)(right + x_offset),
                                    clip_x, clip_right);
            OLED_DrawUTF8((int16_t)(8 + x_offset), y,
                          page->items[item_index].label);
            OLED_SetClipWindow(clip_x, UI_MENU_HEADER_HEIGHT, clip_width,
                               UI_VISIBLE_ROWS * UI_ROW_HEIGHT);
        }
        if (UI_MenuGetItemState(page, item_index) != UI_ITEM_NORMAL) {
            ui_draw_disabled((int16_t)(117 + x_offset),
                                (int16_t)(y + 8));
        }
    }
    OLED_SetDrawMode(OLED_DRAW_XOR);
    OLED_DrawRBox((int16_t)(3 + x_offset),
                  (int16_t)(UI_MENU_HEADER_HEIGHT + focus / 256),
                  118U, UI_ROW_HEIGHT, 3U);
    OLED_SetDrawMode(OLED_DRAW_SET);
    if (visible_count > UI_VISIBLE_ROWS &&
        x_offset + 124 < clip_right && x_offset + 127 >= clip_x) {
        uint16_t first = (uint16_t)((scroll / 256) /
                                    UI_ROW_HEIGHT);
        uint16_t thumb_height = (uint16_t)(48U * UI_VISIBLE_ROWS /
                                           visible_count);
        uint16_t maximum_first = (uint16_t)(visible_count -
                                            UI_VISIBLE_ROWS);
        uint16_t thumb_y = (uint16_t)(UI_MENU_HEADER_HEIGHT +
            (48U - thumb_height) * first / maximum_first);
        OLED_DrawVLine((int16_t)(125 + x_offset), UI_MENU_HEADER_HEIGHT,
                       48U);
        OLED_DrawBox((int16_t)(124 + x_offset), (int16_t)thumb_y, 3U,
                     thumb_height);
    }
    OLED_ResetClipWindow();
}

#else
UI_ItemState UI_MenuGetItemState(const UI_MenuPage *page,
                                       uint16_t item_index)
{ (void)page; (void)item_index; return UI_ITEM_NORMAL; }
void UI_MenuRepairFocus(UI_PageState *state) { (void)state; }
void UI_MenuAnimate(uint32_t now) { (void)now; }
void UI_MenuInput(UI_InputEvent event, uint32_t now)
{ (void)event; (void)now; }
void UI_MenuDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{ (void)state; (void)x_offset; (void)clip_x; (void)clip_width; }
#endif
