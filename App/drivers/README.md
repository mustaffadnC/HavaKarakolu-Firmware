# Sürücüler — entegrasyon notları

Tüm sürücüler `bus/` arayüzlerine (`hk_i2c_bus_t`, `hk_uart_t`) bağlanır; doğrudan HAL çağırmazlar. Böylece aynı sürücü farklı bus'larda (donanım/bit-bang) ve host testlerinde (mock) çalışır.

| Sürücü | Bağımlılık | Durum |
|---|---|---|
| `sht4x` | yok (saf + bus) | ✅ tam; `hk_sht4x_parse` host-testli |
| `bmp581` | yok (register-level + bus + hk_time) | ✅ tam (forced mode) |
| `gps_ublox` (`nmea`) | yok (saf + uart) | ✅ tam; NMEA parser host-testli |
| `bmi270` | **Bosch BMI270_SensorAPI** (vendor) | ⚠️ sarmalayıcı hazır; vendor lib gerekli |
| `ws2812/servo/fan/buzzer/lock/battery` | HAL (F3) | ⬜ sıradaki faz |

## BMI270 vendor kurulumu

BMI270, başlangıçta ~8KB'lik bir **config blob** yüklenmesini gerektirir; bu yalnızca resmi Bosch sürücüsüyle gelir.

1. Şu dosyaları indirip `App/drivers/bmi270/vendor/` altına koy
   (Bosch `BMI270_SensorAPI` deposundan):
   `bmi2.c bmi2.h bmi2_defs.h bmi270.c bmi270.h bmi270_config.c` (ve varsa `bmi270_context.*`).
2. CubeIDE'de bu klasörü **Source Location** + **Include path** olarak ekle.
3. Derleyici sembolü olarak **`HK_USE_BMI270`** tanımla
   (Project → C/C++ Build → Settings → Preprocessor → Defined symbols).

Tanımlanmazsa `bmi270_drv.c` stub'a düşer (`HK_ERR_NOT_FOUND`) ve firmware yine derlenir; IMU verisi gelmez.

> Alternatif: BMP581 için de Bosch BMP5 API kullanmak istersen aynı desen uygulanır; ancak mevcut `bmp581.c` register-level olarak zaten çalışır.

## Notlar
- I2C1'de BMI270 (0x68) + BMP581 (0x46) paylaşımlı → bus mutex'i `i2c_hw` sağlar.
- SHT4x sabit adresli (0x44); iki örnek ayrı bus'larda (I2C2 + bit-bang). Bkz. `board_config.h`.
- ⚠️ İlk bring-up'ta **I2C1 pull-up** doğrulaması (`hk_i2c_scan`) şart.
