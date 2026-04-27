#include "ui_app.h"

#include <stdio.h>
#include <string.h>

#include "imu_cube_ui.h"

namespace {
constexpr uint32_t kDoubleClickMs = 320;
constexpr int kPageW = 240;
constexpr int kPageH = 240;
constexpr int kHomeItemCount = 3;
constexpr int kSettingsItemCount = 3;
constexpr int kSettingsBackIndex = kSettingsItemCount;
constexpr int kSettingsSelectableCount = kSettingsItemCount + 1;

constexpr uint32_t kColBg          = 0x000000;
constexpr uint32_t kColPrimary     = 0xFFFFFF;
constexpr uint32_t kColDim         = 0x9C9C9C;
constexpr uint32_t kColFaint       = 0x4A4A4A;
constexpr uint32_t kColDivider     = 0x3A3A3A;
constexpr uint32_t kColAccent      = 0xFF7A2E;

enum class PageId {
    Home,
    Imu,
    Settings,
    Info,
};

struct HomeItem {
    const char *idx;
    const char *title;
    const char *subtitle;
    PageId target;
};

const HomeItem kHomeItems[kHomeItemCount] = {
    {"01", "IMU",      "MOTION TRACKING", PageId::Imu},
    {"02", "SETTINGS", "DEVICE",          PageId::Settings},
    {"03", "INFO",     "AUTHOR",          PageId::Info},
};

struct UiApp {
    lv_obj_t *root;
    lv_obj_t *page;
    PageId stack[6];
    int stack_depth;
    PageId current;
    int home_selected;
    int settings_selected;
    bool settings_editing;
    bool back_selected;
    uint32_t last_release_ms;
    ui_app_settings_t settings;
    ui_app_config_t cfg;

    lv_obj_t *home_indicators[kHomeItemCount];
    lv_obj_t *home_idx[kHomeItemCount];
    lv_obj_t *home_title[kHomeItemCount];
    lv_obj_t *home_subtitle[kHomeItemCount];

    lv_obj_t *settings_label[kSettingsItemCount];
    lv_obj_t *settings_value[kSettingsItemCount];
    lv_obj_t *settings_track[kSettingsItemCount];
    lv_obj_t *settings_fill[kSettingsItemCount];
    lv_obj_t *settings_marker[kSettingsItemCount];

    lv_obj_t *back_hint;
};

UiApp s_app = {};

void pop_page();

lv_color_t c(uint32_t hex)
{
    return lv_color_hex(hex);
}

int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void clear_page_refs()
{
    memset(s_app.home_indicators, 0, sizeof(s_app.home_indicators));
    memset(s_app.home_idx,        0, sizeof(s_app.home_idx));
    memset(s_app.home_title,      0, sizeof(s_app.home_title));
    memset(s_app.home_subtitle,   0, sizeof(s_app.home_subtitle));
    memset(s_app.settings_label,  0, sizeof(s_app.settings_label));
    memset(s_app.settings_value,  0, sizeof(s_app.settings_value));
    memset(s_app.settings_track,  0, sizeof(s_app.settings_track));
    memset(s_app.settings_fill,   0, sizeof(s_app.settings_fill));
    memset(s_app.settings_marker, 0, sizeof(s_app.settings_marker));
    s_app.back_hint = nullptr;
}

void style_screen(lv_obj_t *obj)
{
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, c(kColBg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *make_text(lv_obj_t *parent, const char *text, int x, int y,
                    const lv_font_t *font, uint32_t color, lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c(color), 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    return label;
}

lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, lv_opa_t opa = LV_OPA_COVER)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_bg_color(r, c(color), 0);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_outline_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_shadow_width(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

void make_status_bar(lv_obj_t *page, const char *label, int idx_one_based)
{
    make_text(page, label, 14, 14, &lv_font_montserrat_12, kColPrimary);

    char idx_text[12];
    snprintf(idx_text, sizeof(idx_text), "%02d/%02d", idx_one_based, kHomeItemCount);
    lv_obj_t *l = make_text(page, idx_text, 14, 14, &lv_font_montserrat_12, kColDim);
    lv_obj_set_width(l, 212);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);

    make_rect(page, 14, 34, 212, 2, kColDivider);
}

void make_footer_divider(lv_obj_t *page)
{
    make_rect(page, 14, 206, 212, 2, kColDivider);
}

void animate_x(lv_obj_t *obj, int from, int to, uint32_t time_ms)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_time(&anim, time_ms);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, [](void *target, int32_t value) {
        lv_obj_set_x((lv_obj_t *)target, value);
    });
    lv_anim_start(&anim);
}

