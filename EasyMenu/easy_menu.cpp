//
// Created by 周煜 on 2026/2/21.
//

#include "easy_menu.h"
#include <cstring>
#include <iterator>

namespace easy_menu {
    MenuCell::MenuCell(const char* title, MenuCallback callback_func, void* user_data)
        : title(title), action(FUNCTION), user_data(user_data) {
        action_data.callback_func = callback_func;
    }

    MenuCell::MenuCell(const char* title, DynamicMenu* sub_menu)
        : title(title), action(SUB_DYNAMIC_MENU), user_data(nullptr) {
        action_data.sub_dynamic_menu = sub_menu;
    }

    MenuCell::MenuCell(const char* title, StaticMenu* sub_menu)
        : title(title), action(SUB_STATIC_MENU), user_data(nullptr) {
        action_data.sub_static_menu = sub_menu;
    }

    MenuCell::~MenuCell() {
#if ENABLE_SUBMENU_AUTO_DESTROY
        if (action == SUB_DYNAMIC_MENU && action_data.sub_dynamic_menu) {
            delete action_data.sub_dynamic_menu;
            action_data.sub_dynamic_menu = nullptr;
        }
        else if (action == SUB_STATIC_MENU && action_data.sub_static_menu) {
            delete action_data.sub_static_menu;
            action_data.sub_static_menu = nullptr;
        }
#endif
    }

    DynamicMenu::~DynamicMenu() {
        for (auto cell : menu_list) {
            delete cell;
        }
        menu_list.clear();
    }

    void DynamicMenu::add_menu(const char* title, MenuCallback callback_func, void* user_data) {
        menu_list.push_back(new MenuCell(title, callback_func, user_data));
        if (menu_list.size() == 1) {
            current_item = menu_list.begin();
        }
    }

    void DynamicMenu::add_menu(const char* title, StaticMenu* sub_menu) {
        menu_list.push_back(new MenuCell(title, sub_menu));
        if (menu_list.size() == 1) {
            current_item = menu_list.begin();
        }
        sub_menu->parent_menu = this;
    }

    void DynamicMenu::add_menu(const char* title, DynamicMenu* sub_menu) {
        menu_list.push_back(new MenuCell(title, sub_menu));
        if (menu_list.size() == 1) {
            current_item = menu_list.begin();
        }
        sub_menu->parent_menu = this;
    }

    void DynamicMenu::erase_menu(list<MenuCell*>::iterator cell_it) {
        delete *cell_it;
        menu_list.erase(cell_it);
        if (current_item == menu_list.end()) {
            current_item = menu_list.begin();
        }
    }

    uint32_t DynamicMenu::get_item_count() const {
        return menu_list.size();
    }

    MenuCell* DynamicMenu::get_current_item() {
        if (current_item != menu_list.end()) {
            return *current_item;
        }
        return nullptr;
    }

    void DynamicMenu::move_up() {
        if (can_move_up()) {
            --current_item;
        }
    }

    void DynamicMenu::move_down() {
        if (can_move_down()) {
            ++current_item;
        }
    }

    bool DynamicMenu::can_move_up() const {
        return current_item != menu_list.begin();
    }

    bool DynamicMenu::can_move_down() const {
        return current_item != menu_list.end() && std::next(current_item) != menu_list.end();
    }

    void DynamicMenu::reset_selection() {
        current_item = menu_list.begin();
    }

    StaticMenu::~StaticMenu() {
        if (auto_delete && menu_list) {
            delete[] menu_list;
        }
    }

    void StaticMenu::add_menu(const char* title, MenuCallback callback_func, void* user_data) {
        if (num < length) {
            menu_list[num] = MenuCell(title, callback_func, user_data);
            if (num == 0) {
                current_item = &menu_list[0];
            }
            num++;
        }
    }

    void StaticMenu::add_menu(const char* title, StaticMenu* sub_menu) {
        if (num < length) {
            menu_list[num] = MenuCell(title, sub_menu);
            if (num == 0) {
                current_item = &menu_list[0];
            }
            num++;
            sub_menu->parent_menu = this;
        }
    }

    void StaticMenu::add_menu(const char* title, DynamicMenu* sub_menu) {
        if (num < length) {
            menu_list[num] = MenuCell(title, sub_menu);
            if (num == 0) {
                current_item = &menu_list[0];
            }
            num++;
            sub_menu->parent_menu = this;
        }
    }

