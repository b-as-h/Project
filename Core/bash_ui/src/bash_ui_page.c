#include "bash_ui_internal.h"
#include "bash_ui_draw.h"

#if UI_ENABLE_INFO

static const UI_InfoPage *ui_info_page(const UI_PageState *state)
{
    const UI_PageRoute *route = UI_GetRoute(state->page);
    return &ui_state.app->info_pages[route->index];
}

static uint16_t ui_info_header_height(const UI_InfoPage *page)
{
    return page->title != NULL && page->title[0] != '\0' ?
           UI_INFO_HEADER_HEIGHT : 0U;
}

static int32_t ui_info_max_scroll_q8(const UI_InfoPage *page)
{
    uint16_t header = ui_info_header_height(page);
    int32_t content = (int32_t)page->row_count * UI_ROW_HEIGHT;
    int32_t viewport = UI_SCREEN_HEIGHT - header;
    return content > viewport ? (content - viewport) * 256 : 0;
}

static int32_t ui_info_scroll(uint32_t now)
{
    if (ui_state.list_anim.active != 0U) {
        uint16_t p = UI_EaseQ12(now - ui_state.list_anim.started,
                                   ui_state.list_anim.duration);
        return UI_LerpQ12(ui_state.list_anim.scroll_from_q8,
                            ui_state.current.scroll_q8, p);
    }
    return ui_state.current.scroll_q8;
}

void UI_InfoAnimate(uint32_t now)
{
    if (UI_CurrentPageType() != UI_PAGE_INFO ||
        ui_state.list_anim.active == 0U) {
        return;
    }
    ui_state.dirty = 1U;
    if (now - ui_state.list_anim.started >= ui_state.list_anim.duration) {
        ui_state.list_anim.active = 0U;
    }
}

static void ui_info_move(int16_t direction, uint16_t steps, uint32_t now)
{
    const UI_InfoPage *page = ui_info_page(&ui_state.current);
    int32_t current = ui_info_scroll(now);
    int32_t target = ui_state.current.scroll_q8;
    int32_t maximum = ui_info_max_scroll_q8(page);
    int32_t amount;

    if (steps == 0U || maximum == 0) {
        return;
    }
    amount = (int32_t)steps * UI_ROW_HEIGHT * 256;
    if (direction < 0) {
        target = target > amount ? target - amount : 0;
    } else {
        target = maximum - target > amount ? target + amount : maximum;
    }
    if (target == ui_state.current.scroll_q8) {
        return;
    }
    ui_state.current.scroll_q8 = target;
    ui_state.current.top = (uint16_t)(target / (UI_ROW_HEIGHT * 256));
    ui_state.list_anim.scroll_from_q8 = current;
    ui_state.list_anim.focus_from_q8 = 0;
    ui_state.list_anim.started = now;
    ui_state.list_anim.duration = UI_INFO_SCROLL_MS;
    ui_state.list_anim.active = 1U;
    ui_state.dirty = 1U;
}

void UI_InfoInput(UI_InputEvent event, uint32_t now)
{
    if (event.action == UI_INPUT_OK) {
        (void)UI_NavigateBack(now);
    } else if (event.action == UI_INPUT_UP) {
        ui_info_move(-1, event.steps, now);
    } else {
        ui_info_move(1, event.steps, now);
    }
}

static void ui_info_close_icon(int16_t x_offset)
{
    OLED_DrawRBox((int16_t)(114 + x_offset), 1, 12U, 12U, 2U);
    OLED_SetDrawMode(OLED_DRAW_CLEAR);
    OLED_DrawLine((int16_t)(117 + x_offset), 4,
                  (int16_t)(122 + x_offset), 9);
    OLED_DrawLine((int16_t)(122 + x_offset), 4,
                  (int16_t)(117 + x_offset), 9);
    OLED_SetDrawMode(OLED_DRAW_SET);
}

