#pragma once

#include <stdint.h>
#include "tone_player.h"

// 提示音列表
enum class MusicId : uint8_t
{
    Startup = 0,
    Shutdown,
    Error,
    Connect,
    Disconnect,
    UnstableConnect,
    BattChargeStart,
    BattChargeEnd,
    DeviceInsert,
    DevicePullout,
    NoOperationWarning,
    Count
};

typedef struct
{
    const TonePlayer::MusicNode_t *mc;
    uint16_t length;
    const char *name;
} MusicList_t;

const MusicList_t *tone_music_get_list(uint16_t *out_count);
const MusicList_t *tone_music_get(MusicId id);
