#include "tone_music.h"

#include "tone_map.h"

#define MUSIC_DEF(name) static const TonePlayer::MusicNode_t Music_##name[] =

/* 开机音 */
MUSIC_DEF(Startup)
{
    {L5, 60, 90},
    {L6, 60, 90},
    {M1, 80, 100},
    {M3, 80, 100},
    {M5, 80, 100},
    {M6, 120, 100},
    {H1, 100, 95},
    {H3, 120, 95},
    {H5, 160, 90},
    {0,  60, 0},
    {H2, 80, 85},
    {H4, 120, 85},
};

/* 关机音 */
MUSIC_DEF(Shutdown)
{
    {H5, 120, 90},
    {H3, 100, 90},
    {H1, 100, 90},
    {M6, 100, 85},
    {M5, 80, 85},
    {M3, 80, 85},
    {M1, 120, 85},
    {0,  60, 0},
    {L6, 120, 80},
    {L5, 160, 80},
};

/* 错误提示音 */
MUSIC_DEF(Error)
{
    {100, 80, 100},
    {0,   80, 0},
    {100, 80, 100},
};

/* 连接成功音 */
MUSIC_DEF(Connect)
{
    {H1, 80, 100},
    {H2, 80, 100},
    {H3, 80, 100},
};

/* 断开连接音 */
MUSIC_DEF(Disconnect)
{
    {H3, 80, 100},
    {H2, 80, 100},
    {H1, 80, 100},
};

/* 信号不稳定提示音 */
MUSIC_DEF(UnstableConnect)
{
    {H1, 80, 100},
    {0, 80, 0},
    {H1, 80, 100},
};

/* 充电启动提示音 */
MUSIC_DEF(BattChargeStart)
{
    {L1, 80, 100},
    {L3, 80, 100},
};

/* 充电关闭提示音 */
MUSIC_DEF(BattChargeEnd)
{
    {L3, 80, 100},
    {L1, 80, 100},
};

/* 设备插入提示音 */
MUSIC_DEF(DeviceInsert)
{
    {M1, 180, 100},
    {L6, 80, 100},
    {L4, 80, 100},
    {M4, 160, 100},
};

/* 设备拔出提示音 */
MUSIC_DEF(DevicePullout)
{
    {L6, 80, 100},
    {L4, 80, 100},
    {L3, 80, 100},
};

/* 无操作提示音 */
MUSIC_DEF(NoOperationWarning)
{
    {4000, 40, 100},
    {0, 80, 0},
    {4000, 40, 100},
    {0, 80, 0},
    {4000, 40, 100},
};

#define ADD_MUSIC(mc) {Music_##mc, (sizeof(Music_##mc) / sizeof(Music_##mc[0])), #mc}

static const MusicList_t MusicList[] =
{
    ADD_MUSIC(Startup),
    ADD_MUSIC(Shutdown),
    ADD_MUSIC(Error),
    ADD_MUSIC(Connect),
    ADD_MUSIC(Disconnect),
    ADD_MUSIC(UnstableConnect),
    ADD_MUSIC(BattChargeStart),
    ADD_MUSIC(BattChargeEnd),
    ADD_MUSIC(DeviceInsert),
    ADD_MUSIC(DevicePullout),
    ADD_MUSIC(NoOperationWarning),
};

const MusicList_t *tone_music_get_list(uint16_t *out_count)
{
    if (out_count != nullptr)
    {
        *out_count = (uint16_t)(sizeof(MusicList) / sizeof(MusicList[0]));
    }
    return MusicList;
}

const MusicList_t *tone_music_get(MusicId id)
{
    uint16_t count = (uint16_t)(sizeof(MusicList) / sizeof(MusicList[0]));
    uint16_t index = static_cast<uint16_t>(id);
    if (index >= count)
    {
        return nullptr;
    }
    return &MusicList[index];
}