    void StaticMenu::erase_menu(MenuCell* cell) {
        if (cell >= menu_list && cell < menu_list + length) {
            cell->title = nullptr;
            cell->action = FUNCTION;
            cell->action_data.callback_func = nullptr;
            cell->user_data = nullptr;
        }
    }

    uint32_t StaticMenu::get_item_count() const {
        return num;
    }

    MenuCell* StaticMenu::get_current_item() {
        return current_item;
    }

    void StaticMenu::move_up() {
        if (can_move_up()) {
            current_item--;
        }
    }

    void StaticMenu::move_down() {
        if (can_move_down()) {
            current_item++;
        }
    }

    bool StaticMenu::can_move_up() const {
        return current_item > menu_list;
    }

    bool StaticMenu::can_move_down() const {
        return current_item < menu_list + num - 1;
    }

    void StaticMenu::reset_selection() {
        current_item = menu_list;
    }

    namespace {
        struct ScrollState {
            uint32_t last_scroll_time = 0;
            uint16_t offset = 0;
            bool fully_displayed = false;
            bool reset_pending = false;
        };

        void update_scroll_state(ScrollState& state, uint16_t text_width, uint16_t display_width, uint32_t current_tick,
                                 uint32_t interval_ms) {
            if (text_width <= display_width) {
                state.offset = 0;
                state.fully_displayed = true;
                return;
            }

            if (state.reset_pending) {
                state.offset = 0;
                state.fully_displayed = false;
                state.reset_pending = false;
                state.last_scroll_time = current_tick;
                return;
            }

            if (state.fully_displayed) {
                if (current_tick - state.last_scroll_time >= interval_ms) {
                    state.reset_pending = true;
                    state.last_scroll_time = current_tick;
                }
            }
            else {
                if (current_tick - state.last_scroll_time >= interval_ms) {
                    state.offset++;
                    state.last_scroll_time = current_tick;

                    if (state.offset >= text_width - display_width) {
                        state.fully_displayed = true;
                        state.last_scroll_time = current_tick;
                    }
                }
            }
        }

        void render_title(const BaseMenu& menu, const Render& render, MenuState&, uint32_t current_tick) {
            static ScrollState title_scroll_state;

            auto text_size = render.calculate_func(menu.title);
            uint16_t text_width = text_size.first;

            update_scroll_state(title_scroll_state, text_width, menu.w, current_tick, 2000);

            render.draw_rect_bg_func(menu.x, menu.y, menu.w, text_size.second, render.user_data);
            render.write_text_func(menu.title + title_scroll_state.offset, menu.x, menu.y, false, render.user_data);
        }

        uint16_t calculate_item_height(const Render& render) {
            auto size = render.calculate_func("A");
            return size.second;
        }

        void render_scrollbar(const BaseMenu& menu, const Render& render, uint16_t visible_items, uint32_t total_items,
                              uint32_t start_index) {
            if (total_items <= visible_items) {
                return;
            }

            uint16_t title_height = render.calculate_func(menu.title).second;
            uint16_t scrollbar_x = menu.x + menu.w - 6;
            uint16_t scrollbar_y = menu.y + title_height + 1;
            uint16_t scrollbar_height = menu.h - title_height - 1;

            render.draw_rect_func(scrollbar_x, scrollbar_y, 6, scrollbar_height, render.user_data);

            uint16_t thumb_height = (visible_items * (scrollbar_height - 2)) / total_items;
            if (thumb_height < 4) {
                thumb_height = 4;
            }

            uint16_t max_thumb_y = scrollbar_height - 2 - thumb_height;
            uint16_t max_start_index = total_items - visible_items;
            uint16_t thumb_y = scrollbar_y + 1 + (start_index * max_thumb_y) / max_start_index;

            render.draw_rect_bg_func(scrollbar_x + 1, thumb_y, 4, thumb_height, render.user_data);
        }

        uint32_t get_current_item_index(BaseMenu& menu) {
            if (auto* dynamic_menu = menu.asDynamicMenu()) {
                uint32_t index = 0;
                for (auto it = dynamic_menu->menu_list.begin(); it != dynamic_menu->menu_list.end(); ++it, ++index) {
                    if (it == dynamic_menu->current_item) {
                        return index;
                    }
                }
            }
            else if (auto* static_menu = menu.asStaticMenu()) {
                return static_menu->current_item - static_menu->menu_list;
            }
            return 0;
        }

