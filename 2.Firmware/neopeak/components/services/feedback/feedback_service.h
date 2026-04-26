#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Music IDs — values match MusicId enum in tone_music.h
// ---------------------------------------------------------------------------
typedef enum {
    FEEDBACK_MUSIC_NONE          = 0xFF,
    FEEDBACK_MUSIC_STARTUP       = 0,
    FEEDBACK_MUSIC_SHUTDOWN      = 1,
    FEEDBACK_MUSIC_ERROR         = 2,
    FEEDBACK_MUSIC_CONNECT       = 3,
    FEEDBACK_MUSIC_DISCONNECT    = 4,
    FEEDBACK_MUSIC_UNSTABLE      = 5,
    FEEDBACK_MUSIC_CHARGE_START  = 6,
    FEEDBACK_MUSIC_CHARGE_END    = 7,
    FEEDBACK_MUSIC_DEVICE_INSERT = 8,
    FEEDBACK_MUSIC_DEVICE_PULLOUT= 9,
    FEEDBACK_MUSIC_NO_OP_WARN    = 10,
} feedback_music_t;

// ---------------------------------------------------------------------------
// Haptic intensity presets
// ---------------------------------------------------------------------------
typedef enum {
    FEEDBACK_HAPTIC_NONE   = 0,
    FEEDBACK_HAPTIC_LIGHT  = 1, // 300ms, intensity 3
    FEEDBACK_HAPTIC_MEDIUM = 2, // 400ms, intensity 3
    FEEDBACK_HAPTIC_STRONG = 3, // 500ms, intensity 5
} feedback_haptic_t;

// ---------------------------------------------------------------------------
// Feedback request — published to MSG_TOPIC_FEEDBACK
// ---------------------------------------------------------------------------
typedef struct {
    feedback_music_t  music;
    feedback_haptic_t haptic;
} feedback_request_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/**
 * Start the feedback service.
 * Subscribes to MSG_TOPIC_FEEDBACK and launches the feedback task.
 */
esp_err_t feedback_service_start(void);

/**
 * Post a feedback request directly (bypasses message bus).
 * Safe to call from any task context.
 */
void feedback_post(feedback_music_t music, feedback_haptic_t haptic);

/**
 * Post a feedback request and block until it finishes playing.
 * Use only for critical sequences like shutdown where timing matters.
 */
void feedback_post_blocking(feedback_music_t music, feedback_haptic_t haptic);

#ifdef __cplusplus
}
#endif
