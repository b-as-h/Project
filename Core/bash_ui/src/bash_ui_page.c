#include "bash_ui_internal.h"

void UI_CustomInput(UI_InputEvent event)
{
    ui_state.in_callback = 1U;
    UI_CustomOnInput(ui_state.current.page, event);
    ui_state.in_callback = 0U;
}

void UI_CustomTick(uint32_t now)
{
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
}
