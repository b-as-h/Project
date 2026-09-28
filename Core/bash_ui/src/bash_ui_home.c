#include "bash_ui_internal.h"
#include "font_bash.h"

#if UI_ENABLE_HOME

static const UI_HomePage *ui_home_page(const UI_PageState *state)
{
    const UI_PageRoute *route = UI_GetRoute(state->page);
    return &ui_state.app->home_pages[route->index];
}

static int16_t ui_home_delta(uint16_t index, uint16_t selected,
                                uint16_t count)
{
    int32_t delta = (int32_t)index - selected;
    int32_t half = count / 2U;
    if (delta > half) {
        delta -= count;
    } else if (delta < -half) {
        delta += count;
    }
    return (int16_t)delta;
}

static int32_t ui_home_label_width_q8(const UI_HomePage *page,
                                         uint16_t selected)
{
    int32_t width;
    width = (int32_t)Font_GetMixedStringWidth(page->items[selected].label) + 12;
    if (width > 120) {
        width = 120;
    }
    width = (width + 1) & ~1;
    return width * 256;
}

static int16_t ui_home_text_spacing(const UI_HomePage *page)
{
    uint16_t maximum = 0U;
    uint16_t i;

    for (i = 0U; i < page->item_count; ++i) {
        uint16_t width = (uint16_t)(Font_GetMixedStringWidth(page->items[i].label) + 12U);
        if (width > maximum) {
            maximum = width;
        }
    }
    if (maximum > 120U) {
        maximum = 120U;
    }
    return (int16_t)(maximum + 4U);
}

static void ui_home_values(uint32_t now, int32_t *offset_q8,
                              int32_t *label_q8)
{
    const UI_HomePage *page = ui_home_page(&ui_state.current);
    if (ui_state.home_anim.active != 0U) {
        uint16_t p = UI_EaseQ12(now - ui_state.home_anim.started, UI_HOME_MS);
        *offset_q8 = UI_LerpQ12(ui_state.home_anim.offset_from_q8, 0, p);
        *label_q8 = UI_LerpQ12(ui_state.home_anim.label_from_q8,
                                 ui_state.home_anim.label_to_q8, p);
    } else {
        *offset_q8 = 0;
        *label_q8 = ui_home_label_width_q8(page, ui_state.current.selected);
    }
}

void UI_HomeAnimate(uint32_t now)
{
    if (UI_CurrentPageType() != UI_PAGE_HOME ||
        ui_state.home_anim.active == 0U) {
        return;
    }
    ui_state.dirty = 1U;
    if (now - ui_state.home_anim.started >= UI_HOME_MS) {
        ui_state.home_anim.active = 0U;
    }
}

static void ui_home_move(int16_t direction, uint16_t steps, uint32_t now)
{
    const UI_HomePage *page = ui_home_page(&ui_state.current);
    int32_t current_offset;
    int32_t current_label;
    int32_t move;

    if (page->item_count == 0U || steps == 0U) {
        return;
    }
    ui_home_values(now, &current_offset, &current_label);
    move = (int32_t)(steps % page->item_count);
    if (move == 0) {
        return;
    }
    if (direction < 0) {
        move = -move;
    }
    ui_state.current.selected = (uint16_t)(((int32_t)ui_state.current.selected + move +
                                (int32_t)page->item_count * 2) %
                               page->item_count);
    ui_state.home_anim.offset_from_q8 = current_offset + direction * 64 * 256;
    ui_state.home_anim.label_from_q8 = current_label;
    ui_state.home_anim.label_to_q8 =
        ui_home_label_width_q8(page, ui_state.current.selected);
    ui_state.home_anim.started = now;
    ui_state.home_anim.active = 1U;
    ui_state.dirty = 1U;
}

void UI_HomeInput(UI_InputEvent event, uint32_t now)
{
    const UI_HomePage *page = ui_home_page(&ui_state.current);

    if (event.action == UI_INPUT_OK) {
        UI_PageId target = page->items[ui_state.current.selected].target_page;
        ui_state.home_anim.active = 0U;
        (void)UI_NavigateTo(target, now);
    } else if (event.action == UI_INPUT_UP) {
        ui_home_move(-1, event.steps, now);
    } else {
        ui_home_move(1, event.steps, now);
    }
}

void UI_HomeDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{
    const UI_HomePage *page = ui_home_page(state);
    int32_t icon_offset_q8 = 0;
    int32_t label_width_q8;
    int16_t icon_offset;
    int16_t label_width;
    int16_t label_left;
    int16_t text_spacing;
    int16_t text_offset;
    uint16_t i;
    int16_t clip_right = (int16_t)(clip_x + clip_width);

    OLED_SetClipWindow(clip_x, 0, clip_width, UI_SCREEN_HEIGHT);
    if (state == &ui_state.current && ui_state.home_anim.active != 0U) {
        uint16_t p = UI_EaseQ12(ui_state.last_update - ui_state.home_anim.started,
                                   UI_HOME_MS);
        icon_offset_q8 = UI_LerpQ12(ui_state.home_anim.offset_from_q8, 0, p);
        label_width_q8 = UI_LerpQ12(ui_state.home_anim.label_from_q8,
                                      ui_state.home_anim.label_to_q8, p);
    } else {
        label_width_q8 = ui_home_label_width_q8(page, state->selected);
    }
    icon_offset = UI_RoundQ8(icon_offset_q8);
    label_width = UI_RoundQ8(label_width_q8);
    text_spacing = ui_home_text_spacing(page);
    text_offset = (int16_t)((int32_t)icon_offset * text_spacing / 64);

    for (i = 0U; i < page->item_count; ++i) {
        int16_t delta = ui_home_delta(i, state->selected, page->item_count);
        int16_t x = (int16_t)(48 + delta * 64 + icon_offset + x_offset);
        if (x < clip_right && x + 32 > clip_x) {
            OLED_DrawXBM(x, 8, 32U, 32U, page->items[i].icon_xbm_32x32);
        }
    }

    label_left = (int16_t)(64 - label_width / 2 + x_offset);
    {
        int16_t left = label_left > clip_x ? label_left : clip_x;
        int16_t right = (int16_t)(label_left + label_width);
        if (right > clip_right) {
            right = clip_right;
        }
        if (right > left) {
            OLED_SetClipWindow(left, 44, (uint16_t)(right - left), 20U);
            for (i = 0U; i < page->item_count; ++i) {
                int16_t delta = ui_home_delta(i, state->selected,
                                                 page->item_count);
                int16_t text_x = (int16_t)((UI_SCREEN_WIDTH -
                    (int16_t)Font_GetMixedStringWidth(page->items[i].label)) / 2 +
                    delta * text_spacing + text_offset + x_offset);
                Font_DrawMixedString(text_x, 46, page->items[i].label);
            }
            OLED_SetDrawMode(OLED_DRAW_XOR);
            OLED_DrawRBox(label_left, 44, (uint16_t)label_width, 20U, 3U);
            OLED_SetDrawMode(OLED_DRAW_SET);
        }
    }
    OLED_ResetClipWindow();
}

#else
void UI_HomeAnimate(uint32_t now) { (void)now; }
void UI_HomeInput(UI_InputEvent event, uint32_t now)
{ (void)event; (void)now; }
void UI_HomeDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{ (void)state; (void)x_offset; (void)clip_x; (void)clip_width; }
#endif