void notify_settings()
{
    if (s_app.cfg.settings_changed_cb != nullptr) {
        s_app.cfg.settings_changed_cb(&s_app.settings, s_app.cfg.user_ctx);
    }
}

void notify_imu_active(bool active)
{
    if (s_app.cfg.imu_active_changed_cb != nullptr) {
        s_app.cfg.imu_active_changed_cb(active, s_app.cfg.user_ctx);
    }
}

void apply_home_selection()
{
    for (int i = 0; i < kHomeItemCount; ++i) {
        if (s_app.home_indicators[i] == nullptr) continue;
        bool selected = (i == s_app.home_selected);

        lv_obj_set_style_bg_color(s_app.home_indicators[i],
                                  c(selected ? kColAccent : kColFaint), 0);
        lv_obj_set_style_text_color(s_app.home_idx[i],
                                    c(selected ? kColPrimary : kColDim), 0);
        lv_obj_set_style_text_color(s_app.home_title[i],
                                    c(selected ? kColPrimary : kColDim), 0);
        lv_obj_set_style_text_color(s_app.home_subtitle[i],
                                    c(selected ? kColDim : kColFaint), 0);
    }
}

void select_home(int index)
{
    s_app.home_selected = clamp_int(index, 0, kHomeItemCount - 1);
    apply_home_selection();
}

void update_setting_row(int index)
{
    int value = 0;
    switch (index) {
    case 0: value = s_app.settings.backlight_percent; break;
    case 1: value = s_app.settings.volume_percent; break;
    default: value = s_app.settings.haptic_percent; break;
    }

    if (s_app.settings_value[index] != nullptr) {
        char text[8];
        snprintf(text, sizeof(text), "%d", value);
        lv_label_set_text(s_app.settings_value[index], text);
    }

    if (s_app.settings_fill[index] != nullptr) {
        constexpr int track_w = 140;
        int w = clamp_int(value * track_w / 100, 0, track_w);
        lv_obj_set_width(s_app.settings_fill[index], w);
    }
}

void apply_settings_selection()
{
    const bool back_selected = s_app.settings_selected == kSettingsBackIndex;

    for (int i = 0; i < kSettingsItemCount; ++i) {
        if (s_app.settings_label[i] == nullptr) continue;
        bool selected = (i == s_app.settings_selected);
        bool editing = selected && s_app.settings_editing;

        uint32_t label_col = selected ? kColPrimary : kColDim;
        uint32_t value_col = editing ? kColAccent : (selected ? kColPrimary : kColDim);
        uint32_t fill_col  = editing ? kColAccent : (selected ? kColPrimary : kColDim);
        uint32_t marker_col = editing ? kColAccent : (selected ? kColPrimary : kColFaint);

        lv_obj_set_style_text_color(s_app.settings_label[i], c(label_col), 0);
        lv_obj_set_style_text_color(s_app.settings_value[i], c(value_col), 0);
        lv_obj_set_style_bg_color(s_app.settings_fill[i], c(fill_col), 0);
        lv_obj_set_style_bg_color(s_app.settings_marker[i], c(marker_col), 0);
    }

    if (s_app.back_hint != nullptr) {
        lv_obj_set_style_text_color(s_app.back_hint,
                                    c(back_selected ? kColAccent : kColDim), 0);
    }

    s_app.back_selected = back_selected;
}

void select_setting(int index)
{
    s_app.settings_selected = clamp_int(index, 0, kSettingsSelectableCount - 1);
    apply_settings_selection();
}

