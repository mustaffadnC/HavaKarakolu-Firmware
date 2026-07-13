# CubeMX / CubeIDE Kurulum Rehberi — Rev-2 Kart

> Bu dokümandaki her madde `App/bsp/board_config.h` ile bire bir uyumludur.
> `.ioc` üretildikten sonra bu repoya (`docs/` yanına veya kök dizine) commit'lenir.

> **İki kart varyantı var** (bkz. `docs/ee-questions.md` cevap özeti):
> Bu rehber **varsayılan = Şükrü'nün kartına** göredir (SWD doğru, önce bu denenecek).
> İlk kart (`HK_BOARD_REV2A`) için farklar §9'da.

## 0. Kurulum

1. [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) indir ve kur. ✅ Bu makinede kurulu: `C:\ST\STM32CubeIDE_2.2.0`.
2. ⚠️ **CubeIDE 2.x, CubeMX'i İÇERMEZ** (`.ioc` açınca "Install stm32cubeMX program" hatası verir). [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) standalone olarak ayrıca indirilip kurulmalı. Kod üretimi CubeMX'te yapılır (Toolchain = STM32CubeIDE), proje CubeIDE'de derlenir.
3. İlk açılışta workspace olarak repo DIŞINDA bir klasör seç (örn. `C:\Users\pc\STM32CubeIDE\workspace`). CubeMX'in ürettiği `Core/Drivers/Middlewares` bu repoya **girmez** (politika: yalnız taşınabilir `App/` versiyonlanır).

## 1. Proje oluşturma

> **KISA YOL — hazır `.ioc` repoda:** [`hk-capsule-fw.ioc`](../hk-capsule-fw.ioc)
> (Şükrü kartı varyantına göre elle hazırlandı). CubeIDE'de:
> **File → New → STM32 Project from an Existing STM32CubeMX Configuration File (.ioc)**
> → repodaki `hk-capsule-fw.ioc`'yi seç. Sürüm taşıma (migrate) sorarsa kabul et.
> Açıldıktan sonra §2–§5'teki değerleri pinout/clock ekranından **gözle doğrula**
> (dosya elle yazıldığı için bu doğrulama şart), sonra §6–§7 ile devam et.

Sıfırdan kurmak istersen:
- File → New → STM32 Project → MCU seç: **STM32F405VGTx** (LQFP100).
- Proje adı: `hk-capsule-fw` (workspace içinde).

## 2. Saat ağacı (RCC)

> ✅ Kristal **8 MHz teyitli** (S2 cevabı).

- RCC → HSE: **Crystal/Ceramic Resonator**.
- Clock Configuration sekmesi: Input 8 MHz → PLL: **M=8, N=336, P=2** → SYSCLK **168 MHz**; AHB 168, **APB1 42** (timer 84), **APB2 84** (timer 168).
- LSE yok (PC14/PC15 boş).

## 3. Çevre birimleri (pinout)