void UI_InfoDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{
    const UI_InfoPage *page = ui_info_page(state);
    uint16_t header = ui_info_header_height(page);
    int32_t scroll = state->scroll_q8;
    uint16_t i;
    uint16_t visible_rows = (uint16_t)((UI_SCREEN_HEIGHT - header) /
                                      UI_ROW_HEIGHT);
    int16_t clip_right = (int16_t)(clip_x + clip_width);
    int16_t content_left = clip_x;
    int16_t content_right = clip_right;
    bool draw_content = true;

    if (state == &ui_state.current && ui_state.list_anim.active != 0U) {
        scroll = ui_info_scroll(ui_state.last_update);
    }
    if (header != 0U) {
        OLED_SetFont(ui_state.app->fonts.title_font);
        OLED_SetClipWindow(clip_x, 0, clip_width, header);
        UI_DrawCentered(x_offset, 1, UI_SCREEN_WIDTH, page->title);
        OLED_DrawHLine(x_offset, (int16_t)(header - 1U), 125U);
        ui_info_close_icon(x_offset);
    }
    OLED_SetFont(ui_state.app->fonts.body_font);
    {
        if (content_right > x_offset + 125) content_right = (int16_t)(x_offset + 125);
        if (content_left < x_offset) content_left = x_offset;
        if (content_right > content_left) {
            OLED_SetClipWindow(content_left, (int16_t)header,
                               (uint16_t)(content_right - content_left),
                               (uint16_t)(UI_SCREEN_HEIGHT - header));
        } else {
            draw_content = false;
        }
    }
    for (i = 0U; i < page->row_count; ++i) {
        int16_t y = (int16_t)(header + i * UI_ROW_HEIGHT -
                              UI_RoundQ8(scroll));
        const char *value = page->rows[i].value;
        if (!draw_content || y >= UI_SCREEN_HEIGHT ||
            y + UI_ROW_HEIGHT <= header) {
            continue;
        }
        if (value != NULL && value[0] != '\0') {
            int16_t value_x = (int16_t)(121 - OLED_GetUTF8Width(value) +
                                        x_offset);
            int16_t name_left = (int16_t)(6 + x_offset);
            int16_t name_right = (int16_t)(value_x - 2);
            if (name_left < content_left) name_left = content_left;
            if (name_right > content_right) name_right = content_right;
            if (name_right > name_left) {
                OLED_SetClipWindow(name_left, (int16_t)header,
                                   (uint16_t)(name_right - name_left),
                                   (uint16_t)(UI_SCREEN_HEIGHT - header));
                OLED_DrawUTF8((int16_t)(6 + x_offset), y,
                              page->rows[i].name);
            }
            OLED_SetClipWindow(content_left, (int16_t)header,
                               (uint16_t)(content_right - content_left),
                               (uint16_t)(UI_SCREEN_HEIGHT - header));
            OLED_DrawUTF8(value_x, y, value);
        } else {
            OLED_DrawUTF8((int16_t)(6 + x_offset), y,
                          page->rows[i].name);
        }
    }
    OLED_SetClipWindow(clip_x, 0, clip_width, UI_SCREEN_HEIGHT);
    if (page->row_count > visible_rows && x_offset + 126 >= clip_x &&
        x_offset + 126 < clip_right) {
        uint16_t viewport = (uint16_t)(UI_SCREEN_HEIGHT - header);
        uint16_t thumb_height = (uint16_t)((uint32_t)viewport * viewport /
                                  (page->row_count * UI_ROW_HEIGHT));
        int32_t maximum = ui_info_max_scroll_q8(page) / 256;
        int32_t scroll_pixels = UI_RoundQ8(scroll);
        uint16_t thumb_y;
        if (thumb_height < 8U) thumb_height = 8U;
        thumb_y = (uint16_t)(header +
            ((int32_t)viewport - thumb_height) * scroll_pixels / maximum);
        OLED_DrawBox((int16_t)(126 + x_offset), (int16_t)thumb_y, 2U,
                     thumb_height);
    }
    OLED_ResetClipWindow();
}

#else
void UI_InfoAnimate(uint32_t now) { (void)now; }
void UI_InfoInput(UI_InputEvent event, uint32_t now)
{ (void)event; (void)now; }
void UI_InfoDraw(const UI_PageState *state, int16_t x_offset,
                    int16_t clip_x, uint16_t clip_width)
{ (void)state; (void)x_offset; (void)clip_x; (void)clip_width; }
#endif

void UI_CustomInput(UI_InputEvent event)
{
#if UI_ENABLE_CUSTOM
    ui_state.in_callback = 1U;
    UI_CustomOnInput(ui_state.current.page, event);
    ui_state.in_callback = 0U;
#else
    (void)event;
#endif
}

void UI_CustomTick(uint32_t now)
{
#if UI_ENABLE_CUSTOM
    if (UI_CurrentPageType() == UI_PAGE_CUSTOM &&
        ui_state.transition.active == 0U) {
        bool redraw;
        ui_state.in_callback = 1U;
        redraw = UI_CustomOnTick(ui_state.current.page, now);
        ui_state.in_callback = 0U;
        if (redraw) {
            ui_state.dirty = 1U;
        }
    }
#else
    (void)now;
#endif
}