        MenuCell* get_item_by_index(BaseMenu& menu, uint32_t index) {
            if (auto* dynamic_menu = menu.asDynamicMenu()) {
                uint32_t i = 0;
                for (auto& cell : dynamic_menu->menu_list) {
                    if (i == index) {
                        return cell;
                    }
                    i++;
                }
            }
            else if (auto* static_menu = menu.asStaticMenu()) {
                if (index < static_menu->num) {
                    return &static_menu->menu_list[index];
                }
            }
            return nullptr;
        }

        bool is_item_selected(BaseMenu& menu, const MenuCell* item) {
            return menu.get_current_item() == item;
        }

        struct RenderCache {
            MenuCell* last_selected_item = nullptr;
            uint32_t last_start_index = 0;
            uint32_t last_total_items = 0;
            bool needs_full_redraw = true;
        };

        void render_item(BaseMenu&, MenuCell* item, uint16_t x, uint16_t y, bool is_selected,
                         const Render& render, ScrollState& scroll_state, uint32_t current_tick, uint16_t list_width) {
            if (!item || !item->title) {
                return;
            }

            auto text_size = render.calculate_func(item->title);
            uint16_t text_width = text_size.first;

            if (is_selected) {
                update_scroll_state(scroll_state, text_width, list_width, current_tick, 2000);
            }

            render.draw_rect_bg_func(x, y, list_width, text_size.second, render.user_data);
            render.write_text_func(item->title + (is_selected ? scroll_state.offset : 0), x, y, is_selected,
                                   render.user_data);
        }

        void partial_redraw(BaseMenu& menu, const Render& render, uint16_t list_y, uint16_t item_height,
                            uint16_t list_width, uint32_t start_index, uint32_t visible_items,
                            MenuCell* old_selected, MenuCell* new_selected) {
            if (!old_selected || !new_selected) return;

            uint32_t old_index = 0;
            uint32_t new_index = 0;

            for (uint32_t i = 0; i < menu.get_item_count(); i++) {
                MenuCell* item = get_item_by_index(menu, i);
                if (item == old_selected) old_index = i;
                if (item == new_selected) new_index = i;
            }

            if (old_index >= start_index && old_index < start_index + visible_items) {
                uint16_t old_y = list_y + (old_index - start_index) * item_height;
                render.draw_rect_bg_func(menu.x, old_y, list_width, item_height, render.user_data);
                render.write_text_func(old_selected->title, menu.x, old_y, false, render.user_data);
            }

            if (new_index >= start_index && new_index < start_index + visible_items) {
                uint16_t new_y = list_y + (new_index - start_index) * item_height;
                render.draw_rect_bg_func(menu.x, new_y, list_width, item_height, render.user_data);
                render.write_text_func(new_selected->title, menu.x, new_y, true, render.user_data);
            }
        }