| Birim | Ayar | Pinler |
|---|---|---|
| **USART1** | Asynchronous, 9600 8N1 (GPS varsayılanı) | PA9 TX, PA10 RX |
| USART1 DMA | RX: **Circular** mode DMA kanalı + NVIC global interrupt açık; IDLE line kullanılır | — |
| **I2C1** | I2C, Fast Mode **400 kHz** | PB6 SCL, PB7 SDA |
| **I2C2** | I2C, Fast Mode **400 kHz** | PB10 SCL, PB11 SDA |
| **SPI1** | Full-Duplex Master, 8-bit, Motorola, **prescaler 256 ile başla** (~328 kHz; sürücü çalışma anında hızlandırır), NSS: **Disable** | PA5 SCK, PA6 MISO, PA7 MOSI |
| **TIM1** | CH1 PWM Generation; PSC=167 (1 µs tick), ARR=19999 (50 Hz) | PA8 |
| **TIM12** | CH1 PWM Generation (buzzer; PSC/ARR'yi kod ayarlar) | **PB14** |
| **IWDG** | Etkin; prescaler 64, reload ~2 s | — |

> Bu kartta ADC yok (BAT_TEST bağlanmamış, PC0 boş) — ADC1 açılmaz.

## 4. GPIO

| Pin | Mod | Başlangıç | Etiket (User Label) |
|---|---|---|---|
| **PA4** | GPIO_Output | **HIGH** (CS pasif) | SD_CS |
| **PB0** | GPIO_Output | **LOW** (enerjisiz = kilitli, S3) | SOLENOID |
| PB12 | GPIO_Output | LOW | FAN1 |
| **PB13** | GPIO_Output | LOW | FAN2 |
| PB8 | GPIO_Output **Open-Drain**, pull yok | HIGH | SWI2C_SCL |
| PB9 | GPIO_Output **Open-Drain**, pull yok | HIGH | SWI2C_SDA |
| **PC4** | GPIO_EXTI4 (Rising), NVIC EXTI4 açık | — | BMI270_INT (opsiyonel) |

> PA0–PA3, PB5, PB15, PC0 ve diğer işaretsiz pinler bu kartta boş — atama yapılmaz.

## 5. SYS / FreeRTOS

- SYS → Debug: **Serial Wire** (PA13/PA14), Timebase Source: **TIM6** (SysTick FreeRTOS'a kalır).
- Middleware → FREERTOS: **CMSIS_V2**. Görevleri CubeMX'te TANIMLAMA — görevler `App/tasks/app.c`'de `xTaskCreate` ile kurulur. Default task kalabilir (boşta döner) ya da silinir.
- FreeRTOS Heap: `configTOTAL_HEAP_SIZE` ≥ **32 KB** yap (görev stack'leri buradan gelir).

## 6. Proje ayarları (Project Manager sekmesi)

- Toolchain: STM32CubeIDE.
- Code Generator: "Generate peripheral initialization as a pair of .c/.h" işaretle.
- Beklenen handle adları (kod bunlara bağlanır): `hi2c1, hi2c2, huart1, htim1, htim12, hspi1, hiwdg`.

## 7. Kod üretimi sonrası entegrasyon

1. `App/` klasörünü projeye bağla: Project → Properties → C/C++ General → Paths and Symbols →
   - **Source Location** → Link Folder → bu reponun `App/` klasörü.
   - **Includes** → `App/` kökü **ve** `App/third_party/fatfs` **ve** `App/drivers/bmi270/vendor` ekle.
2. Preprocessor sembolü: **`HK_USE_BMI270`** tanımla (Paths and Symbols → Symbols).
3. Linker ayarları:
   - `STM32F405RGTX_FLASH.ld` içinde `FLASH ... LENGTH = 1024K` satırını **`896K`** yap (son sektör 11 config NV alanı — `App/bsp/nv_flash_stm32.c`).
   - CSV'de float yazımı için: Project → Properties → C/C++ Build → Settings → MCU Settings → **"Use float with printf"** işaretle (`-u _printf_float`).
4. `main.c` içinde (CubeMX init'lerinden sonra, scheduler'dan önce):
   ```c
   /* USER CODE BEGIN 2 */
   #include "tasks/app.h"    // dosyanın başına, USER CODE Includes bölümüne
   hk_app_init();
   hk_app_start();
   /* USER CODE END 2 */
   ```
   CubeMX default task'ı kaldıysa dokunma; `osKernelStart()` zaten çağrılıyor.
5. Derle (Ctrl+B). Hata çıkmamalı; çıkarsa handle adlarını (madde 6) kontrol et.
6. Üretilen **`.ioc` dosyasını bu repoya kopyala ve commit'le** (tek metin dosyası; `Core/` klasörü repoya girmez).

## 8. Headless derleme (bu makinede yapıldı ✅)

`.ioc`'den kod üretimi ve firmware derlemesi bu makinede tamamlandı:

1. **Kod üretimi** (CubeMX CLI, GUI'siz):
   ```bash
   MX="$LOCALAPPDATA/Programs/STM32CubeMX"
   printf 'config load <proje>\\hk-capsule-fw.ioc\nproject generate\nexit\n' > gen.txt
   "$MX/jre/bin/java.exe" -jar "$MX/STM32CubeMX.exe" -q gen.txt
   ```
   (Gerekirse önce `swmgr install stm32cube_f4_1.28.3 ask` ile HAL paketi indirilir.)
2. **App bağlama:** proje köküne repo `App/` klasörü junction'lanır
   (`cmd /c mklink /J <proje>\App <repo>\App`).
3. **Derleme:** `tools/target-build/build_hk.sh` (ST'nin arm-none-eabi-gcc'si).
   Linker `STM32F405VGTX_FLASH.ld` içinde FLASH **896K**'ya çekilir (NV sektörü).

**Sonuç (Şükrü varyantı, `HK_BOARD_SUKRU`):** 0 uyarı ile derlendi.

| Bölge | Kullanım | Boyut | % |
|---|---|---|---|
| FLASH | 92.5 KB | 896 KB | %10 |
| RAM | 50.5 KB | 128 KB | %38 |
| CCMRAM | 0 | 64 KB | %0 |

`hk-capsule-fw.elf` + `.bin` hazır; PCB gelince ST-Link ile doğrudan yüklenebilir.

## 9. Bilinen açık noktalar

- BMI270 INT (PC4/EXTI4) şimdilik opsiyonel: kod polling ile çalışıyor; EXTI yolu bring-up'ta etkinleştirilecek.
- Batarya ölçümü yalnız `HK_BOARD_REV2A`'da derlenir (Şükrü kartında PC0 boş, sürücü tamamen dışlanıyor).

## 10. HK_BOARD_REV2A varyantı (ilk kart — yedek)

İlk kart denenmek zorunda kalınırsa **ayrı bir `.ioc`** gerekir; farklar:

| Konu | Şükrü (varsayılan) | REV2A |
|---|---|---|
| Buzzer | TIM12_CH1, PB14 | **TIM3_CH2, PB5** (handle `htim3`) |
| FAN2 | PB13 | **PB15** |
| Batarya | yok | **ADC1_IN10, PC0** (handle `hadc1`) |
| SWD | PA13/PA14 çalışır | **BOZUK** → BOOT0+UART bootloader (`bringup.md` §3); PB13/PB14'e hiçbir şey atanmaz |
| Derleyici sembolü | — | **`HK_BOARD_REV2A`** tanımla (Paths and Symbols → Symbols) |
