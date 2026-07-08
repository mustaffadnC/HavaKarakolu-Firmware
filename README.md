# Ülgen ÇARGE — Hava Karakolu Firmware

STM32F405VGTx tabanlı, model uçağa bağlıyken 28V busundan beslenen; **servo + solenoid kilit** ile uçaktan ayrılıp kendi **Li-ion** paketiyle **otonom hava istasyonu** olarak çalışmaya devam eden payload kartının gömülü yazılımı.

- **MCU:** STM32F405VGTx (Cortex-M4F @168 MHz, 1MB Flash, 192KB SRAM + 64KB CCM)
- **RTOS / Dil:** FreeRTOS + C (C11)
- **Araç zinciri:** STM32CubeIDE + CubeMX (HAL). Host birim testleri için gcc + CMake.

> Tam mimari ve yol haritası: `../.claude/plans/bu-sistemin-yaz-l-m-yaz-lacak-shimmering-yeti.md`

---

## Mimari (katmanlar)

```
App/                      ← bu repoda yazılan, taşınabilir uygulama kodu
  bsp/                    board pin/clock soyutlaması
  bus/                    i2c_bus_if, uart_if (arayüzler) + HW/SW implementasyonları
  common/                 crc, ringbuf, units, log (saf C, host-testable)
  drivers/                bmi270, bmp581, sht4x, gps_ublox, ws2812, servo, fan, buzzer, lock, battery
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

Bu repo yalnızca taşınabilir `App/` katmanını ve testleri içerir. ST'nin ürettiği `Core/`, `Drivers/`, `Middlewares/` katmanı CubeMX'te bir kez üretilir, sonra `App/` eklenir:

1. **CubeMX** ile yeni proje: STM32F405VGTx, Toolchain = STM32CubeIDE.
2. **Clock:** HSE 8 MHz kristal → PLL → SYSCLK 168 MHz (APB1 42, APB2 84).
3. **Çevre birimleri** (pin haritası `App/bsp/board_config.h` ile bire bir):
   - USART1 (PA9/PA10) — GPS, DMA RX + global interrupt, IDLE açık
   - I2C1 (PB6/PB7), I2C2 (PB10/PB11) — DMA, Fast Mode 400 kHz
   - TIM1_CH1 (PA8) — servo PWM 50 Hz
   - TIM5_CH2/CH3 (PA1/PA2) — WS2812, DMA
   - TIM12_CH1 (PB14) — buzzer
   - ADC1_IN10 (PC0) — BAT_SENSE
   - GPIO: PC13 (LOCK_CTRL, out), PB12/PB13 (FAN1/FAN2, out), PB8/PB9 (bit-bang I2C, OD)
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

> CubeMX handle isimleri kod tarafında beklenen adlarla eşleşmeli: `hi2c1`, `hi2c2`, `huart1`, `htim1`, `htim5`, `htim12`, `hadc1`. Farklıysa `App/bsp/board_config.h` güncellenir.

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
| F2 | Sensör sürücüleri (sht4x, bmp581, gps/nmea, bmi270-wrapper) | ✅ |
| F3 | Aktüatör/IO sürücüleri (servo, buzzer, fan, lock, battery, ws2812) | ✅ |
| F4–F5 | Sağlık/güç + RTOS entegrasyon (filters, system_state, sensor_manager, health/IWDG, app+görevler) | ✅ |
| F6–F7 | Telemetri + görev makinesi | ⬜ |
| F8–F10 | Config/NV + sağlamlık + dokümantasyon | ⬜ |

---

## Donanım bulguları (bring-up öncesi netlenmeli)

1. **I2C1 pull-up'ları** (BMI270/BMP581) şemada görünmedi → yoksa bu iki sensör çalışmaz. İlk `i2cscan` ile doğrula.
2. **FAN1/FAN2 (PB12/PB13)** timer OC kanalı yok → varsayılan on/off termostat; değişken hız gerekirse SW PWM/pin değişimi.
3. **WS2812 D6** data 3.3V (level-shift yok) → güvenilirlik gözlenecek.
4. **Telemetri radyosu / SD yok** → transport/storage soyutlandı; donanım eklenince bağlanır.
5. **Ayrılma teyit switch'i yok** → boş GPIO'ya mikro-switch önerilir.
6. **Solenoid fail-safe polaritesi** (enerji=kilit/aç) doğrulanacak.