        void scroll_redraw(BaseMenu& menu, const Render& render, uint16_t list_y, uint16_t item_height,
                           uint16_t list_width, uint32_t old_start_index, uint32_t new_start_index,
                           uint32_t visible_items, RenderCache& cache) {
            if (render.copy_canvas && !cache.needs_full_redraw) {
                auto scroll_offset = (static_cast<int32_t>(old_start_index) - static_cast<int32_t>(new_start_index)) *
                    static_cast<int32_t>(item_height);

                if (scroll_offset != 0) {
                    uint16_t actual_visible_items = visible_items;
                    uint32_t total_items = menu.get_item_count();

                    if (new_start_index + visible_items > total_items) {
                        actual_visible_items = total_items - new_start_index;
                    }

                    uint16_t copy_height = actual_visible_items * item_height;

                    if (scroll_offset > 0) {
                        if (copy_height - scroll_offset > 0) {
                            render.copy_canvas(menu.x, list_y, list_width, copy_height - scroll_offset,
                                               menu.x, list_y + scroll_offset, render.user_data);
                        }

                        for (uint32_t i = new_start_index; i < old_start_index; i++) {
                            uint16_t y = list_y + (i - new_start_index) * item_height;
                            MenuCell* item = get_item_by_index(menu, i);
                            if (item) {
                                render.draw_rect_bg_func(menu.x, y, list_width, item_height, render.user_data);
                                render.write_text_func(item->title, menu.x, y, false, render.user_data);
                            }
                        }
                    }
                    else {
                        int32_t scroll_offset_abs = -scroll_offset;
                        uint16_t copy_height_abs = copy_height - scroll_offset_abs;
                        if (copy_height_abs > 0) {
                            MenuCell* item;
                            if (menu.asDynamicMenu()) {
                                item = *std::prev(menu.asDynamicMenu()->current_item);
                            }
                            else {
                                item = menu.asStaticMenu()->current_item - 1;
                            }
                            render.write_text_func(item->title, 0, list_y + item_height * (visible_items - 1), false,
                                                   render.user_data);
                            render.copy_canvas(menu.x, list_y + scroll_offset_abs, list_width, copy_height_abs,
                                               menu.x, list_y, render.user_data);
                        }

                        for (uint32_t i = old_start_index + visible_items; i < new_start_index + visible_items; i++) {
                            if (i >= total_items) break;
                            uint16_t y = list_y + (i - new_start_index) * item_height;
                            MenuCell* item = get_item_by_index(menu, i);
                            if (item) {
                                render.draw_rect_bg_func(menu.x, y, list_width, item_height, render.user_data);
                                render.write_text_func(item->title, menu.x, y, false, render.user_data);
                            }
                        }
                    }

                    MenuCell* current_item = menu.get_current_item();
                    if (current_item) {
                        uint32_t current_index = get_current_item_index(menu);
                        if (current_index >= new_start_index && current_index < new_start_index + visible_items) {
                            uint16_t y = list_y + (current_index - new_start_index) * item_height;
                            render.draw_rect_bg_func(menu.x, y, list_width, item_height, render.user_data);
                            render.write_text_func(current_item->title, menu.x, y, true, render.user_data);
                        }
                    }

                    if (actual_visible_items < visible_items) {
                        uint16_t empty_start_y = list_y + actual_visible_items * item_height;
                        uint16_t empty_height = (visible_items - actual_visible_items) * item_height;
                        render.draw_rect_bg_func(menu.x, empty_start_y, list_width, empty_height, render.user_data);
                    }

                    return;
                }
            }

            cache.needs_full_redraw = true;
        }
    }

