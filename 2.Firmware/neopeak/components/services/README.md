# services

## 目的
服务与应用层，包含通信状态机、算法与应用逻辑等。

## 这里放什么
- 通信状态机与协议适配
- 算法与策略模块
- 应用层流程与业务逻辑

## 目录划分
- core: 通用能力（消息总线、提示音谱子/播放器等）
- imu: 传感器采样、融合与姿态计算
- power: 电源策略、按键状态机、开关机流程
- display: 显示线程、页面切换与 UI 状态机
- comm: 通信协议、连接状态机与数据同步（含编码器事件）

## 开发指导
- 组合使用 message bus/HAL/drivers 能力。
- 业务逻辑清晰分层，避免下沉到驱动层。
- 通过接口隔离与单元测试保证可维护性。

## 示例
- encoder_service_start()
- imu_service_start()
- power_service_start()

## 提示音模块（Tone）
- 文件：core/tone_map.h / core/tone_player.h/.cpp / core/tone_music.h/.cpp / core/tone_utils.h/.cpp
- 提供提示音谱子与播放器逻辑，便于电源管理或业务层触发音效。
- 典型流程：
	1) 通过 `tone_music_get()` 获取曲目
	2) `TonePlayer::SetCallback()` 绑定蜂鸣器输出
	3) 周期调用 `TonePlayer::Update()` 播放
