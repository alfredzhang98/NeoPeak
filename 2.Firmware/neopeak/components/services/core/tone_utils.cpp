#include "tone_utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void tone_play_and_wait(TonePlayer &player, MusicId id, TickType_t tick_delay)
{
    const MusicList_t *music = tone_music_get(id);
    if (music == nullptr) {
        return;
    }

    player.Play(music->mc, music->length);

    uint32_t max_ms = 0;
    for (uint16_t i = 0; i < music->length; ++i) {
        max_ms += music->mc[i].Time;
    }
    max_ms += 200;

    uint32_t elapsed_ms = 0;
    while (player.Update((uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS)) {
        vTaskDelay(tick_delay);
        elapsed_ms += (uint32_t)(tick_delay * portTICK_PERIOD_MS);
        if (elapsed_ms > max_ms) {
            break;
        }
    }
}

bool tone_play_start(TonePlayer &player, MusicId id)
{
    const MusicList_t *music = tone_music_get(id);
    if (music == nullptr) {
        return false;
    }

    player.Play(music->mc, music->length);
    return true;
}

void tone_play_stop(TonePlayer &player)
{
    player.Stop();
}

bool tone_play_update(TonePlayer &player, uint32_t tick_ms)
{
    return player.Update(tick_ms);
}

uint32_t tone_get_duration_ms(MusicId id)
{
    const MusicList_t *music = tone_music_get(id);
    if (music == nullptr) {
        return 0;
    }

    uint32_t total_ms = 0;
    for (uint16_t i = 0; i < music->length; ++i) {
        total_ms += music->mc[i].Time;
    }
    return total_ms + 200;
}