void build_home(lv_obj_t *page)
{
    make_status_bar(page, "NEOPEAK", 1);

    constexpr int row_h = 52;
    constexpr int row_top = 50;

    for (int i = 0; i < kHomeItemCount; ++i) {
        int y = row_top + i * row_h;

        s_app.home_indicators[i] = make_rect(page, 14, y + 4, 4, 32, kColFaint);

        s_app.home_idx[i] = make_text(page, kHomeItems[i].idx, 28, y + 2,
                                      &lv_font_montserrat_12, kColDim);
        s_app.home_title[i] = make_text(page, kHomeItems[i].title, 56, y,
                                        &lv_font_montserrat_14, kColPrimary);
        s_app.home_subtitle[i] = make_text(page, kHomeItems[i].subtitle, 56, y + 22,
                                           &lv_font_montserrat_12, kColDim);
    }

    make_footer_divider(page);
    make_text(page, "ROTATE  /  PRESS", 14, 216, &lv_font_montserrat_12, kColDim);

    apply_home_selection();
}

void build_settings(lv_obj_t *page)
{
    make_status_bar(page, "SETTINGS", 2);

    const char *names[kSettingsItemCount] = {"BACKLIGHT", "VOLUME", "HAPTIC"};
    constexpr int row_top = 52;
    constexpr int row_h = 50;
    constexpr int track_w = 140;

    for (int i = 0; i < kSettingsItemCount; ++i) {
        int y = row_top + i * row_h;

        s_app.settings_marker[i] = make_rect(page, 14, y + 6, 2, 14, kColFaint);

        s_app.settings_label[i] = make_text(page, names[i], 24, y + 4,
                                            &lv_font_montserrat_12, kColDim);

        s_app.settings_value[i] = make_text(page, "0", 24, y + 18,
                                            &lv_font_montserrat_24, kColPrimary);
        lv_obj_set_width(s_app.settings_value[i], 60);

        s_app.settings_track[i] = make_rect(page, 84, y + 32, track_w, 1, kColFaint);
        s_app.settings_fill[i]  = make_rect(page, 84, y + 31, 0, 3, kColPrimary);

        update_setting_row(i);
    }

    make_footer_divider(page);
    s_app.back_hint = make_text(page, LV_SYMBOL_LEFT "  BACK", 14, 216,
                                &lv_font_montserrat_12, kColDim);

    apply_settings_selection();
}

void build_info(lv_obj_t *page)
{
    make_status_bar(page, "INFO", 3);

    constexpr int x = 14;
    int y = 52;

    make_text(page, "AUTHOR", x, y, &lv_font_montserrat_12, kColDim);
    make_text(page, "ALFRED ZHANG", x, y + 14, &lv_font_montserrat_14, kColPrimary);

    y += 46;
    make_text(page, "ROLE", x, y, &lv_font_montserrat_12, kColDim);
    make_text(page, "PHD  /  UCL", x, y + 14, &lv_font_montserrat_14, kColPrimary);

    y += 46;
    make_text(page, "CONTACT", x, y, &lv_font_montserrat_12, kColDim);
    make_text(page, "qingyu.zhang.23", x, y + 14, &lv_font_montserrat_12, kColPrimary);
    make_text(page, "@ucl.ac.uk",      x, y + 30, &lv_font_montserrat_12, kColPrimary);

    make_footer_divider(page);
    s_app.back_hint = make_text(page, LV_SYMBOL_LEFT "  BACK", 14, 216,
                                &lv_font_montserrat_12, kColAccent);
    s_app.back_selected = true;
}

void build_imu(lv_obj_t *page)
{
    imu_cube_ui_create(page);
    s_app.back_hint = make_text(page, LV_SYMBOL_LEFT "  BACK", 14, 216,
                                &lv_font_montserrat_12, kColDim);
}

void show_page(PageId page_id, bool push)
{
    if (s_app.root == nullptr) {
        return;
    }

    if (push && s_app.stack_depth < (int)(sizeof(s_app.stack) / sizeof(s_app.stack[0]))) {
        s_app.stack[s_app.stack_depth++] = s_app.current;
    }

    PageId old_page_id = s_app.current;
    lv_obj_t *old_page = s_app.page;
    clear_page_refs();

    lv_obj_t *page = lv_obj_create(s_app.root);
    lv_obj_set_size(page, kPageW, kPageH);
    lv_obj_set_pos(page, old_page != nullptr ? kPageW : 0, 0);
    style_screen(page);
    s_app.page = page;
    s_app.current = page_id;
    s_app.settings_editing = false;
    s_app.back_selected = false;

    if (old_page != nullptr && old_page_id == PageId::Imu && page_id != PageId::Imu) {
        notify_imu_active(false);
    }

    switch (page_id) {
    case PageId::Home:     build_home(page); break;
    case PageId::Imu:      build_imu(page); break;
    case PageId::Settings: build_settings(page); break;
    case PageId::Info:     build_info(page); break;
    }

    if (page_id == PageId::Imu && old_page_id != PageId::Imu) {
        notify_imu_active(true);
        imu_cube_ui_reset();
    }

    if (old_page != nullptr) {
        animate_x(page, kPageW, 0, 160);
        animate_x(old_page, 0, -kPageW, 160);
        lv_obj_delete_delayed(old_page, 200);
    }
}

