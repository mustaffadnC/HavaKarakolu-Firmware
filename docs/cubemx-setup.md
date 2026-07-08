# CubeMX / CubeIDE Kurulum Rehberi — Rev-2 Kart

> Bu dokümandaki her madde `App/bsp/board_config.h` ile bire bir uyumludur.
> `.ioc` üretildikten sonra bu repoya (`docs/` yanına veya kök dizine) commit'lenir.

## 0. Kurulum (bu makinede henüz yok)

1. [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) indir (ücretsiz, ST hesabı ister) ve kur. CubeMX, CubeIDE'nin içinde gömülü gelir; ayrıca kurmaya gerek yok.
2. İlk açılışta workspace olarak repo DIŞINDA bir klasör seç (örn. `C:\Users\pc\STM32CubeIDE\workspace`). CubeMX'in ürettiği `Core/Drivers/Middlewares` bu repoya **girmez** (politika: yalnız taşınabilir `App/` versiyonlanır).

## 1. Proje oluşturma

- File → New → STM32 Project → MCU seç: **STM32F405VGTx** (LQFP100).
- Proje adı: `hk-capsule-fw` (workspace içinde).

## 2. Saat ağacı (RCC)

> ⚠️ **Y2 kristal frekansı elektronik ekibinden teyit bekliyor** (`docs/ee-questions.md` S2). Aşağıdakiler 8 MHz varsayımıyla; farklı çıkarsa yalnız "Input frequency" ve PLL M değişir.

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
| **TIM3** | CH2 PWM Generation (buzzer; PSC/ARR'yi kod ayarlar) | **PB5** |
| **ADC1** | IN10, 12-bit, sürekli değil (kod tetikler) | PC0 |
| **IWDG** | Etkin; prescaler 64, reload ~2 s | — |

## 4. GPIO

| Pin | Mod | Başlangıç | Etiket (User Label) |
|---|---|---|---|
| **PA4** | GPIO_Output | **HIGH** (CS pasif) | SD_CS |
| **PB0** | GPIO_Output | **LOW** (solenoid enerjisiz!) | SOLENOID |
| PB12 | GPIO_Output | LOW | FAN1 |
| **PB15** | GPIO_Output | LOW | FAN2 |
| PB8 | GPIO_Output **Open-Drain**, pull yok | HIGH | SWI2C_SCL |
| PB9 | GPIO_Output **Open-Drain**, pull yok | HIGH | SWI2C_SDA |
| **PC4** | GPIO_EXTI4 (Rising), NVIC EXTI4 açık | — | BMI270_INT (opsiyonel) |

> 🔴 **PB13 ve PB14'e HİÇBİR ŞEY atama.** Şemada SWD netleri bu pinlerde görünüyor (`docs/ee-questions.md` S1); cevap gelene kadar bu pinler girişte/analogda kalmalı.
> PA1/PA2 (eski WS2812) rev-2'de boş — atama yapılmaz.

## 5. SYS / FreeRTOS

- SYS → Debug: **Serial Wire** (PA13/PA14), Timebase Source: **TIM6** (SysTick FreeRTOS'a kalır).
- Middleware → FREERTOS: **CMSIS_V2**. Görevleri CubeMX'te TANIMLAMA — görevler `App/tasks/app.c`'de `xTaskCreate` ile kurulur. Default task kalabilir (boşta döner) ya da silinir.
- FreeRTOS Heap: `configTOTAL_HEAP_SIZE` ≥ **32 KB** yap (görev stack'leri buradan gelir).

## 6. Proje ayarları (Project Manager sekmesi)

- Toolchain: STM32CubeIDE.
- Code Generator: "Generate peripheral initialization as a pair of .c/.h" işaretle.
- Beklenen handle adları (kod bunlara bağlanır): `hi2c1, hi2c2, huart1, htim1, htim3, hspi1, hadc1, hiwdg`.

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

## 8. Bilinen açık noktalar

- Kristal ≠ 8 MHz çıkarsa: yalnız §2 değişir; UART baud ve tüm timer hesapları SYSCLK'ten türediği için başka değişiklik gerekmez.
- BMI270 INT (PC4/EXTI4) şimdilik opsiyonel: kod polling ile çalışıyor; EXTI yolu bring-up'ta etkinleştirilecek.
