# imu

ICM-42688-P device driver layer.

This driver only talks to the sensor through `hal_i2c_device_t`, so board code
owns the ESP-IDF v6 I2C master bus/device creation and pin mapping.

Hardware notes from DS-000347 v1.6:

- I2C address is `0x68` when `AP_AD0` is low and `0x69` when `AP_AD0` is high.
- `WHO_AM_I` is `0x47`.
- Pin 7 is `RESV` and should be connected to GND.
- Pin 12 `AP_CS` should be connected to VDDIO for I2C/I3C operation.
- I2C lines are open drain and need pull-up resistors.

Typical flow:

1. Create an ESP-IDF v6 I2C master bus with `hal_i2c_master_bus_init()`.
2. Add the IMU address with `hal_i2c_master_device_add()`.
3. Call `icm42688p_config_default()` and `icm42688p_begin()`.
4. Read data with `icm42688p_read_raw()` or `icm42688p_read()`.
