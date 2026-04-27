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
| Madgwick gain β_p (proportional) | 2.0 | [imu_service.cpp:21-31](../components/services/imu/imu_service.cpp#L21) |
| Accel 软 LPF α | 0.5 (~14 ms tau) | [imu_service.cpp:34](../components/services/imu/imu_service.cpp#L34) |
| 校准样本数 | 60 (~0.6 s) | [imu_service.cpp:33](../components/services/imu/imu_service.cpp#L33) |
| 校准静止阈值 | 12°/s | [imu_service.cpp:34](../components/services/imu/imu_service.cpp#L34) |
| Axis remap | `(+X, -Y, -Z)` 对 accel + gyro | [imu_service.cpp:48-58](../components/services/imu/imu_service.cpp#L48) |

## Madgwick 6 轴融合

状态：单位 quaternion `q = (w, x, y, z)`，body → world 旋转。

每步：
1. **gyro 积分**：`q̇ = ½ q ⊗ ω_body`（ω 是 bias 修正后的角速度）
2. **accel 校正（proportional 形式）**：用 (q^T)·(0,0,1) 与 a_norm 的残差 ∇f 做"非归一化"梯度下降——`q̇_corr = -β_p · ∇f`，**校正幅度直接和误差成正比**（不是论文的归一化形式）
3. **更新**：`q ← q + (q̇_gyro + q̇_corr) · dt`，归一化

无 Euler 中间状态 → 无万向锁、无 atan2 跳变。

**为什么用 proportional 形式（不是论文原版）**：

- 归一化版校正速度恒为 β rad/s，与误差大小无关。结果：β 大→静止时被噪声放大成抖动；β 小→大误差恢复线性慢。两者天生互斥
- 非归一化版校正 ∝ 误差。误差大时快、近真值时几乎不动 → 同时解决"快速恢复"和"静止不抖"
- β_p 的物理意义：1/τ 时间常数。β_p=2.0 → τ=0.5s，90% 恢复 ~1.15s

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

## 抗运动加速度门控

`madgwick_step()` 仅在 `|a| ∈ [0.65, 1.35] g` 时应用 accel 校正——超出说明手在大幅运动，accel 不只读到重力（含线性加速度），**只信 gyro 积分**，不污染 q。回到带内立即恢复 proportional 校正。

不再需要"恢复期"或"真静止"特殊档——proportional 形式的指数衰减天然完成：误差大→拉得快、误差小→自动慢→不抖。

效果：剧烈晃动后只要把表放下、或停手，**100~200ms 内** β 跳到 0.5，roll/pitch 直接被锁回真重力方向。

## 已知 trade-off

- **每次进 IMU 页等 ~1s**：换温度自适应；可改成"后台一直跑"方案省这 1s 但 bias 会随戴久了温度变化偏移
- **β = 0.015 偏低**：静止稳，但若 gyro bias 长期飘 (温度变化 30°C+)，yaw 修正会偏慢——属于"每次重新校准"覆盖的场景
- **远端 yaw 漂移**：6 轴无磁力计无法绑定真北，长时间 yaw 必然漂；通过 reset 按钮人工修正
- **持续大动作时 roll/pitch 会临时跟不上**：抗运动门控期间只走 gyro，gyro bias 会有累积；动作停下、|a| 回到 1g 时 Madgwick 重新拉回——观感是"动作大时稍慢、停下后快速归位"
