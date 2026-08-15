# Devir Teslim — Sıfırdan Kurulum ve Proje Durumu

> **Bu dosya kimin için:** projeye **hiçbir programın kurulu olmadığı yeni bir
> bilgisayardan** devam edecek kişi (ya da Claude Code oturumu) için yazıldı.
> Kurulumdan başlayıp donanım devreye alma noktasına kadar her adımı içerir.
>
> **Son güncelleme:** 2026-08-01 · **Durum:** firmware hazır ve doğrulanmış,
> PCB elde, devreye alma yarıda — I2C hattında bir montaj sorunu var (§8).

---

## 1. Proje nedir

**Ülgen ÇARGE** — insansız hava aracına asılan, taşıyıcıdan ayrılıp inen bir
**modüler kargo/görev kapsülü**. Bu repo kapsülün **gömülü yazılımıdır**.

Kapsül ne yapar: taşıyıcıdan servo + solenoid mandalla ayrılır, iniş boyunca ve
yerde ortam verisini ölçer, **SD karta kaydeder**, iki fanla termal kontrol
yapar, indikten sonra buzzer'la sesli işaret vererek bulunmayı kolaylaştırır.

| | |
|---|---|
| **MCU** | STM32F407VGT6 (Cortex-M4F @168 MHz, 1 MB Flash, 192 KB RAM) |
| **RTOS / dil** | FreeRTOS (CMSIS-RTOS v2) + C11 |
| **Sensörler** | 2× SHT4x (sıcaklık/nem), BMP581 (basınç/irtifa), BMI270 (IMU), MAX-M10S (GPS) |
| **Aktüatörler** | Servo (TIM1_CH1), solenoid kilit (PB0), 2× fan, buzzer (TIM3_CH2) |
| **Veri çıkışı** | **Yalnızca SD kart.** Telemetri radyosu YOK — bilinçli tasarım kararı |
| **Depo** | `https://github.com/mustaffadnC/HavaKarakolu-Firmware` (private) |

> ⚠️ **Kapsam sınırı:** Bu proje **yalnızca kapsül firmware'idir**. Haberleşme,
> ROS 2/Docker, yapay zekâ ve yer kontrol istasyonu bu repoda **yoktur**.
> `kerkenez-gcs` diye ayrı bir proje var ama o **kişisel portföy çalışmasıdır**,
> ÇARGE/TULPAR ürününün parçası değildir — ikisini karıştırma.

---

## 2. En kritik yapısal bilgi: repo tek başına derlenmez

Bu repo **yalnızca taşınabilir `App/` katmanını ve testleri** içerir.
ST'nin ürettiği `Core/`, `Drivers/` (HAL), `Middlewares/` (FreeRTOS) katmanı
**repoda değildir** ve **repo dışında** bir CubeMX projesinde durur:

```
C:\Users\pc\Desktop\projeler\HavaKarakolu-Firmware\   ← repo (App/, tests/, docs/)
C:\Users\pc\STM32CubeIDE\hk-capsule-fw\               ← CubeMX projesi (repoda YOK)
        ├── Core/  Drivers/  Middlewares/             ← CubeMX üretir
        ├── App  →  repo'nun App/ klasörüne JUNCTION
        ├── STM32F407VGTX_FLASH.ld                    ← elle 896K'ya çekilmiş
        ├── build_hk.sh                               ← repodaki kopyanın aynısı
        └── hk-capsule-fw.ioc                         ← repodaki .ioc'nin kopyası
```

**Yeni bilgisayarda bu dış projeyi yeniden kurman gerekir** (§5). Repodaki
`hk-capsule-fw.ioc` bunun için gereken tek girdi dosyasıdır.

`tests/host/` ise **hiçbir dış bağımlılık istemez** — sadece gcc + cmake yeter.
Yani sensör/mantık geliştirmesi yapacaksan §5'i hiç yapmadan çalışabilirsin.

---

## 3. Sıfırdan kurulum — sırayla

Aşağıdakiler Windows içindir (proje bu ortamda geliştirildi). Komutlar
**PowerShell**'de çalıştırılır; sonrasında derleme komutları **Git Bash**'te.

### 3.1 Zorunlu: Git

```powershell
winget install Git.Git
```

Kurulumla birlikte **Git Bash** gelir — bu projedeki tüm derleme scriptleri
bash'tir, PowerShell'de çalışmaz.