void pop_page()
{
    if (s_app.stack_depth <= 0) {
        if (s_app.current != PageId::Home) {
            show_page(PageId::Home, false);
        }
        return;
    }

    PageId previous = s_app.stack[--s_app.stack_depth];
    show_page(previous, false);
}

void handle_single_click()
{
    switch (s_app.current) {
    case PageId::Home:
        show_page(kHomeItems[s_app.home_selected].target, true);
        break;
    case PageId::Imu:
        if (imu_cube_ui_consume_click()) {
            // Calibration overlay just dismissed; click is consumed.
            break;
        }
        if (s_app.back_selected) {
            pop_page();
        } else {
            imu_cube_ui_reset();
        }
        break;
    case PageId::Settings:
        if (s_app.settings_selected == kSettingsBackIndex) {
            pop_page();
        } else {
            s_app.settings_editing = !s_app.settings_editing;
            apply_settings_selection();
        }
        break;
    case PageId::Info:
        pop_page();
        break;
    }
}

void adjust_setting(int delta)
{
    uint8_t *target = nullptr;
    switch (s_app.settings_selected) {
    case 0: target = &s_app.settings.backlight_percent; break;
    case 1: target = &s_app.settings.volume_percent; break;
    case 2: target = &s_app.settings.haptic_percent; break;
    default: break;
    }

    if (target == nullptr) return;

    int value = (int)(*target) + delta * 5;
    *target = (uint8_t)clamp_int(value, 0, 100);
    update_setting_row(s_app.settings_selected);
    notify_settings();
}

void set_back_hint_state(bool selected)
{
    s_app.back_selected = selected;
    if (s_app.back_hint != nullptr) {
        lv_obj_set_style_text_color(s_app.back_hint,
                                    c(selected ? kColAccent : kColDim), 0);
    }
}
} // namespace

void ui_app_create(lv_obj_t *root, const ui_app_config_t *cfg)
{
    memset(&s_app, 0, sizeof(s_app));
    s_app.root = root;
    s_app.current = PageId::Home;
    s_app.settings.backlight_percent = 15;
    s_app.settings.volume_percent = 30;
    s_app.settings.haptic_percent = 0;
    if (cfg != nullptr) {
        s_app.cfg = *cfg;
    }

    style_screen(root);
    notify_settings();
    notify_imu_active(false);
    show_page(PageId::Home, false);
    lv_obj_invalidate(root);
}

void ui_app_handle_encoder_rotate(int8_t delta)
{
    if (delta == 0 || s_app.root == nullptr) {
        return;
    }

    int step = (delta > 0) ? 1 : -1;
    switch (s_app.current) {
    case PageId::Home:
        select_home(s_app.home_selected + step);
        break;
    case PageId::Imu:
        set_back_hint_state(!s_app.back_selected);
        break;
    case PageId::Settings:
        if (s_app.settings_editing) {
            adjust_setting(step);
        } else {
            select_setting(s_app.settings_selected + step);
        }
        break;
    default:
        break;
    }
}

void ui_app_handle_encoder_button(bool pressed, uint32_t timestamp_ms)
{
    if (pressed || s_app.root == nullptr) {
        return;
    }

    bool double_click = s_app.last_release_ms != 0 && (timestamp_ms - s_app.last_release_ms) <= kDoubleClickMs;
    s_app.last_release_ms = timestamp_ms;

    if (double_click) {
        s_app.last_release_ms = 0;
        pop_page();
        return;
    }

    handle_single_click();
}

void ui_app_handle_imu(const float q[4], uint8_t calib_percent,
                       const float gyro_bias_rad_s[3])
{
    if (s_app.current == PageId::Imu) {
        imu_cube_ui_update(q, calib_percent, gyro_bias_rad_s);
    }
}
