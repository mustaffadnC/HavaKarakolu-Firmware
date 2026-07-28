# Ülgen ÇARGE — Modüler Kapsül Firmware (Rev-2)

STM32F407VGTx tabanlı modüler kapsül kartının gömülü yazılımı. Kapsül; taşıyıcıdan **servo + solenoid** ile ayrılır, iniş boyunca ve yerde ortam verisini (2× SHT4x sıcaklık/nem, BMP280 basınç/irtifa, BMI270 IMU, MAX-M10S GPS) ölçer, **SD karta loglar** ve 2 fan ile termal kontrol yapar. Güç: Li-ion paket (PTC + ters-polarite koruması → 5V buck → 3.3V LDO; servo için ayrı 5V buck).

- **MCU:** STM32F407VGTx (Cortex-M4F @168 MHz, 1MB Flash, 192KB SRAM + 64KB CCM)
- **RTOS / Dil:** FreeRTOS + C (C11)
- **Araç zinciri:** STM32CubeIDE + CubeMX (HAL). Host birim testleri için gcc + CMake.
- **Veri çıkışı:** SD kart (SPI1 + FatFs). Telemetri radyosu yok.

> Donanım rev-2 pin haritasının tek doğru kaynağı: `App/bsp/board_config.h`
> Elektronik ekibine açık sorular: `docs/ee-questions.md`

---

## Mimari (katmanlar)

```
App/                      ← bu repoda yazılan, taşınabilir uygulama kodu
  bsp/                    board pin/clock soyutlaması
  bus/                    i2c_bus_if, uart_if (arayüzler) + HW/SW implementasyonları
  common/                 crc, ringbuf, units, log (saf C, host-testable)
  drivers/                bmi270, bmp280, sht4x, gps_ublox, sd_spi, servo, fan, buzzer, lock, battery
  services/               sensor_manager, telemetry, command, mission, health, config, storage
  tasks/                  FreeRTOS görev tanımları + app_init
Core/ Drivers/ Middlewares/   ← CubeMX/CubeIDE üretir (bu repoda YOK; aşağıdaki entegrasyon)
tests/host/               gcc + CMake ile çalışan saf-mantık birim testleri
tools/                    yer istasyonu / yardımcı scriptler (python)
```

**Tasarım ilkesi — bağımlılık enjeksiyonu:** Sürücüler doğrudan HAL'a değil, `bus/` altındaki arayüzlere (`i2c_bus_if_t`, `uart_if_t`) bağlanır. Böylece:
- `sht4x` sürücüsü hem donanım I2C2 hem bit-bang I2C ile **aynı kodla** çalışır.
- Host testlerinde gerçek HAL yerine **mock bus** verilebilir.

---

## CubeMX/CubeIDE entegrasyonu (özet)

> **Adım adım tam rehber: [`docs/cubemx-setup.md`](docs/cubemx-setup.md)** — kurulumdan linker ayarına (FLASH=896K, printf float) kadar. PCB bring-up planı: [`docs/bringup.md`](docs/bringup.md).

Bu repo yalnızca taşınabilir `App/` katmanını ve testleri içerir. ST'nin ürettiği `Core/`, `Drivers/`, `Middlewares/` katmanı CubeMX'te bir kez üretilir, sonra `App/` eklenir:

1. **CubeMX** ile yeni proje: STM32F407VGTx, Toolchain = STM32CubeIDE.
2. **Clock:** HSE 8 MHz kristal (⚠️ Y2 değeri teyit bekliyor — `docs/ee-questions.md` S2) → PLL → SYSCLK 168 MHz (APB1 42, APB2 84).
3. **Çevre birimleri** (pin haritası `App/bsp/board_config.h` ile bire bir):
   - USART1 (PA9/PA10) — GPS, DMA RX + global interrupt, IDLE açık
   - I2C1 (PB6/PB7), I2C2 (PB10/PB11) — DMA, Fast Mode 400 kHz
   - SPI1 (PA5 SCK / PA6 MISO / PA7 MOSI) — SD kart; PA4 = GPIO output (soft CS, init HIGH)
   - TIM1_CH1 (PA8) — servo PWM 50 Hz
   - TIM3_CH2 (PB5) — buzzer
   - ADC1_IN10 (PC0) — BAT_TEST
   - GPIO: PB0 (Solenoid_Tetik, out, **init LOW**), PB12/PB15 (FAN1/FAN2, out), PB8/PB9 (bit-bang I2C, OD), PC4 (EXTI4, BMI270 INT — opsiyonel)
   - **PB13/PB14 hiçbir şeye atanmaz** (şemada SWD netleri görünüyor — `docs/ee-questions.md` S1)
   - SYS: SWD (PA13/PA14), Timebase = TIM (SysTick'i FreeRTOS'a bırak)