### 3.2 Zorunlu: repoyu klonla

```bash
cd ~/Desktop
mkdir -p projeler && cd projeler
git clone https://github.com/mustaffadnC/HavaKarakolu-Firmware.git
cd HavaKarakolu-Firmware
```

Private repo olduğu için GitHub kimlik doğrulaması ister. Git Credential
Manager kuruluysa tarayıcıdan giriş yapar.

### 3.3 Host testleri için: gcc + cmake

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
winget install Kitware.CMake
```

Doğrulama (Git Bash'te, terminali yeniden aç):

```bash
gcc --version && cmake --version
```

Bu makinede çalışan sürümler: gcc 16.1.0 (MinGW-W64 UCRT), cmake 4.3.2.

### 3.4 Firmware derlemek için: STM32CubeIDE

[st.com](https://www.st.com/en/development-tools/stm32cubeide.html) → indir → kur.
Bu makinede `C:\ST\STM32CubeIDE_2.2.0` altına kuruldu.

**Neden gerekli:** ARM derleyicisini (`arm-none-eabi-gcc`) ve **CubeProgrammer'ı**
içinde getiriyor — ikisini ayrıca kurmana gerek yok. Yolları:

```
ARM derleyici:
C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\

CubeProgrammer CLI (karta yükleme + hafıza okuma):
C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe
```

> Sürüm numaraları klasör adında geçiyor; farklı CubeIDE sürümünde bu adlar
> değişir. Bulmak için: `find /c/ST -iname "STM32_Programmer_CLI.exe"`

### 3.5 Kod üretmek için: STM32CubeMX (ayrı program!)

⚠️ **CubeIDE 2.x, CubeMX'i İÇERMEZ.** `.ioc` açmak istersen "Install
stm32cubeMX program" hatası verir. Ayrıca indir:
[st.com/stm32cubemx](https://www.st.com/en/development-tools/stm32cubemx.html)

Bu makinede: `%LOCALAPPDATA%\Programs\STM32CubeMX`, sürüm **6.15.0**.

### 3.6 Log okuyucu için: Python

```powershell
winget install Python.Python.3.12
```

⚠️ Windows'ta `python` komutu Microsoft Store kısayoluna gidiyor ve çalışmıyor.
Gerçek yol: `C:\Users\<kullanıcı>\AppData\Local\Programs\Python\Python312\python.exe`
Git Bash'te `py` launcher da kullanılabilir.

### 3.7 Opsiyonel: Node.js

Yalnızca Havelsan soru-cevap Word dosyasını yeniden üretmek için gerekiyor
(`docx` npm paketi). Firmware için gerekmez.

---

## 4. Host testlerini çalıştır (en hızlı doğrulama)

```bash
cd tests/host
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

**13 test hedefi** var, hepsi geçmeli.

> ⚠️ **Windows Smart App Control (SAC) tuzağı:** Bu geliştirme makinesinde SAC
> "enforce" modundaydı ve **yerelde derlenen imzasız exe'lerin çalıştırılmasını
> engelliyordu** (`WinError 4551`). Derleme sorunsuz oluyordu ama `ctest`
> çalışmıyordu. Yeni makinede SAC kapalıysa sorun yok. Açıksa ya kapat ya da
> testleri yalnızca CI'da (GitHub Actions, Ubuntu) koştur — CI her push'ta
> zaten çalışıyor ve şu an **yeşil**.

---

## 5. CubeMX projesini yeniden kur (firmware derlemek için)

Bu, repoda olmayan katmanı üretme adımıdır. **Dikkatli yap, birkaç elle
müdahale gerekiyor.**

### 5.1 Kodu üret

`.ioc` dosyasını hedef klasöre kopyala ve CubeMX'i komut satırından çalıştır:

```bash
mkdir -p /c/Users/$USER/STM32CubeIDE/hk-capsule-fw
cp hk-capsule-fw.ioc /c/Users/$USER/STM32CubeIDE/hk-capsule-fw/

MX="$LOCALAPPDATA/Programs/STM32CubeMX"
cat > gen.txt <<'EOF'
config load C:/Users/pc/STM32CubeIDE/hk-capsule-fw/hk-capsule-fw.ioc
project generate
exit
EOF
"$MX/jre/bin/java.exe" -jar "$MX/STM32CubeMX.exe" -q gen.txt
```