    bool flush_menu(DynamicMenu& menu, volatile InputEvent& input, const Render& render, MenuState& state) {
        BaseMenu* active_menu = menu.current_menu ? menu.current_menu : &menu;
        uint32_t current_tick = render.get_tick_func();

        if (!state.initialized) {
            state.initialized = true;
            state.title_needs_update = true;
            state.current_item_needs_update = true;
        }

        uint16_t item_height = calculate_item_height(render);
        uint16_t title_height = render.calculate_func(active_menu->title).second;
        uint16_t list_y = active_menu->y + title_height + 1;
        uint16_t list_h = active_menu->h - title_height - 1;
        uint16_t visible_items = list_h / item_height;

        static RenderCache cache;
        static ScrollState item_scroll_state;

        uint32_t total_items = active_menu->get_item_count();
        uint32_t current_index = get_current_item_index(*active_menu);
        MenuCell* current_item = active_menu->get_current_item();

        uint32_t start_index = 0;
        if (current_index >= visible_items) {
            start_index = current_index - visible_items + 1;
        }

        uint16_t list_width = active_menu->w - 6;

        bool selection_changed = (current_item != cache.last_selected_item);
        bool scroll_changed = (start_index != cache.last_start_index);
        bool menu_changed = (total_items != cache.last_total_items);

        if (cache.needs_full_redraw || menu_changed || active_menu->force_redraw_flag) {
            render.draw_rect_bg_func(active_menu->x, active_menu->y, active_menu->w, active_menu->h, render.user_data);
            render_title(*active_menu, render, state, current_tick);

            for (uint32_t i = start_index; i < total_items; i++) {
                uint16_t item_y = list_y + (i - start_index) * item_height;

                if (item_y + item_height > active_menu->y + active_menu->h) {
                    break;
                }

                MenuCell* item = get_item_by_index(*active_menu, i);
                bool is_selected = is_item_selected(*active_menu, item);
                render_item(*active_menu, item, active_menu->x, item_y, is_selected, render, item_scroll_state,
                            current_tick, list_width);
            }

            cache.needs_full_redraw = false;
            active_menu->force_redraw_flag = false;
        }
        else if (scroll_changed) {
            scroll_redraw(*active_menu, render, list_y, item_height, list_width, cache.last_start_index, start_index,
                          visible_items, cache);

            if (cache.needs_full_redraw) {
                render.draw_rect_bg_func(active_menu->x, active_menu->y, active_menu->w, active_menu->h,
                                         render.user_data);
                render_title(*active_menu, render, state, current_tick);

                for (uint32_t i = start_index; i < total_items; i++) {
                    uint16_t item_y = list_y + (i - start_index) * item_height;

                    if (item_y + item_height > active_menu->y + active_menu->h) {
                        break;
                    }

                    MenuCell* item = get_item_by_index(*active_menu, i);
                    bool is_selected = is_item_selected(*active_menu, item);
                    render_item(*active_menu, item, active_menu->x, item_y, is_selected, render, item_scroll_state,
                                current_tick, list_width);
                }

                cache.needs_full_redraw = false;
            }
        }
        else if (selection_changed) {
            partial_redraw(*active_menu, render, list_y, item_height, list_width, start_index, visible_items,
                           cache.last_selected_item, current_item);
        }

        render_scrollbar(*active_menu, render, visible_items, total_items, start_index);

        if (input.up) {
            active_menu->move_up();
            input.up = false;
        }
        if (input.down) {
            active_menu->move_down();
            input.down = false;
        }
        if (input.enter) {
            input.enter = false;
            if (MenuCell* selected = active_menu->get_current_item()) {
                if (selected->action == SUB_DYNAMIC_MENU && selected->action_data.sub_dynamic_menu) {
                    selected->action_data.sub_dynamic_menu->parent_menu = active_menu;
                    active_menu->current_menu = selected->action_data.sub_dynamic_menu;
                    cache.needs_full_redraw = true;
                }
                else if (selected->action == SUB_STATIC_MENU && selected->action_data.sub_static_menu) {
                    selected->action_data.sub_static_menu->parent_menu = active_menu;
                    active_menu->current_menu = selected->action_data.sub_static_menu;
                    cache.needs_full_redraw = true;
                }
                else if (selected->action == FUNCTION && selected->action_data.callback_func) {
                    selected->action_data.callback_func(selected, ENTER, selected->user_data);
                }
            }
        }
        if (input.shift) {
            input.shift = false;
            MenuCell* selected = active_menu->get_current_item();
            if (selected && selected->action == FUNCTION && selected->action_data.callback_func) {
                selected->action_data.callback_func(selected, SHIFT, selected->user_data);
            }
            else if (active_menu->current_menu) {
                active_menu->current_menu = active_menu->current_menu->parent_menu;
                cache.needs_full_redraw = true;
            }
        }
        if (input.break_out) {
            input.break_out = false;
            if (active_menu->parent_menu) {
                menu.current_menu = active_menu->current_menu = active_menu->parent_menu;
            }
            else return false;
        }

        cache.last_selected_item = current_item;
        cache.last_start_index = start_index;
        cache.last_total_items = total_items;

        render.display_canvas(menu.x, menu.y, render.user_data);

        return true;
    }