4. **Middlewares:** FreeRTOS (CMSIS-RTOS v2).
5. Üretilen projeye `App/` klasörünü ekle; Include path'lere `App/**` dizinlerini ekle.
6. **IWDG**'yi `.ioc`'de etkinleştir (handle adı `hiwdg`).
7. `main.c` içinde, CubeMX init'lerinden sonra ve scheduler başlamadan önce:
   ```c
   #include "tasks/app.h"
   ...
   hk_app_init();    /* bus/sensör/aktüatör init, fail-safe LOCK */
   hk_app_start();   /* FreeRTOS görevlerini oluştur */
   /* sonra osKernelStart() / vTaskStartScheduler() */
   ```
   Log çıkışı için `hk_log_init(<sink>, HK_LOG_INFO)` çağır (RTT veya debug UART sink).

> CubeMX handle isimleri kod tarafında beklenen adlarla eşleşmeli: `hi2c1`, `hi2c2`, `huart1`, `htim1`, `htim3`, `hspi1`, `hadc1`, `hiwdg`. Farklıysa `App/bsp/board_config.h` güncellenir.

---

## Host birim testleri (saf mantık)

Donanım gerektirmeyen kısımlar (CRC, ring buffer, SHT4x parse, GPS/NMEA parser,
aktüatör matematiği, ileride telemetri + durum makinesi) host'ta test edilir.

**CMake + ctest (taşınabilir, CI'da bu kullanılır):**
```bash
cd tests/host
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

**Windows hızlı yol (gcc'yi otomatik bulur):**
```powershell
powershell -ExecutionPolicy Bypass -File tests\host\run_tests.ps1
```

Gereksinim: `gcc` + `cmake`. Bu makinede gcc kuruldu:
`winget install BrechtSanders.WinLibs.POSIX.UCRT`.

> ⚠️ **Windows Smart App Control (SAC):** Bu makinede SAC enforce modunda olduğu
> için yerelde taze derlenen imzasız exe/DLL'lerin **çalıştırılması** engelleniyor
> (`WinError 4551`). Derleme sorunsuz; testler SAC kapatılırsa yerelde, her durumda
> **CI'da (Linux)** yeşil verir. SAC, ISG-reputation tabanlı olduğundan self-signed
> sertifika kabul etmez. Geçmiş çalıştırmalarda 70+ check yeşil görüldü.

**CI:** `.github/workflows/host-tests.yml` her push'ta Ubuntu'da build + ctest çalıştırır.

---

## Durum (faz takibi)

| Faz | Kapsam | Durum |
|---|---|---|
| F0 | İskelet + ortak altyapı (crc/ringbuf/units/log, bus arayüzleri, board_config, host test harness) | ✅ |
| F1 | Bus bring-up (I2C HW/SW, UART DMA, i2c_scan) | ✅ |
| F2 | Sensör sürücüleri (sht4x, baro, gps/nmea, bmi270-wrapper) | ✅ |
| F3 | Aktüatör/IO sürücüleri (servo, buzzer, fan, lock, battery) | ✅ |
| F4–F5 | Sağlık/güç + RTOS entegrasyon (filters, system_state, sensor_manager, health/IWDG, app+görevler) | ✅ |
| **Rev-2 uyarlaması** | board_config v2 (BMP280, pin değişiklikleri, WS2812 çıkarıldı) | ✅ |
| P2 | BMP280 sürücüsü (datasheet-birebir kompanzasyon testli) | ✅ |
| P3 | SPI + SD kart + FatFs + storage (log) servisi (güç-kesintisi testli) | ✅ |
| P4 | BMI270 vendor lib (Bosch SensorAPI v2.86.1, repoda) | ✅ |
| P5 | Görev durum makinesi (parametrik, %100 durum kapsamalı test) | ✅ |
| P6 | Uçuş simülatörü + uçtan uca sistem testi (`test_flight_e2e`) | ✅ |
| P7 | Config/NV flash journal + SD `CONFIG.INI` override | ✅ |
| P8 | Entegrasyon rehberi (`docs/cubemx-setup.md`) + `hk-capsule-fw.ioc` | ✅ (ilk açılışta gözle doğrula) |
| P9 | Donanım bring-up (PCB gelince) — plan: `docs/bringup.md` | ⬜ donanım bekliyor |

---

## Donanım durumu (EE cevapları işlendi — `docs/ee-questions.md`)

**İki kart varyantı** (derleme: varsayılan `HK_BOARD_SUKRU`; ilk kart için `HK_BOARD_REV2A` sembolü):

| | Şükrü kartı (önce denenecek) | İlk kart (yedek) |
|---|---|---|
| SWD | ✅ PA13/PA14 (ST-Link çalışır) | ❌ yanlış bağlı → BOOT0+UART bootloader (`docs/bringup.md` §3) |
| Buzzer | PB14 / TIM12_CH1 | PB5 / TIM3_CH2 |
| FAN2 | PB13 | PB15 |
| Batarya ölçümü | **yok** (PC0 boş) | PC0 / ADC1_IN10 (÷11) |

Teyitler: kristal **8 MHz** ✓; solenoid **enerjisiz = kilitli** (fail-safe ✓, bobin yalnız RELEASE'te enerjilenir); I2C1 pull-up ✓; BMP280 0x76 ✓; BMI270 INT1 ✓; SD 5V Arduino modülü ✓; güç bütçesi ✓. Kalan öneri: ayrılma teyit mikro-switch'i (boş GPIO mevcut).
