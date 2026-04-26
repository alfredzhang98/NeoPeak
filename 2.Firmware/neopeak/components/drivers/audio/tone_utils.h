#pragma once

#include "freertos/FreeRTOS.h"

#include "tone_music.h"
#include "tone_player.h"

// Play a tone sequence and block until finished.
void tone_play_and_wait(TonePlayer &player, MusicId id, TickType_t tick_delay);

// Start playing a tone sequence without blocking.
bool tone_play_start(TonePlayer &player, MusicId id);

// Stop current tone sequence.
void tone_play_stop(TonePlayer &player);

// Update playback state, returns true while playing.
bool tone_play_update(TonePlayer &player, uint32_t tick_ms);

// Get total duration (ms) for a tone sequence, including tail guard.
uint32_t tone_get_duration_ms(MusicId id);
