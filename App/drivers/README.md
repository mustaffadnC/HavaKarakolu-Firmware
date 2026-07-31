# Sürücüler — entegrasyon notları

Tüm sürücüler `bus/` arayüzlerine (`hk_i2c_bus_t`, `hk_spi_bus_t`, `hk_uart_t`) bağlanır; doğrudan HAL çağırmazlar. Böylece aynı sürücü farklı bus'larda (donanım/bit-bang) ve host testlerinde (mock) çalışır.

| Sürücü | Bağımlılık | Durum |
|---|---|---|
| `sht4x` | yok (saf + bus) | ✅ tam; `hk_sht4x_parse` host-testli |
| `bmp581` | yok (register-level + bus + hk_time) | ✅ tam (forced mode; on-chip lineerleştirme → kompanzasyon yok, ölçek `raw/2^16` °C ve `raw/2^6` Pa) |
| `bmp581` | yok | 📦 rev-1 referansı; rev-2 kartta kullanılmıyor |
| `gps_ublox` (`nmea`) | yok (saf + uart) | ✅ tam; NMEA parser host-testli |
| `bmi270` | Bosch BMI270_SensorAPI (**vendored**: `bmi270/vendor/`, v2.86.1) | ✅ sarmalayıcı + vendor CI'da derleniyor ve register-map mock ile testli |
| `sd_spi` | yok (saf + spi + crc) | ✅ tam; davranışsal kart modeliyle host-testli |
| `servo/fan/buzzer/lock/battery` | HAL (hesap katmanı saf) | ✅ tam; matematik host-testli |
| `ws2812` | HAL | 📦 rev-1 referansı; rev-2 kartta LED yok |

## BMI270

Vendor kaynakları **repoda**: `App/drivers/bmi270/vendor/` (sürüm: `VERSION.md`).
CubeIDE tarafında yapılacaklar:

1. `App/drivers/bmi270/vendor/` klasörünü **Include path**'e ekle
   (kaynaklar `App/` altında olduğundan Source Location zaten kapsar).
2. Derleyici sembolü olarak **`HK_USE_BMI270`** tanımla
   (Project → C/C++ Build → Settings → Preprocessor → Defined symbols).

Tanımlanmazsa `bmi270_drv.c` stub'a düşer (`HK_ERR_NOT_FOUND`) ve firmware yine derlenir; IMU verisi gelmez.

## Notlar
- I2C1'de BMI270 (0x68) + BMP581 (0x46) paylaşımlı → bus mutex'ini `i2c_hw` sağlar.
- SHT4x sabit adresli (0x44); iki örnek ayrı bus'larda (I2C2 + bit-bang). Bkz. `board_config.h`.
- SD kart: `sd_spi` + FatFs + `services/storage`; kart yoksa sistem **degraded** loglamayla çalışmaya devam eder.
- ⚠️ İlk bring-up'ta **I2C1 pull-up** doğrulaması (`hk_i2c_scan`) şart.
