# IMU 姿态估计

ICM-42688-P (6 轴) → Madgwick 滤波 → quaternion → cube 渲染。

## 信号链

```
ICM-42688-P (200 Hz ODR)
  └ 片上 3 阶 LPF, BW = ODR/10 ≈ 20 Hz
        │
        ▼  poll @ 100 Hz (10 ms)
imu_service::imu_task
  ├ remap_imu()      chip body → screen frame: (+X, -Y, -Z)
  ├ bias 校准        60 连续静止样本 (gmag < 12°/s)，运动则清零
  ├ initial_q        校准结束时由 accel 均值算 shortest-arc q
  ├ madgwick_step()  β = 0.015, dt 由 timestamp 算
  └ publish MSG_TOPIC_IMU (q, calib_percent, bias)
        │
        ▼
display_service::on_imu_msg → ui_app_handle_imu → imu_cube_ui_update
imu_cube_ui state machine: Calibrating → AwaitConfirm → Running
```

## 关键参数

| 参数 | 值 | 文件 |
|---|---|---|
| ODR | 200 Hz | [icm42688p.cpp:276](../components/drivers/imu/icm42688p.cpp#L276) |
| 片上 LPF | 3 阶, ODR/10 (20 Hz) | [imu_service.cpp:411-415](../components/services/imu/imu_service.cpp#L411) |
| polling | 100 Hz (10 ms) | sdkconfig: `CONFIG_IMU_SAMPLE_PERIOD_MS=10` |
| Madgwick β | 0.015 | [imu_service.cpp:27](../components/services/imu/imu_service.cpp#L27) |
| 校准样本数 | 60 (~0.6 s) | [imu_service.cpp:33](../components/services/imu/imu_service.cpp#L33) |
| 校准静止阈值 | 12°/s | [imu_service.cpp:34](../components/services/imu/imu_service.cpp#L34) |
| Axis remap | `(+X, -Y, -Z)` 对 accel + gyro | [imu_service.cpp:48-58](../components/services/imu/imu_service.cpp#L48) |

## Madgwick 6 轴融合

状态：单位 quaternion `q = (w, x, y, z)`，body → world 旋转。

每步：
1. **gyro 积分**：`q̇ = ½ q ⊗ ω_body`（ω 是 bias 修正后的角速度）
2. **accel 校正**：用 (q^T)·(0,0,1) 与 a_norm 的残差做梯度下降，幅度由 β 控制
3. **更新**：`q ← q + (q̇ - β·∇f) · dt`，归一化

无 Euler 中间状态 → 无万向锁、无 atan2 跳变。

## 启动校准

```cpp
if (gmag_dps < 12.0f) {
    accumulate gyro/accel
    if (samples == 60) {
        s_gyro_bias = mean(gyro_accum)
        s_q         = initial_q_from_accel(mean(accel_accum))
        s_bias_ready = true
    }
} else {
    reset accumulator   // 运动检测到就重新数
}
```

`initial_q_from_accel` 用 shortest-arc 公式从重力方向直接算 q，避免 Madgwick 从 identity 慢慢收敛 30 秒。

## UI 状态机

[imu_cube_ui.cpp](../components/ui/imu_cube_ui.cpp)：

| 状态 | 显示 | 进入条件 |
|---|---|---|
| `Calibrating` | "CALIBRATING" + 进度条 + 提示 | 进页面 / 收到 calib_percent < 100 |
| `AwaitConfirm` | "CALIBRATED" + 三轴 bias 数值 | 收到 calib_percent == 100 |
| `Running` | cube + q0..q3 标签 | 单击 |

## 生命周期

| 事件 | 行为 |
|---|---|
| 进 IMU 页 | sensor 上电 + reset_filter_state + bias 重新校准 |
| Running 单击 | `imu_cube_ui_reset()` 把当前 q 当新零点 |
| BACK 单击 / 双击 | sensor 断电，下次进入再校准 |

bias 不缓存——每次进入都重新校准（处理温度漂移）。

## 已知 trade-off

- **每次进 IMU 页等 ~1s**：换温度自适应；可改成"后台一直跑"方案省这 1s 但 bias 会随戴久了温度变化偏移
- **β = 0.015 偏低**：静止稳，但若 gyro bias 长期飘 (温度变化 30°C+)，yaw 修正会偏慢——属于"每次重新校准"覆盖的场景
- **远端 yaw 漂移**：6 轴无磁力计无法绑定真北，长时间 yaw 必然漂；通过 reset 按钮人工修正