> ⚠️ **Yol tuzağı:** Komut dosyasında **ileri bölü** (`/`) kullan. `printf` ile
> `C:\Users\...` yazarsan bash `\U` dizisini unicode kaçış sanıp hata verir.
> Yukarıdaki heredoc bu yüzden böyle yazıldı.
>
> ⚠️ HAL paketi yoksa CubeMX indirir; bu makinede **STM32Cube FW_F4 V1.28.3**
> kullanıldı (`C:\Users\pc\STM32Cube\Repository\` altında).

### 5.2 App klasörünü junction'la

```powershell
cmd /c mklink /J C:\Users\pc\STM32CubeIDE\hk-capsule-fw\App C:\Users\pc\Desktop\projeler\HavaKarakolu-Firmware\App
```

### 5.3 Linker'ı 896K'ya çek (ZORUNLU)

`STM32F407VGTX_FLASH.ld` içinde:

```
FLASH (rx) : ORIGIN = 0x8000000, LENGTH = 896K   /* 1024K DEĞİL! */
```

**Neden:** flash'ın son sektörü (sektör 11, 128 KB) **ayar saklama alanı**
olarak kullanılıyor (`App/bsp/nv_flash_stm32.c`). 1024K bırakırsan firmware
kendi ayarlarının üstüne yazar.

### 5.4 TIM3 + ADC1 için elle müdahale (ZORUNLU, tuhaf ama gerekli)

⚠️ **Headless CubeMX 6.15, `.ioc`'ye elle eklenen ADC/TIM bloklarını migrate
sırasında sessizce siliyor.** İki denemede de pinleri kabul edip çevre
birimlerini attı; `MX_TIM3_Init`/`MX_ADC1_Init` üretmedi ve `gpio.c`'de PB5'i
AF moduna alıp `Alternate` alanını hiç doldurmadı.

Bu yüzden **buzzer (TIM3) ve batarya ölçümü (ADC1) CubeMX'te değil,
`App/bsp/periph_tim3_adc1_stm32.c` içinde kuruluyor** — repoda, versiyonlu,
CubeMX ne yaparsa yapsın bozulmaz.

Bunun iki yan gereksinimi var:

**a)** HAL ADC modülü `stm32f4xx_hal_conf.h`'de kapalı kalıyor; derleme bayrağı
`-DHAL_ADC_MODULE_ENABLED` ile açılıyor (`build_hk.sh` içinde zaten var).

**b)** CubeMX ADC dosyalarını kopyalamıyor, **5 dosyayı elle kopyala**:

```bash
R=/c/Users/$USER/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/STM32F4xx_HAL_Driver
P=/c/Users/$USER/STM32CubeIDE/hk-capsule-fw/Drivers/STM32F4xx_HAL_Driver
cp "$R/Inc/stm32f4xx_hal_adc.h" "$R/Inc/stm32f4xx_hal_adc_ex.h" "$R/Inc/stm32f4xx_ll_adc.h" "$P/Inc/"
cp "$R/Src/stm32f4xx_hal_adc.c" "$R/Src/stm32f4xx_hal_adc_ex.c" "$P/Src/"
```

### 5.5 Startup dosyası

CubeMX F407 için `Core/Startup/startup_stm32f407vgtx.s` üretmeli. Üretmezse
ST'nin CMSIS şablonunu kopyala (projede bu yapıldı, ikisi zaten birebir aynı):

```bash
cp "$R/../CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s" \
   /c/Users/$USER/STM32CubeIDE/hk-capsule-fw/Core/Startup/startup_stm32f407vgtx.s
```

### 5.6 main.c kancaları

CubeMX `main.c`'yi yeniden üretir ama USER CODE bloklarını korur. İçinde şunlar
olmalı (yoksa ekle):

```c
/* USER CODE BEGIN Includes */
#include "tasks/app.h"
/* USER CODE END Includes */

/* ... MX_*_Init() çağrılarından sonra, osKernelStart()'tan önce ... */
/* USER CODE BEGIN 2 */
hk_app_init();     /* bus/sensör/aktüatör init, fail-safe LOCK */
hk_app_start();    /* mutex'ler + FreeRTOS görevleri */
/* USER CODE END 2 */
```

### 5.7 Derle

```bash
cp tools/target-build/build_hk.sh /c/Users/$USER/STM32CubeIDE/hk-capsule-fw/
cd /c/Users/$USER/STM32CubeIDE/hk-capsule-fw
bash build_hk.sh
```

**Beklenen sonuç (2026-08-01 itibarıyla):**

```
RAM:    50600 B / 128 KB  (38.60%)
FLASH:  94220 B / 896 KB  (10.27%)
=== BUILD OK: build_hk/hk-capsule-fw.elf ===
```

0 uyarı çıkmalı. Çıktı: `build_hk/hk-capsule-fw.elf` ve `.bin`.

---

## 6. Karta yükleme ve hafıza okuma

CubeProgrammer CLI ile (ayrı program kurmaya gerek yok, CubeIDE içinde):

```bash
P="/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin"

# Bağlantı testi
"$P/STM32_Programmer_CLI.exe" -c port=SWD mode=HOTPLUG freq=1800

# Yükle + doğrula + çalıştır
"$P/STM32_Programmer_CLI.exe" -c port=SWD mode=UR freq=1800 \
    -w build_hk/hk-capsule-fw.elf -v --go

# Hafızadan 32-bit oku (örn. FreeRTOS tick)
"$P/STM32_Programmer_CLI.exe" -q -c port=SWD mode=HOTPLUG freq=1800 -r32 0x200004E0 4

# Çekirdek register'ları (PC, LR, SP...) — çekirdeği DURDURUR
"$P/STM32_Programmer_CLI.exe" -q -c port=SWD mode=HOTPLUG freq=1800 --coreReg
```

**Sembol adresi bulmak için:**

```bash
T="/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
"$T/arm-none-eabi-nm.exe" build_hk/hk-capsule-fw.elf | grep -iw "uwTick"

# Adresi kaynak satırına çevir (takılma teşhisi için paha biçilmez)
"$T/arm-none-eabi-addr2line.exe" -f -C -e build_hk/hk-capsule-fw.elf 0x08002DC2

# Yapı üyelerinin offset'leri
"$T/arm-none-eabi-gdb.exe" -batch -ex "ptype /o hk_system_state_t" build_hk/hk-capsule-fw.elf
```

**Devreye alma sırasında kullanılan adresler** (her derlemede değişebilir,
`nm` ile tazele):

| Sembol | Adres | Ne işe yarar |
|---|---|---|
| `uwTick` | `0x20000478` | HAL zaman sayacı — artmıyorsa kesmeler maskeli |
| `xTickCount` | `0x200004E0` | FreeRTOS tick — artmıyorsa zamanlayıcı durmuş |
| `uxCriticalNesting` | `0x2000002C` | `0xAAAAAAAA` ise zamanlayıcı hiç başlamamış |
| `s_state` | `0x200092E0` | Sistem durumu (offset 124 = `sensor_ok`) |

> ⚠️ **Ölçüm tuzağı:** Her CLI bağlantısı kartı **resetliyor**. Bu yüzden
> ardışık iki okuma "sayaç artmıyor" gibi görünür — aslında her okuma taze bir
> açılıştan ~0,7 sn sonrasını gösterir. Uzun süreli davranışı görmek için SD
> kart logunu kullan, SWD'yi değil.

---

## 7. Donanım — kullanılacak TEK kart

İlk üretilen kart (SWD'si yanlış bağlı olan) **tasarımdan çıkarıldı**. Kodda
kart varyantı/derleme sembolü **yok**. Pin haritası şemayla satır satır
doğrulandı (2026-07-29). Tek doğru kaynak: `App/bsp/board_config.h`.

| İşlev | Pin | Not |
|---|---|---|
| MCU | — | Şemada STM32F405 yazar, kartta **F407VGT6** takılı (uyumlu) |
| SWD | PA13 / PA14 | ✅ Doğru — ST-Link doğrudan çalışır |
| Buzzer | PB5 / TIM3_CH2 | |
| FAN1 / FAN2 | PB12 / PB15 | On-off termostat: 40 °C aç, 35 °C kapa |
| Solenoid | PB0 | **Enerjisiz = KİLİTLİ** (fail-safe, donanımda doğrulandı) |
| Servo | PA8 / TIM1_CH1 | |
| Batarya ölçümü | PC0 / ADC1_IN10 | 100k/10k bölücü (÷11) |
| I2C1 | PB6 / PB7 | BMI270 (0x68) + BMP581 (0x46), 4,7K pull-up |
| I2C2 | PB10 / PB11 | SHT4x #1 (0x44) |
| Bit-bang I2C | PB8 / PB9 | SHT4x #2 (0x44) — **ayrı hat, bu yüzden aynı adres sorun değil** |
| GPS | PA9 / PA10 (USART1) | MAX-M10S, DMA RX |
| SD kart | PA4-PA7 (SPI1) | 5V Arduino tipi modül |
| **Boşta** | PB13, PB14, D/E portları | Yeni özellik için yer var |

---

## 8. 🔴 DEVREYE ALMA DURUMU — buradan devam edilecek

PCB 2026-08-01'de geldi. İlk devreye alma yapıldı. **Firmware çalışıyor**, ama
donanımda iki açık sorun var.

### 8.1 Çalıştığı doğrulananlar ✅

- ST-Link SWD bağlantısı (Device ID `0x413`, 1 MB flash)
- Firmware yükleme ve doğrulama
- **8 MHz kristal salınıyor, PLL kilitli, sistem 168 MHz'de** (RCC register'larından okundu)
- FreeRTOS zamanlayıcı çalışıyor, her iki tick sayacı ~1 kHz'de ilerliyor
- Görev durum makinesi SELFTEST fazına ulaşıyor, buzzer BOOT desenini çalıyor
- **Solenoid pini düşük = kilitli** — fail-safe gerçek donanımda doğrulandı
- Fanlar kapalı (termostat 35 °C altında doğru davranıyor)
- Kartın kendi güç zinciri (12 V → 5 V → 3,3 V): LED'ler yanıyor

### 8.2 🔴 Sorun 1: I2C1 veri hattı toprağa çekili

**Belirti:** PB7 (I2C1_SDA) sürekli **düşük**. PB6 (SCL) normal yüksek.
I2C1 durum register'ında **ARLO** (arbitration lost) bayrağı kalkmış.
Sonuç: BMP581 ve BMI270 hiç cevap vermiyor (`sensor_ok = 0`).

**Denenen:** Yazılımsal bus-clear (9 SCL darbesi) eklendi ve çalıştırıldı —
**düzelmedi.** Yani takılı bir köle değil, fiziksel bir sorun.

**Elektronikçiye iletilecek mesaj:**
> I2C1 veri hattı (SDA, MCU pin 93 / PB7) sürekli toprağa çekili duruyor. Saat
> hattı (SCL, pin 92) normal. Yazılımsal bus-clear denendi, düzelmedi. Lütfen
> SDA pull-up direncini (R11, 4,7K) ve BMP581'in boş footprint'indeki lehim
> köprülerini kontrol edin.

**En olası iki sebep:** R11 takılmamış/lehimi kötü, ya da SDA hattı toprağa
kısa devre (muhtemelen boş BMP581 pad'lerinde köprü).

### 8.3 🔴 Sorun 2: Kart 12 V ile beslenirken SWD kurulmuyor

**Belirti tablosu:**

| Besleme | SWD |
|---|---|
| ST-Link'in 3,3 V ucu takılı | ✅ Bağlanıyor |
| Sadece 12 V (3,3 V ucu sökülü) | ❌ "Unable to get core ID" |

**İki açıklama var, henüz ayrılmadı:**

- **(A)** Kartın kendi 3,3 V'u MCU'ya ulaşmıyor. Yanan LED'ler 5 V/12 V
  hattında olabilir, 3,3 V regülatörü çalışmıyor olabilir → gerçek donanım sorunu.
- **(B)** ST-Link o ucu **hedef gerilim referansı (VTref)** olarak istiyor.
  Kart çalışıyor olabilir ama ST-Link referans görmediği için iletişim kurmuyor
  → sadece kablo meselesi.

**AYIRT ETME YÖNTEMİ (sıradaki iş):** 12 V takılıyken multimetreyle kartın
**3,3 V noktası ile toprak arasını ölç.**
- ~3,3 V okunursa → (B) doğru, kart sağlam. 12 V ile ST-Link 3,3 V ucunu
  birlikte bağlamak güvenli (istersen araya 100 Ω koy, sadece referans olur).
- 0 V okunursa → (A) doğru, 3,3 V regülatörü çalışmıyor. Bu I2C sorunundan
  **daha öncelikli**, çünkü kart kendi başına ayakta duramıyor demektir.

### 8.4 🟡 Basınç sensörü (BMP581) kartta takılı DEĞİL

Kullanıcı beyanı: kartta basınç sensörü yok. Firmware onu bekliyor ve bulamayınca
degraded moda düşüyor (bu desteklenen bir durum, sistem çalışmaya devam eder).

**Karar gerekiyor:** Sensör sonradan takılacak mı? Kalıcı olarak yoksa görev
mantığı **yalnızca IMU ile** çalışacak şekilde ayarlanmalı — kod bunu zaten
destekliyor (`baro-only / IMU-only` degraded modlar test edilmiş durumda) ama
irtifa tabanlı kurulma (30 m) ve iniş tespiti eşikleri gözden geçirilmeli.

### 8.5 Sıradaki adımlar (öncelik sırasıyla)

1. **3,3 V rayını multimetreyle ölç** (§8.3) — kartın sağlam olup olmadığını bu belirler
2. Elektronikçi I2C1 SDA hattını incelesin (§8.2)
3. Bunlar çözülünce: SD kart takılıp kayıt testi (`LOGS/FL_0001.BIN` + `.CSV`)
4. Sonra sırayla: sensörler → GPS → aktüatörler → IWDG testi → gece soak testi
5. Tam plan: [`docs/bringup.md`](bringup.md)

---

## 9. Devreye almada bulunan ve düzeltilen firmware hataları

Bu üçü **gerçek donanım olmadan yakalanamayacak** hatalardı; kayıt olarak
duruyor ki benzerleri tekrar edilmesin.

### 9.1 Zamanlayıcı öncesi FreeRTOS çağrısı kesmeleri kalıcı maskeliyordu

**Commit:** `245b0cf` · **En kritik olanı buydu.**

`hk_app_init()` içinde mutex oluşturuluyordu. Her FreeRTOS API çağrısı bir
kritik bölge alır; zamanlayıcı başlamadan önce `uxCriticalNesting` hâlâ
`0xAAAAAAAA` nöbetçi değerinde olduğu için çıkışta sayaç sıfıra inmiyor ve
**kesmeler bir daha açılmıyor.** Sonuç: TIM6 tick ölüyor, `HAL_GetTick()` 1'de
donuyor, **bütün HAL zaman aşımları sonsuza dönüşüyor** ve cevap vermeyen ilk
I2C cihazında sistem sonsuza kadar kilitleniyor.

**Çözüm:** Mutex'ler artık `hk_app_start()` içinde, `osKernelStart()`'a birkaç
komut kala oluşturuluyor. Bus modülleri NULL mutex'i zaten "zamanlayıcı öncesi,
kilit gerekmez" diye ele alıyordu.

**Kural:** `hk_app_init()` içinde **asla** FreeRTOS API'si çağırma.

### 9.2 BMP581 sürücüsü deep standby'da yapılandırmayı kaybediyordu

**Commit:** `c7a37ff`

BMP581 soft reset sonrası DEEP STANDBY'a düşer ve o modda yapılandırma
yazmaları **sessizce yutulur**. Sürücü `deep_dis` bitini kurmuyordu → sensör
bütün uçuşu 1× örneklemede geçirebilirdi, kimse fark etmezdi.

**Çözüm:** init artık deep standby'dan çıkıyor, oversampling'i yazıp **geri
okuyarak doğruluyor**; okuma da sabit gecikme yerine cihazın FORCED'dan
STANDBY'a döndüğünü register'dan teyit ediyor.

### 9.3 Varsayılan yapılandırma buzzer/fanı boş pinlere sürüyordu

**Commit:** `cbb2392`

Şema fotoğrafları gelince fark edildi: varsayılan derleme buzzer'ı PB14'e,
FAN2'yi PB13'e sürüyordu — bu kartta o iki pin **boşta**. Hata vermeden,
sessizce çalışmayacaktı.

---

## 10. Bilinen tuzaklar (zaman kaybettirenler)

| Tuzak | Belirti | Çözüm |
|---|---|---|
| **Smart App Control** | `ctest` çalışmıyor, `WinError 4551` | SAC'ı kapat ya da testleri CI'da koştur |
| **CubeMX ADC/TIM'i siliyor** | `MX_ADC1_Init` üretilmiyor | §5.4 — bsp dosyası zaten çözüyor |
| **bash `printf` + `C:\Users`** | `missing unicode digit for \U` | Heredoc + ileri bölü kullan |
| **Her SWD bağlantısı resetliyor** | Sayaçlar artmıyor gibi görünür | Uzun gözlem için SD logu kullan |
| **`python` komutu** | Store kısayoluna gidiyor | Tam yolu ya da `py` kullan |
| **CI'da `Service Unavailable`** | "Set up job" aşamasında kırmızı | GitHub altyapısı, kodla ilgisi yok — rerun |
| **`--coreReg` çekirdeği durdurur** | Sonraki okumalar donmuş görünür | Ölçüm sırasını buna göre kur |

---

## 11. Test ve CI

- **13 host test hedefi**, her push'ta GitHub Actions'ta (Ubuntu) koşuyor — **şu an yeşil**
- Kapsam: CRC, ring buffer, SHT4x, GPS/NMEA, aktüatörler, servisler, BMP581,
  SD-SPI, storage, BMI270, mission (%100 durum kapsamı), config, uçtan uca uçuş
- Uçuş simülatörü (`tests/host/sim/`) irtifa/hız/ivme üretip durum makinesini
  gerçek bir uçuş profilinden geçiriyor, sonuç SD kart imajına yazılıp geri okunuyor
- Derleyici: host tarafında `-Wall -Wextra -Wshadow -Wconversion`, **sıfır uyarı**

**Yeni test eklerken:** `tests/host/test_<x>.c` yaz (kendi `main()`'i olacak,
`return hk_test_summary();` ile bitecek), `tests/host/CMakeLists.txt` içinde hem
`add_executable` hem `foreach(...)` listesine ekle.

---

## 12. Çalışma kuralları (önceki oturumlardan)

- **Commit mesajlarında AI izi YOK** — `Co-Authored-By: Claude` veya
  "Generated with" satırı ekleme. Kesin kural.
- Kod ve commit mesajları **İngilizce**, dokümanlar **Türkçe** (README'de EN özet var).
- Conventional Commits: `feat(bmp581): ...`, `fix(rtos): ...`
- `gh` CLI kurulu ama token scope'u eksik → GitHub API'ye **curl + git-credential-manager
  token'ı** ile gidiliyor:
  ```bash
  TOKEN=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill | grep '^password=' | cut -d= -f2)
  ```
- Vendor kodu (`App/third_party/fatfs`, `App/drivers/bmi270/vendor`) **elle
  düzenlenmez**, gerekirse yeniden içe aktarılır. `-w` ile derlenir.

---

## 13. Diğer dokümanlar

| Dosya | İçerik |
|---|---|
| [`README.md`](../README.md) | Proje özeti, faz tablosu, donanım durumu |
| [`CLAUDE.md`](../CLAUDE.md) | Claude Code için kısa yönerge (komutlar, mimari, kısıtlar) |
| [`docs/bringup.md`](bringup.md) | Adım adım devreye alma planı ve kabul ölçütleri |
| [`docs/cubemx-setup.md`](cubemx-setup.md) | CubeMX/CubeIDE entegrasyonunun tam ayrıntısı |
| [`docs/ee-questions.md`](ee-questions.md) | Elektronik ekibiyle yazışma arşivi (tarihsel) |
| [`App/drivers/README.md`](../App/drivers/README.md) | Sürücü envanteri ve olgunluk durumu |

Masaüstünde ayrıca **`Havelsan_Teknik_Soru_Cevap.docx`** var (33 soruluk teknik
savunma dosyası, repoda değil).

---

## 14. Yeni oturuma hızlı başlangıç özeti

1. §3'teki araçları kur (Git, gcc+cmake, CubeIDE, CubeMX, Python)
2. Repoyu klonla
3. `cd tests/host && cmake -B build -S . -G "MinGW Makefiles" && cmake --build build` → 13 hedef derlenmeli
4. Firmware'e dokunacaksan §5'i uygula (CubeMX projesi + junction + linker + ADC dosyaları)
5. Karta bağlanacaksan §6'daki CubeProgrammer komutlarını kullan
6. **Nerede kaldığımızı öğrenmek için §8'i oku** — sıradaki iş 3,3 V rayını ölçmek