    bool flush_menu(StaticMenu& menu, volatile InputEvent& input, const Render& render, MenuState& state) {
        BaseMenu* active_menu = menu.current_menu ? menu.current_menu : &menu;
        uint32_t current_tick = render.get_tick_func();

        if (!state.initialized) {
            state.initialized = true;
            state.title_needs_update = true;
            state.current_item_needs_update = true;
        }

        uint16_t item_height = calculate_item_height(render);
        uint16_t title_height = render.calculate_func(active_menu->title).second;
        uint16_t list_y = active_menu->y + title_height + 1;
        uint16_t list_h = active_menu->h - title_height - 1;
        uint16_t visible_items = list_h / item_height;

        static RenderCache cache;
        static ScrollState item_scroll_state;

        uint32_t total_items = active_menu->get_item_count();
        uint32_t current_index = get_current_item_index(*active_menu);
        MenuCell* current_item = active_menu->get_current_item();

        uint32_t start_index = 0;
        if (current_index >= visible_items) {
            start_index = current_index - visible_items + 1;
        }

        uint16_t list_width = active_menu->w - 6;

        bool selection_changed = (current_item != cache.last_selected_item);
        bool scroll_changed = (start_index != cache.last_start_index);
        bool menu_changed = (total_items != cache.last_total_items);

        if (cache.needs_full_redraw || menu_changed || active_menu->force_redraw_flag) {
            render.draw_rect_bg_func(active_menu->x, active_menu->y, active_menu->w, active_menu->h, render.user_data);
            render_title(*active_menu, render, state, current_tick);

            for (uint32_t i = start_index; i < total_items; i++) {
                uint16_t item_y = list_y + (i - start_index) * item_height;

                if (item_y + item_height > active_menu->y + active_menu->h) {
                    break;
                }

                MenuCell* item = get_item_by_index(*active_menu, i);
                bool is_selected = is_item_selected(*active_menu, item);
                render_item(*active_menu, item, active_menu->x, item_y, is_selected, render, item_scroll_state,
                            current_tick, list_width);
            }

            cache.needs_full_redraw = false;
            active_menu->force_redraw_flag = false;
        }
        else if (scroll_changed) {
            scroll_redraw(*active_menu, render, list_y, item_height, list_width, cache.last_start_index, start_index,
                          visible_items, cache);

            if (cache.needs_full_redraw) {
                render.draw_rect_bg_func(active_menu->x, active_menu->y, active_menu->w, active_menu->h,
                                         render.user_data);
                render_title(*active_menu, render, state, current_tick);

                for (uint32_t i = start_index; i < total_items; i++) {
                    uint16_t item_y = list_y + (i - start_index) * item_height;

                    if (item_y + item_height > active_menu->y + active_menu->h) {
                        break;
                    }

                    MenuCell* item = get_item_by_index(*active_menu, i);
                    bool is_selected = is_item_selected(*active_menu, item);
                    render_item(*active_menu, item, active_menu->x, item_y, is_selected, render, item_scroll_state,
                                current_tick, list_width);
                }

                cache.needs_full_redraw = false;
            }
        }
        else if (selection_changed) {
            partial_redraw(*active_menu, render, list_y, item_height, list_width, start_index, visible_items,
                           cache.last_selected_item, current_item);
        }

        render_scrollbar(*active_menu, render, visible_items, total_items, start_index);

        if (input.up) {
            active_menu->move_up();
            input.up = false;
        }
        if (input.down) {
            active_menu->move_down();
            input.down = false;
        }
        if (input.enter) {
            input.enter = false;
            if (MenuCell* selected = active_menu->get_current_item()) {
                if (selected->action == SUB_DYNAMIC_MENU && selected->action_data.sub_dynamic_menu) {
                    selected->action_data.sub_dynamic_menu->parent_menu = active_menu;
                    active_menu->current_menu = selected->action_data.sub_dynamic_menu;
                    cache.needs_full_redraw = true;
                }
                else if (selected->action == SUB_STATIC_MENU && selected->action_data.sub_static_menu) {
                    selected->action_data.sub_static_menu->parent_menu = active_menu;
                    active_menu->current_menu = selected->action_data.sub_static_menu;
                    cache.needs_full_redraw = true;
                }
                else if (selected->action == FUNCTION && selected->action_data.callback_func) {
                    selected->action_data.callback_func(selected, ENTER, selected->user_data);
                }
            }
        }
        if (input.shift) {
            input.shift = false;
            MenuCell* selected = active_menu->get_current_item();
            if (selected && selected->action == FUNCTION && selected->action_data.callback_func) {
                selected->action_data.callback_func(selected, SHIFT, selected->user_data);
            }
            else if (active_menu->current_menu) {
                active_menu->current_menu = active_menu->current_menu->parent_menu;
                cache.needs_full_redraw = true;
            }
        }
        if (input.break_out) {
            input.break_out = false;
            if (active_menu->parent_menu) {
                menu.current_menu = active_menu->current_menu = active_menu->parent_menu;
            }
            else return false;
        }

        cache.last_selected_item = current_item;
        cache.last_start_index = start_index;
        cache.last_total_items = total_items;

        render.display_canvas(menu.x, menu.y, render.user_data);

        return true;
    }

    void menu_mainloop(DynamicMenu& menu, volatile InputEvent& input, const Render& render) {
        MenuState state;
        while (flush_menu(menu, input, render, state)) {
        }
    }

    void menu_mainloop(StaticMenu& menu, volatile InputEvent& input, const Render& render) {
        MenuState state;
        while (flush_menu(menu, input, render, state)) {
        }
    }
}
