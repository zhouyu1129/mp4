//
// Created by 周煜 on 2026/2/21.
//

#ifndef SD_AND_LCD2_EASY_MENU_H
#define SD_AND_LCD2_EASY_MENU_H
#include <list>
#include <cstdint>
#include <iterator>

#define ENABLE_SUBMENU_AUTO_DESTROY true
// 是否允许自动销毁子菜单

#define DOUBLE_CLICK_INTERVAL_MS 300
#define LONG_PRESS_THRESHOLD_MS 500

namespace easy_menu {
    using std::list;
    using std::pair;

    class BaseMenu;
    class DynamicMenu;
    class StaticMenu;
    class MenuCell;

    enum ActionType {
        SUB_DYNAMIC_MENU,
        SUB_STATIC_MENU,
        FUNCTION,
    };

    enum ClickType {
        ENTER,
        SHIFT
    };

    using MenuCallback = void(*)(const MenuCell* sender, ClickType type, void* user_data); // 允许为回调函数提供上下文信息
    using WriteText = void(*)(const char* str, uint16_t x, uint16_t y, bool color_inversion, void* user_data);
    // 绘制文本的函数，被选中的项使用颜色反转高亮显示
    using DrawRectangle = void(*)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, void* user_data); // 绘制实心矩形的函数
    using DrawRectangleBg = void(*)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, void* user_data); // 绘制背景色实心矩形的函数
    using DisplayCanvas = void(*)(uint16_t x, uint16_t y, void* user_data); // 渲染画布的函数
    using CalculateTextSize = pair<uint16_t, uint16_t>(*)(const char* str); // 计算字符串宽x高的函数
    using CopyCanvas = void(*)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t x0, uint16_t y0,
                               void* user_data); // 复制画布区域的函数，将左上角x, y宽高w, h的区域复制到x0, y0
    using GetTick_ms = uint32_t(*)();

    struct Render {
        WriteText write_text_func;
        DrawRectangle draw_rect_func;
        DrawRectangleBg draw_rect_bg_func;
        CalculateTextSize calculate_func;
        DisplayCanvas display_canvas;
        CopyCanvas copy_canvas; // 此函数允许为nullptr，此时菜单渲染会退回到基本渲染模式，不使用区域复制加速
        GetTick_ms get_tick_func;
        void* user_data;
    };

    class BaseMenu {
    public:
        const char* title;
        uint16_t x, y, w, h;
        BaseMenu* parent_menu = nullptr;
        BaseMenu* current_menu = nullptr;
        bool force_redraw_flag = false; // 强制重绘标志

        BaseMenu(const char* title, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
            : title(title), x(x), y(y), w(w), h(h) {
        }

        virtual ~BaseMenu() = default;

        [[nodiscard]] virtual uint32_t get_item_count() const = 0;
        virtual MenuCell* get_current_item() = 0;
        virtual void move_up() = 0;
        virtual void move_down() = 0;
        [[nodiscard]] virtual bool can_move_up() const = 0;
        [[nodiscard]] virtual bool can_move_down() const = 0;
        virtual void reset_selection() = 0;
        void set_to_home() { while (current_menu) current_menu = current_menu->parent_menu; }
        virtual DynamicMenu* asDynamicMenu() { return nullptr; }
        virtual StaticMenu* asStaticMenu() { return nullptr; }
        virtual void force_redraw() { force_redraw_flag = true; } // 强制全部重绘，认为画布上的所有缓存都作废
    };

    class MenuCell {
    public:
        const char* title;
        ActionType action; // 菜单项的调用可以是回调函数，也可以是子菜单
        union Action {
            MenuCallback callback_func;
            StaticMenu* sub_static_menu;
            DynamicMenu* sub_dynamic_menu;
        } action_data{};

        void* user_data;
        MenuCell(const char* title, MenuCallback callback_func, void* user_data = nullptr);
        MenuCell(const char* title, DynamicMenu* sub_menu);
        MenuCell(const char* title, StaticMenu* sub_menu);

        MenuCell() : MenuCell(nullptr, nullptr, nullptr) {
        }

        ~MenuCell();
    };

    class DynamicMenu : public BaseMenu {
        // 菜单列表可动态扩展
    public:
        list<MenuCell*> menu_list;
        list<MenuCell*>::iterator current_item; // 当前选中的项
        void add_menu(const char* title, MenuCallback, void* user_data = nullptr);
        void add_menu(const char* title, StaticMenu* sub_menu);
        void add_menu(const char* title, DynamicMenu* sub_menu);
        void erase_menu(list<MenuCell*>::iterator cell_it);

        DynamicMenu(const char* title, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
            : BaseMenu(title, x, y, w, h) {
            current_item = menu_list.begin();
        };
        ~DynamicMenu() override;
        DynamicMenu* asDynamicMenu() override { return this; }

        [[nodiscard]] uint32_t get_item_count() const override;
        MenuCell* get_current_item() override;
        void move_up() override;
        void move_down() override;
        [[nodiscard]] bool can_move_up() const override;
        [[nodiscard]] bool can_move_down() const override;
        void reset_selection() override;
    };

    class StaticMenu : public BaseMenu {
        // 菜单列表不可动态扩展
    public:
        MenuCell* menu_list;
        uint32_t length, num = 0;
        bool auto_delete = true;
        MenuCell* current_item; // 当前选中的项
        void add_menu(const char* title, MenuCallback, void* user_data = nullptr);
        void add_menu(const char* title, StaticMenu* sub_menu);
        void add_menu(const char* title, DynamicMenu* sub_menu);
        void erase_menu(MenuCell* cell);
        // 用户仅给出长度时列表空间由对象自行管理，否则由用户管理
        StaticMenu(const uint32_t length, const char* title, const uint16_t x, const uint16_t y, const uint16_t w,
                   const uint16_t h) : BaseMenu(title, x, y, w, h), length(length) {
            current_item = menu_list = new MenuCell[length];
        }

        StaticMenu(MenuCell* menu_list, const uint32_t length, const char* title, const uint16_t x, const uint16_t y,
                   const uint16_t w, const uint16_t h) : BaseMenu(title, x, y, w, h), menu_list(menu_list),
                                                         length(length), auto_delete(false),
                                                         current_item(menu_list) {
        }

        ~StaticMenu() override;
        StaticMenu* asStaticMenu() override { return this; }

        [[nodiscard]] uint32_t get_item_count() const override;
        MenuCell* get_current_item() override;
        void move_up() override;
        void move_down() override;
        [[nodiscard]] bool can_move_up() const override;
        [[nodiscard]] bool can_move_down() const override;
        void reset_selection() override;
    };

    struct InputEvent {
        bool enter, shift, up, down, break_out; // 确认、菜单、向上、向下、返回
    };

    struct MenuState {
        bool initialized = false;
        bool title_needs_update = false;
        bool current_item_needs_update = false;
        uint16_t title_offset = 0, current_item_offset = 0;
    };

    // 使用由用户提供的WriteWords和DrawRectangle实现菜单UI和滚动条
    // 通过用户提供的中断事件判断是单击、双击还是长按，通过up, down进行上下滑动，通过break_out退出
    void menu_mainloop(DynamicMenu& menu, volatile InputEvent& input, const Render& render);
    void menu_mainloop(StaticMenu& menu, volatile InputEvent& input, const Render& render);

    // 非阻塞轮询刷新菜单界面
    // 返回值：true 表示菜单仍在运行，false 表示菜单已退出（break_out 为 true）
    bool flush_menu(DynamicMenu& menu, volatile InputEvent& input, const Render& render, MenuState& state);
    bool flush_menu(StaticMenu& menu, volatile InputEvent& input, const Render& render, MenuState& state);
}

#endif //SD_AND_LCD2_EASY_MENU_H
