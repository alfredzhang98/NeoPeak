#include "imu_cube_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr int kDefaultWidth = 240;
constexpr int kDefaultHeight = 240;
constexpr int kVertexCount = 8;
constexpr int kEdgeCount = 12;

constexpr uint32_t kColBg       = 0x000000;
constexpr uint32_t kColPrimary  = 0xFFFFFF;
constexpr uint32_t kColDim      = 0x9C9C9C;
constexpr uint32_t kColFaint    = 0x4A4A4A;
constexpr uint32_t kColDivider  = 0x3A3A3A;
constexpr uint32_t kColAccent   = 0xFF7A2E;

constexpr float kRadToDeg = 57.2957795f;

const int kEdges[kEdgeCount][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

const float kVertices[kVertexCount][3] = {
    {-1.0f, -1.0f, -1.0f},
    { 1.0f, -1.0f, -1.0f},
    { 1.0f,  1.0f, -1.0f},
    {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f},
    { 1.0f, -1.0f,  1.0f},
    { 1.0f,  1.0f,  1.0f},
    {-1.0f,  1.0f,  1.0f},
};

enum class State {
    kCalibrating,    // bias calibration in progress, prompt user to hold still
    kAwaitConfirm,   // calibration done, waiting for user to dismiss the dialog
    kRunning,        // live cube view, IMU drives the rotation
};

struct ImuCubeUi {
    lv_obj_t *root;

    // cube view
    lv_obj_t *edge[kEdgeCount];
    lv_point_precise_t edge_points[kEdgeCount][2];
    lv_obj_t *center_dot;
    lv_obj_t *quat_label[4];
    lv_obj_t *quat_value[4];

    // calibration overlay
    lv_obj_t *overlay_title;
    lv_obj_t *overlay_progress_track;
    lv_obj_t *overlay_progress_fill;
    lv_obj_t *overlay_progress_text;
    lv_obj_t *overlay_hint;
    lv_obj_t *overlay_bias_label[3];

    int width;
    int height;
    int cube_cx;
    int cube_cy;

    State state;
    uint8_t last_calib_percent;
    float last_q[4];
    float offset_inv_q[4];
    bool yaw_enabled;
};

ImuCubeUi s_ui = {};

constexpr int kProgressTrackX = 50;
constexpr int kProgressTrackY = 102;
constexpr int kProgressTrackW = 140;
constexpr int kProgressTrackH = 4;

lv_color_t c(uint32_t hex)
{
    return lv_color_hex(hex);
}

int get_obj_size_or_default(lv_obj_t *obj, bool width)
{
    int size = width ? lv_obj_get_width(obj) : lv_obj_get_height(obj);
    if (size <= 0) {
        size = width ? kDefaultWidth : kDefaultHeight;
    }
    return size;
}

void set_visible(lv_obj_t *obj, bool v)
{
    if (obj == nullptr) return;
    if (v) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

// Hamilton product q1 * q2 with q = (w, x, y, z).
static inline void quat_mul(const float a[4], const float b[4], float out[4])
{
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

// Swing-twist decomposition around the Z axis: strip the yaw component from a
// quaternion and return only the residual roll/pitch rotation. Useful when
// the user disables yaw display — gyro-integrated yaw drifts forever without
// a magnetometer, so locking it to the calibration "forward" pose is the
// honest thing to do for a 6-axis IMU.
static inline void quat_strip_yaw(const float q[4], float out[4])
{
    const float w = q[0], z = q[3];
    const float twist_norm_sq = w * w + z * z;
    if (twist_norm_sq < 1e-9f) {
        // Pure 180-degree swing (e.g., flipped face-down). No yaw to strip.
        out[0] = q[0]; out[1] = q[1]; out[2] = q[2]; out[3] = q[3];
        return;
    }
    const float inv = 1.0f / sqrtf(twist_norm_sq);
    const float tw = w * inv;
    const float tz = z * inv;
    const float twist[4] = {tw, 0.0f, 0.0f, tz};
    const float twist_inv[4] = {tw, 0.0f, 0.0f, -tz};
    // swing = q * conj(twist).
    quat_mul(q, twist_inv, out);
    (void)twist;
}

// Rotate vector v by unit quaternion q: v' = q * (0, v) * conj(q).
static inline void rotate_by_q(const float v[3], const float q[4], float out[3])
{
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    const float t2 = 2.0f * (y * v[2] - z * v[1]);
    const float t3 = 2.0f * (z * v[0] - x * v[2]);
    const float t4 = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * t2 + (y * t4 - z * t3);
    out[1] = v[1] + w * t3 + (z * t2 - x * t4);
    out[2] = v[2] + w * t4 + (x * t3 - y * t2);
}

void project_vertex(const float v[3], const float q[4],
                    int *out_x, int *out_y, float *out_z)
{
    float r[3];
    rotate_by_q(v, q, r);

    const float scale = 56.0f / (2.8f + r[2]);
    *out_x = (int)((float)s_ui.cube_cx + r[0] * scale);
    *out_y = (int)((float)s_ui.cube_cy - r[1] * scale);
    *out_z = r[2];
}

lv_obj_t *make_line(lv_obj_t *parent, lv_point_precise_t *points, int width, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_set_size(line, s_ui.width, s_ui.height);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, c(color), 0);
    lv_obj_set_style_line_opa(line, opa, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_line_set_points(line, points, 2);
    return line;
}

lv_obj_t *make_text(lv_obj_t *parent, const char *text, int x, int y,
                    const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, c(color), 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    return label;
}

lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_bg_color(r, c(color), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_outline_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_shadow_width(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

void show_cube_view(bool v)
{
    for (int i = 0; i < kEdgeCount; ++i) set_visible(s_ui.edge[i], v);
    set_visible(s_ui.center_dot, v);
    for (int i = 0; i < 4; ++i) {
        set_visible(s_ui.quat_label[i], v);
        set_visible(s_ui.quat_value[i], v);
    }
}

void show_overlay(bool v)
{
    set_visible(s_ui.overlay_title, v);
    set_visible(s_ui.overlay_hint, v);
    // Progress bar + percent only during calibration; bias values only during await-confirm.
    // The caller fine-tunes which subset within the overlay is visible.
}

void apply_state_visibility()
{
    switch (s_ui.state) {
    case State::kCalibrating:
        show_cube_view(false);
        show_overlay(true);
        set_visible(s_ui.overlay_progress_track, true);
        set_visible(s_ui.overlay_progress_fill, true);
        set_visible(s_ui.overlay_progress_text, true);
        for (int i = 0; i < 3; ++i) set_visible(s_ui.overlay_bias_label[i], false);
        lv_label_set_text(s_ui.overlay_title, "CALIBRATING");
        lv_obj_set_style_text_color(s_ui.overlay_title, c(kColPrimary), 0);
        lv_label_set_text(s_ui.overlay_hint, "HOLD STILL FACE-UP");
        break;
    case State::kAwaitConfirm:
        show_cube_view(false);
        show_overlay(true);
        set_visible(s_ui.overlay_progress_track, false);
        set_visible(s_ui.overlay_progress_fill, false);
        set_visible(s_ui.overlay_progress_text, false);
        for (int i = 0; i < 3; ++i) set_visible(s_ui.overlay_bias_label[i], true);
        lv_label_set_text(s_ui.overlay_title, "CALIBRATED");
        lv_obj_set_style_text_color(s_ui.overlay_title, c(kColAccent), 0);
        lv_label_set_text(s_ui.overlay_hint, "PRESS TO CONTINUE");
        break;
    case State::kRunning:
        show_cube_view(true);
        show_overlay(false);
        set_visible(s_ui.overlay_progress_track, false);
        set_visible(s_ui.overlay_progress_fill, false);
        set_visible(s_ui.overlay_progress_text, false);
        for (int i = 0; i < 3; ++i) set_visible(s_ui.overlay_bias_label[i], false);
        break;
    }
}

void update_progress(uint8_t percent)
{
    if (percent > 100) percent = 100;
    int fill_w = (kProgressTrackW * percent) / 100;
    if (s_ui.overlay_progress_fill != nullptr) {
        lv_obj_set_width(s_ui.overlay_progress_fill, fill_w);
    }
    if (s_ui.overlay_progress_text != nullptr) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent);
        lv_label_set_text(s_ui.overlay_progress_text, buf);
    }
}

void update_bias_labels(const float bias_rad_s[3])
{
    static const char kAxisName[3] = {'X', 'Y', 'Z'};
    for (int i = 0; i < 3; ++i) {
        if (s_ui.overlay_bias_label[i] == nullptr) continue;
        char buf[24];
        const float dps = (bias_rad_s == nullptr ? 0.0f : bias_rad_s[i]) * kRadToDeg;
        snprintf(buf, sizeof(buf), "G%c   %+.3f deg/s", kAxisName[i], (double)dps);
        lv_label_set_text(s_ui.overlay_bias_label[i], buf);
    }
}

void update_cube(const float q[4])
{
    int px[kVertexCount] = {};
    int py[kVertexCount] = {};
    float pz[kVertexCount] = {};

    for (int i = 0; i < kVertexCount; ++i) {
        project_vertex(kVertices[i], q, &px[i], &py[i], &pz[i]);
    }

    for (int i = 0; i < kEdgeCount; ++i) {
        int a = kEdges[i][0];
        int b = kEdges[i][1];
        float depth = (pz[a] + pz[b]) * 0.5f;
        bool front = depth < 0.05f;

        s_ui.edge_points[i][0].x = (lv_value_precise_t)px[a];
        s_ui.edge_points[i][0].y = (lv_value_precise_t)py[a];
        s_ui.edge_points[i][1].x = (lv_value_precise_t)px[b];
        s_ui.edge_points[i][1].y = (lv_value_precise_t)py[b];
        lv_line_set_points(s_ui.edge[i], s_ui.edge_points[i], 2);

        lv_obj_set_style_line_color(s_ui.edge[i], c(front ? kColPrimary : kColFaint), 0);
        lv_obj_set_style_line_width(s_ui.edge[i], front ? 2 : 1, 0);
        lv_obj_set_style_line_opa(s_ui.edge[i], front ? LV_OPA_COVER : LV_OPA_70, 0);
    }
}

void update_data(const float q[4])
{
    static const float kIdentity[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    const float *values = (q == nullptr) ? kIdentity : q;

    for (int i = 0; i < 4; ++i) {
        if (s_ui.quat_value[i] == nullptr) continue;
        char text[16];
        snprintf(text, sizeof(text), "%+.2f", values[i]);
        lv_label_set_text(s_ui.quat_value[i], text);
    }
}
} // namespace

void imu_cube_ui_create(lv_obj_t *root)
{
    if (root == nullptr) {
        return;
    }

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.root = root;
    s_ui.width = get_obj_size_or_default(root, true);
    s_ui.height = get_obj_size_or_default(root, false);
    s_ui.cube_cx = 86;
    s_ui.cube_cy = 118;
    s_ui.last_q[0] = 1.0f;
    s_ui.offset_inv_q[0] = 1.0f;
    s_ui.state = State::kCalibrating;
    s_ui.last_calib_percent = 0;
    s_ui.yaw_enabled = true;

    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_set_style_bg_color(root, c(kColBg), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // Status bar (always visible).
    make_text(root, "IMU", 14, 14, &lv_font_montserrat_12, kColPrimary);
    lv_obj_t *idx = make_text(root, "01/03", 14, 14, &lv_font_montserrat_12, kColDim);
    lv_obj_set_width(idx, 212);
    lv_obj_set_style_text_align(idx, LV_TEXT_ALIGN_RIGHT, 0);
    make_rect(root, 14, 34, 212, 1, kColDivider);
    make_rect(root, 14, 206, 212, 1, kColDivider);

    // Cube view widgets.
    static const char *names[4] = {"q0", "q1", "q2", "q3"};
    constexpr int panel_x = 168;
    for (int i = 0; i < 4; ++i) {
        int y = 56 + i * 32;
        s_ui.quat_label[i] = make_text(root, names[i], panel_x, y,
                                       &lv_font_montserrat_12, kColDim);
        s_ui.quat_value[i] = make_text(root, "+0.00", panel_x, y + 14,
                                       &lv_font_montserrat_14, kColPrimary);
    }
    for (int i = 0; i < kEdgeCount; ++i) {
        s_ui.edge[i] = make_line(root, s_ui.edge_points[i], 1, kColPrimary, LV_OPA_COVER);
    }
    s_ui.center_dot = make_rect(root, s_ui.cube_cx - 2, s_ui.cube_cy - 2, 4, 4, kColAccent);
    lv_obj_set_style_radius(s_ui.center_dot, LV_RADIUS_CIRCLE, 0);

    // Calibration overlay widgets.
    s_ui.overlay_title = make_text(root, "CALIBRATING", 14, 64,
                                   &lv_font_montserrat_14, kColPrimary);
    lv_obj_set_width(s_ui.overlay_title, 212);
    lv_obj_set_style_text_align(s_ui.overlay_title, LV_TEXT_ALIGN_CENTER, 0);

    s_ui.overlay_progress_track = make_rect(root, kProgressTrackX, kProgressTrackY,
                                            kProgressTrackW, kProgressTrackH, kColFaint);
    s_ui.overlay_progress_fill = make_rect(root, kProgressTrackX, kProgressTrackY,
                                           0, kProgressTrackH, kColAccent);

    s_ui.overlay_progress_text = make_text(root, "0%", 14, 116,
                                           &lv_font_montserrat_12, kColDim);
    lv_obj_set_width(s_ui.overlay_progress_text, 212);
    lv_obj_set_style_text_align(s_ui.overlay_progress_text, LV_TEXT_ALIGN_CENTER, 0);

    for (int i = 0; i < 3; ++i) {
        int y = 88 + i * 18;
        s_ui.overlay_bias_label[i] = make_text(root, "GX  +0.000 deg/s", 50, y,
                                               &lv_font_montserrat_14, kColDim);
    }

    s_ui.overlay_hint = make_text(root, "HOLD STILL FACE-UP", 14, 162,
                                  &lv_font_montserrat_12, kColDim);
    lv_obj_set_width(s_ui.overlay_hint, 212);
    lv_obj_set_style_text_align(s_ui.overlay_hint, LV_TEXT_ALIGN_CENTER, 0);

    apply_state_visibility();

    static const float kIdentityQ[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    update_cube(kIdentityQ);
    update_data(nullptr);
    update_progress(0);
}

void imu_cube_ui_update(const float q[4], uint8_t calib_percent,
                        const float gyro_bias_rad_s[3])
{
    if (s_ui.root == nullptr) {
        return;
    }

    static const float kIdentityQ[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    const float *qin = (q == nullptr) ? kIdentityQ : q;

    s_ui.last_q[0] = qin[0];
    s_ui.last_q[1] = qin[1];
    s_ui.last_q[2] = qin[2];
    s_ui.last_q[3] = qin[3];
    s_ui.last_calib_percent = calib_percent;

    // Drive the state machine off calib_percent.
    if (calib_percent < 100) {
        if (s_ui.state != State::kCalibrating) {
            // Sample re-entered calibration (e.g., user moved during await — IMU
            // service resets the counter and we pick that up here too).
            s_ui.state = State::kCalibrating;
            apply_state_visibility();
        }
        update_progress(calib_percent);
        return;
    }

    // calib_percent == 100
    if (s_ui.state == State::kCalibrating) {
        s_ui.state = State::kAwaitConfirm;
        update_bias_labels(gyro_bias_rad_s);
        apply_state_visibility();
        return;
    }
    if (s_ui.state == State::kAwaitConfirm) {
        // Keep latest bias on screen but otherwise wait for user click.
        update_bias_labels(gyro_bias_rad_s);
        return;
    }

    // kRunning — drive the cube.
    float q_rel[4];
    quat_mul(s_ui.offset_inv_q, qin, q_rel);
    if (!s_ui.yaw_enabled) {
        float q_no_yaw[4];
        quat_strip_yaw(q_rel, q_no_yaw);
        update_cube(q_no_yaw);
    } else {
        update_cube(q_rel);
    }
    update_data(qin);
}

void imu_cube_ui_reset(void)
{
    if (s_ui.root == nullptr) {
        return;
    }
    if (s_ui.state != State::kRunning) {
        // Reset is only meaningful for the live cube view.
        return;
    }

    // Capture conjugate of the current attitude as the new "zero".
    s_ui.offset_inv_q[0] =  s_ui.last_q[0];
    s_ui.offset_inv_q[1] = -s_ui.last_q[1];
    s_ui.offset_inv_q[2] = -s_ui.last_q[2];
    s_ui.offset_inv_q[3] = -s_ui.last_q[3];

    static const float kIdentityQ[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    update_cube(kIdentityQ);
    update_data(nullptr);
}

bool imu_cube_ui_consume_click(void)
{
    if (s_ui.root == nullptr) {
        return false;
    }
    if (s_ui.state != State::kAwaitConfirm) {
        return false;
    }
    // Snap the cube's "zero" to the current attitude so the user enters the
    // running view facing forward, then transition.
    s_ui.offset_inv_q[0] =  s_ui.last_q[0];
    s_ui.offset_inv_q[1] = -s_ui.last_q[1];
    s_ui.offset_inv_q[2] = -s_ui.last_q[2];
    s_ui.offset_inv_q[3] = -s_ui.last_q[3];
    s_ui.state = State::kRunning;
    apply_state_visibility();

    static const float kIdentityQ[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    update_cube(kIdentityQ);
    update_data(s_ui.last_q);
    return true;
}

void imu_cube_ui_set_yaw_enabled(bool enabled)
{
    s_ui.yaw_enabled = enabled;
}
