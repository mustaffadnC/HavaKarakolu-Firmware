# Devir Teslim — Sıfırdan Kurulum ve Proje Durumu

> **Bu dosya kimin için:** projeye **hiçbir programın kurulu olmadığı yeni bir
> bilgisayardan** devam edecek kişi (ya da Claude Code oturumu) için yazıldı.
> Kurulumdan başlayıp donanım devreye alma noktasına kadar her adımı içerir.
>
> **Son güncelleme:** 2026-08-15 · **Durum:** firmware hazır, doğrulanmış ve
> **karta yüklenmiş, kartta koşuyor**. İkinci bir bilgisayara sıfırdan kuruldu
> ve referans derlemeyi **bit-bit** yeniden üretti. NRST kablosu bağlandı,
> §8.6'daki kısır döngü kapandı: yükleme artık CubeProgrammer'ın kendi 92 KB'lık
> doğrulamasından tek seferde geçiyor. §9.4 ve §9.5 kapandı. **Kart 12 V ile
> kendi ayakları üzerinde duruyor (§8.3) — bringup'ı tıkayan asıl madde çözüldü.**
>
> **Açık kalanlar:** §8.2 (I2C1 SDA toprağa çekili) ve §8.3.1 (12 V ile
> beslenirken flash programlanamıyor — yükleme ST-Link 3,3 V'uyla yapılmalı).
>
> ⚠️ **Yükleme/çalıştırma düzeni:** yüklerken 12 V kesik + ST-Link 3,3 V takılı;
> çalıştırırken 3,3 V sökülü + 12 V takılı. **İkisi aynı anda asla bağlanmaz.**

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
<repo>\                                   ← repo (App/, tests/, docs/)
<kullanıcı>\STM32CubeIDE\hk-capsule-fw\   ← CubeMX projesi (repoda YOK)
        ├── Core/  Drivers/  Middlewares/             ← CubeMX üretir
        ├── App  →  repo'nun App/ klasörüne JUNCTION
        ├── STM32F407VGTX_FLASH.ld                    ← elle 896K'ya çekilmiş
        ├── build_hk.sh                               ← repodaki kopyanın aynısı
        └── hk-capsule-fw.ioc                         ← repodaki .ioc'nin kopyası
```

Bu proje iki makinede kuruldu; yollar makineye göre değişir:

| | 1. makine (2026-07/08) | 2. makine (2026-08-15) |
|---|---|---|
| repo | `C:\Users\pc\Desktop\projeler\HavaKarakolu-Firmware` | `C:\dev\HavaKarakolu-Firmware` |
| CubeMX projesi | `C:\Users\pc\STM32CubeIDE\hk-capsule-fw` | `C:\Users\erdem\STM32CubeIDE\hk-capsule-fw` |
| CubeIDE | `C:\ST\STM32CubeIDE_2.2.0` | `C:\ST\STM32CubeIDE_2.2.0` |

> ⚠️ **Repoyu Türkçe karakter içeren bir yola koyma.** İlk denemede repo
> `...\OneDrive\Masaüstü\havakarakolu` altındaydı ve `mingw32-make`, `Masaüstü`
> içindeki `ü` yüzünden kaynak dosyaları bulamadı (`No rule to make target`).
> Ninja jeneratörü çalışıyordu ama dokümandaki `-G "MinGW Makefiles"` komutu
> çalışmıyordu. ASCII bir yola (`C:\dev\...`) taşınınca sorun tamamen bitti.
> Ayrıca OneDrive içinde derleme yapmak `build/` çıktılarını buluta
> senkronlar ve CubeIDE junction'ıyla iyi geçinmez — repoyu OneDrive dışında tut.

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

Doğrulanmış sürümler: 1. makinede gcc 16.1.0 + cmake 4.3.2, 2. makinede
gcc 16.1.0 (MinGW-W64 UCRT) + cmake 4.4.2 — ikisi de sorunsuz.

> ℹ️ WinLibs paketi cmake, ctest, ninja, gdb ve cppcheck'i de beraberinde
> getiriyor; ayrıca `Kitware.CMake` kurmak şart değil ama zararı da yok
> (PATH'te CMake'inki önce gelir).
>
> ⚠️ `winget search`/`install` ilk çalıştırmada **msstore kaynak sözleşmesi**
> için etkileşimli onay ister ve script içinde `0x8a150042` hatasıyla düşer.
> `--source winget` ekleyerek bu kaynağı tamamen atla:
> `winget install BrechtSanders.WinLibs.POSIX.UCRT --source winget`

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

Kurulum yeri: `%LOCALAPPDATA%\Programs\STM32CubeMX` (varsayılanı değiştirme,
scriptler bu yolu arıyor). Doğrulanmış sürümler: **6.15.0** ve **6.18.1**.

> **Kurulum notları (2. makine):** Her iki ST aracı da winget'te **yok**;
> st.com'dan ST hesabıyla giriş yapılarak indiriliyor. CubeIDE kurulumu NSIS
> tabanlı, sessiz kurulabiliyor:
> `stm32cubeide_*.exe /S /D=C:\ST\STM32CubeIDE_2.2.0` (UAC onayı ister).
> CubeMX kurulumu Java/IzPack tabanlı, GUI'den tıklanarak kuruldu.
>
> CubeMX ilk `project generate` çağrısında **STM32Cube FW_F4 V1.28.3**
> paketini kendisi indirdi (~1 GB indirme, açılınca ~4 GB, `%USERPROFILE%\
> STM32Cube\Repository\`). Açma aşamasında ilerleme çubuğu takılmış gibi
> görünüyor ama takılmıyor — on binlerce küçük dosya yazıyor; `Cancel`'a
> basma, dosya sayısı düzenli artıyorsa çalışıyordur.

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

> ⚠️ **Windows Smart App Control (SAC) tuzağı:** 1. geliştirme makinesinde SAC
> "enforce" modundaydı ve **yerelde derlenen imzasız exe'lerin çalıştırılmasını
> engelliyordu** (`WinError 4551`). Derleme sorunsuz oluyordu ama `ctest`
> çalışmıyordu; bu yüzden o makinede "faz bitti" ölçütü CI'ın yeşil olmasıydı.
>
> **2. makinede SAC kapalı ve testler yerelde gerçekten koşuyor — 13/13
> geçiyor.** Yani artık her değişiklikten sonra CI beklemeye gerek yok.
> Kendi makinende durumu şöyle kontrol et:
>
> ```powershell
> Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' VerifiedAndReputablePolicyState
> ```
>
> `0` = kapalı (sorun yok), `1` = enforce (engeller), `2` = değerlendirme.

> ⚠️ **Jeneratör seçimi:** `-G "MinGW Makefiles"` yalnızca repo **ASCII bir
> yolda** ise çalışır (§2). Türkçe karakterli bir yol kaldıysa `-G "Ninja"`
> kullan — WinLibs ninja'yı zaten getiriyor ve aynı 13 testi derleyip koşar.

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

⚠️ **Headless CubeMX, `.ioc`'ye elle eklenen ADC/TIM bloklarını migrate
sırasında sessizce siliyor.** İki denemede de pinleri kabul edip çevre
birimlerini attı; `MX_TIM3_Init`/`MX_ADC1_Init` üretmedi ve `gpio.c`'de PB5'i
AF moduna alıp `Alternate` alanını hiç doldurmadı.

> **6.18.1'de de aynen geçerli** — bu davranış sürüm 6.15'e özgü değil.
> Üretilen `Core/Src/` içinde `adc.c` ve TIM3 yok, `.ioc`'nin IP listesinde de
> yoklar (PB5 `S_TIM3_CH2`, PC0 `ADCx_IN10` olarak yalnızca pin işaretli).
> Bu **beklenen** durum, `bsp/periph_tim3_adc1_stm32.c` çözümü hâlâ şart.

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

### 5.5 Startup dosyası ve linker

CubeMX F407 için `Core/Startup/startup_stm32f407vgtx.s` ve
`STM32F407VGTX_FLASH.ld` üretmeli.

> ✅ **CubeMX 6.18.1 ikisini de kendisi üretti**, elle kopyalamaya gerek
> kalmadı. Üretim bitmeden klasöre bakarsan eksik görünürler — `Core/`,
> `Drivers/`, `Middlewares/`, `.cproject` ve `.ld` hepsi işin sonunda birden
> belirir. Java süreci çıkana kadar bekle.

Üretmediği bir sürümle karşılaşırsan ST'nin CMSIS şablonunu kopyala (ikisi
birebir aynı):

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

**Beklenen sonuç:**

| Sürüm | RAM | FLASH |
|---|---|---|
| 2026-08-01 (referans) | 50600 B (%38,60) | 94220 B (%10,27) |
| 2026-08-15 (log ring + backoff, §9.4/§15) | 52680 B (%40,19) | 94604 B (%10,31) |
| 2026-08-15 (sağlık raporlama düzeltmesi, §15.3) | 52680 B (%40,19) | 94684 B (%10,32) |

```
=== BUILD OK: build_hk/hk-capsule-fw.elf ===
```

0 uyarı çıkmalı. Çıktı: `build_hk/hk-capsule-fw.elf` ve `.bin`.

> ✅ **Yeniden üretilebilirlik kanıtlandı.** 2. makinede sıfırdan kurulan
> araç zinciri (gcc 16.1.0, CubeMX 6.18.1, CubeIDE 2.2.0, FW_F4 V1.28.3)
> 2026-08-01 referans ikilisini **bayt bayt aynı** üretti: karttan okunan
> flash ile yerelde derlenen `.bin`in SHA256'sı birebir eşleşti
> (`40964846...87031`). CubeIDE eklenti klasör adları da birebir aynı çıktığı
> için `build_hk.sh` hiç değiştirilmeden çalıştı.

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

> 🔴 **Yüklemeden önce 12 V'u kes ve ST-Link'in 3,3 V'unu tak.** Kart 12 V ile
> beslenirken flash **programlanamıyor** — silme geçiyor, yazma düşüyor
> (§8.3.1). Çalıştırma tam tersi: 3,3 V'u çıkar, 12 V'u tak. **İkisi aynı anda
> asla bağlanmaz**, kartın kendi 3,3 V'u çalıştığı için iki kaynak çekişir.

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

> ⚠️ **Ölçüm tuzağı:** Her CLI bağlantısı kartı **resetliyor** (RCC_CSR'da
> `SFTRSTF` set oluyor). Bu yüzden ardışık iki okuma "sayaç artmıyor" gibi
> görünür — aslında her okuma taze bir açılıştan kısa süre sonrasını gösterir.

**Gözlem penceresini büyütmenin iki yolu (2026-08-15'te bulundu):**

1. **`reset=HWrst` ekle.** Bağlantı parametrelerine eklenince reset ile okuma
   arasındaki gecikme **~0,15 sn yerine ~1,7 sn** oluyor. NRST bağlı olmasa
   bile işe yarıyor. Görevlerin oturmuş hâlini görmek için tek pratik yol:
   ```bash
   "$P/STM32_Programmer_CLI.exe" -q -c port=SWD mode=HOTPLUG reset=HWrst freq=1800 -r32 <adres> 4
   ```
2. **Tek bağlantıda çok okuma yap.** Aynı `-c` çağrısına arka arkaya onlarca
   `-r32` dizersen aralarında reset olmaz; ~60 okuma ~100 ms'lik kesintisiz
   bir pencere verir. Sayaçların gerçekten ilerleyip ilerlemediğini böyle
   doğrula (reset döngüsü ile karıştırmamak için şart).

> ⚠️ **RCC_CSR reset bayrakları güvenilir bir kanıt DEĞİL.** `hk_app_init()`
> içinde `hk_health_reset_reason()` çağrılıyor ve o fonksiyon
> `__HAL_RCC_CLEAR_RESET_FLAGS()` ile bayrakları **siliyor**. Yani "IWDGRSTF
> temiz, demek ki watchdog resetlemiyor" çıkarımı geçersizdir.

> ✅ **NRST bağlandı (2026-08-15).** ST-Link'in RST ucu kartın NRST'sine
> bağlıdır ve **bağlı kalmalıdır**. Bu kablo opsiyonel değil: yokken `mode=UR`
> çekirdeği gerçekten reset'te tutamıyor ve programlama sessizce bozuluyor
> (§8.6). Bağlandıktan sonra yukarıdaki `-w ... -v --go` komutu ilk denemede,
> sektör silme + 92 KB yazma + tam doğrulama dahil sorunsuz geçti.
>
> ⚠️ **NRST bağlı değilken yükleme "başarılı" görünüp bozuk inebiliyor.** Kabloyu
> takmadan önce görülen tablo: `-e all` çalışıyordu, kısa okumalar doğruydu,
> CubeProgrammer "File download complete" diyordu — ama flash'a yazılan veri bir
> noktadan sonra **sıfırlanıyordu**. 256 baytlık kontrollü bir testte ilk 136
> bayt doğru, kalanı `0x00` inmişti (iki ayrı okuma yöntemiyle, iki kez teyit).
> Bu yüzden **doğrulamasız (`-v`siz) yükleme yapma**; "download complete" tek
> başına yüklemenin sağlam olduğunu göstermez.

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
- **Firmware karta yüklendi ve CubeProgrammer'ın kendi tam doğrulamasından
  geçti** (`Download verified successfully`) — NRST bağlandıktan sonra, temiz
  derlemeyle, ilk denemede
- **Beklenen beş görevin hepsi canlı ve watchdog tazeleniyor** — §9.4
  düzeltmesinin donanım teyidi, ayrıntı §9.4
- **RAM log halkası SWD'den okunabiliyor** (§15.3) — firmware'in kendi çıktısı
  artık kart üzerinden okunuyor, seri port ya da SD kart gerekmeden
- **SD karta kayıt çalışıyor** — mount oluyor, `write_errors = 0`,
  `records_written` artıyor (§8.8)
- **SHT4x #1 (I2C2), SHT4x #2 (bit-bang) ve batarya ölçümü** çalışıyor (§8.8)
- **8 MHz kristal salınıyor, PLL kilitli, sistem 168 MHz'de** (RCC register'larından okundu)
- FreeRTOS zamanlayıcı çalışıyor, her iki tick sayacı ~1 kHz'de ilerliyor;
  `uxCriticalNesting = 0` → §9.1'deki kesme maskeleme hatasının düzeltmesi
  gerçek donanımda teyitli
- 10 görev oluşturuluyor (`uxCurrentNumberOfTasks = 10`, `xSchedulerRunning = 1`)
- Görev durum makinesi **BOOT → SELFTEST → ATTACHED** ilerliyor (log halkasından
  okundu, §15.3) — I2C1 arızasına rağmen degraded mod tasarlandığı gibi çalışıyor
- **IWDG açık ve 2 sn timeout ile sayıyor**, sağlık görevi düzenli tazeliyor (§9.5)
- **Buzzer FİZİKSEL OLARAK ÇALIŞIYOR** — 2026-08-15'te sesle doğrulandı (§8.5)
- **Solenoid pini düşük = kilitli** — fail-safe gerçek donanımda doğrulandı
- Fanlar kapalı (termostat 35 °C altında doğru davranıyor)
- **Kartın kendi güç zinciri (12 V → 5 V → 3,3 V) ÇALIŞIYOR** — kart yalnızca
  12 V ile açılıyor, buzzer ötüyor, SWD bağlanıyor, firmware tam işlevli koşuyor
  (§8.3). ⚠️ Ama o hâldeyken flash **programlanamıyor**, bkz. §8.3.1

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

### 8.3 ✅ ÇÖZÜLDÜ: kart 12 V ile kendi ayakları üzerinde duruyor

**Bu madde uzun süre "MCU kartın kendi rayından beslenmiyor" diye açık
duruyordu. 2026-08-15'te aksi kanıtlandı: kart 12 V ile tek başına açılıyor ve
firmware koşuyor.**

Doğru teşhisi engelleyen şey, testin **yalnızca SWD'ye dayanmasıydı**: MCU
beslenmiyor mu, yoksa besleniyor da SWD mi konuşmuyor — ayırt edilemiyordu.
Ayrım `HK_DIAG_BUZZER_ALWAYS` (§15.1) ile sağlandı: buzzer, `hk_app_init()`'in
en başında sürekli ton çalıyor, yani **kulak** SWD'den bağımsız bir kanıt
kanalı oluyor.

**Ölçüm (12 V bağlı, ST-Link 3,3 V ucu sökülü, yalnızca GND + SWCLK + SWDIO):**

| Kanıt | Sonuç |
|---|---|
| Buzzer | ✅ Kesintisiz ötüyor → MCU beslendi, saat ağacı kalktı, TIM3 sürüyor |
| Güç LED'leri | ✅ İkisi de yanıyor |
| SWD | ✅ Bağlanıyor, Device ID `0x413` |
| `xTickCount` | ✅ Düzenli ilerliyor (37.077 → 37.760) |
| Görev sayısı | ✅ 10 |
| `s_kicked` | ✅ `0x3F` ↔ `0x3D` döngüsü, watchdog tazeleniyor |
| Log halkası | ✅ Açılış kaydı tam, görev makinesi **ATTACHED**'a ilerlemiş |

Yani **12 V → 5 V → 3,3 V zinciri çalışıyor**, MCU ve SWD sağlam, firmware
kartın kendi beslemesiyle tam işlevli koşuyor.

> ⚠️ **Eski kayıtta "Sadece 12 V → Unable to get core ID" yazıyordu.** O ölçüm
> tekrar edilemedi. Muhtemel sebep temassız/gevşek bir bağlantı ya da o sırada
> flash'ın boş olması. Ders: **tek kanala dayanan bir teşhise güvenme** —
> buzzer gibi bağımsız bir kanıt kanalı olmadan "MCU beslenmiyor" sonucuna
> varılamaz.

> ⚠️ **3,17 V okuması kartı kanıtlamaz.** Klon ST-Link dongle'larında VTref
> kendi 3,3 V çıkışına bağlıdır; hedef ne olursa olsun ~3,2 V raporlar. Bu
> okumaya bakarak "kartın regülatörü çalışıyor" sonucuna varma — nitekim bu
> okuma doğruyken bile teşhis yanlıştı.

### 8.3.1 🔴 Ama: **12 V ile beslenirken flash PROGRAMLANAMIYOR**

Kart 12 V'ta sorunsuz **çalışıyor**, fakat o hâldeyken **yüklenemiyor**. Aynı
kablolar, aynı NRST, aynı komut — tek değişken besleme kaynağı:

| Besleme | Tam silme | Programlama |
|---|---|---|
| ST-Link 3,3 V | ✅ | ✅ tam doğrulamayla geçiyor (2026-08-15'te 5 kez) |
| Kart kendi 12 V | ✅ | ❌ `failed to erase memory` / `failed to download Sector[0]` |

Mass erase geçiyor ama sürekli programlama düşüyor. En olası sebep 12 V → 5 V
buck'ının gerilim dalgalanması ya da 12 V kaynağı ile USB toprağı arasındaki
referans farkı: flash program/silme çevrimi besleme kalitesine karşı hassastır,
tek uzun bir mass erase tolere eder, ardışık programlama etmez.

**Çalışma düzeni — buna göre kur:**

1. Yükleme: **12 V'u kes → ST-Link 3,3 V'u tak → yükle ve doğrula**
2. Çalıştırma: **3,3 V'u çıkar → 12 V'u tak**

> 🔴 **İki kaynağı asla aynı anda bağlama.** Kartın kendi 3,3 V'u çalıştığı için
> ST-Link'in 3,3 V'u da bağlanırsa iki kaynak çekişir.

**Elektronikçiye iletilecek mesaj:**
> Kart 12 V ile sorunsuz çalışıyor, ancak o sırada MCU'nun flash'ı
> programlanamıyor (silme geçiyor, yazma düşüyor); ST-Link'in 3,3 V'uyla
> beslenirken aynı işlem sorunsuz. Lütfen 3,3 V rayındaki dalgalanmayı
> osiloskopla ölçün (özellikle 12 V → 5 V buck anahtarlama gürültüsü) ve MCU
> besleme pinlerindeki decoupling kondansatörlerini kontrol edin.

> ℹ️ İlgili bulgu: option byte'larda **`BOR_LEV = 0x3`, yani brown-out reset
> KAPALI.** Besleme düşse bile MCU temiz bir reset atmıyor, sessizce hatalı
> davranıyor. Uçuş yazılımı için bu ayrıca gözden geçirilmeli.

### 8.4 🟡 Basınç sensörü (BMP581) kartta takılı DEĞİL

Kullanıcı beyanı: kartta basınç sensörü yok. Firmware onu bekliyor ve bulamayınca
degraded moda düşüyor (bu desteklenen bir durum, sistem çalışmaya devam eder).

**Karar gerekiyor:** Sensör sonradan takılacak mı? Kalıcı olarak yoksa görev
mantığı **yalnızca IMU ile** çalışacak şekilde ayarlanmalı — kod bunu zaten
destekliyor (`baro-only / IMU-only` degraded modlar test edilmiş durumda) ama
irtifa tabanlı kurulma (30 m) ve iniş tespiti eşikleri gözden geçirilmeli.

### 8.5 ✅ Buzzer çalışıyor (2026-08-15)

Buzzer uzun süre "çalışmıyor" sanıldı çünkü kimse duymamıştı. Sebep arıza
değilmiş: BOOT deseni `{2000 Hz, 1 tick açık / 10 tick}` yani **saniyede
yalnızca 100 ms** ötüyor, kaçırması çok kolay.

Teşhis için `HK_DIAG_BUZZER_ALWAYS` bayrağı eklendi (§15.1); sürekli tonla
buzzer **sesle doğrulandı**. Ayrıca yazılım tarafı register seviyesinde de
kanıtlandı: `TIM3_CR1.CEN=1`, `CCER.CC2E=1`, `PSC=0`, `ARR=41999`
(84 MHz ÷ 42000 = tam 2000 Hz), `CCR2=21000` (%50 duty), `CNT` fiilen dönüyor,
`GPIOB_MODER` PB5 = AF, `GPIOB_AFRL` PB5 = AF2 (TIM3_CH2).

**Sonuç:** buzzer artık "MCU beslendi ve saat ağacı kalktı" göstergesi olarak
güvenilir şekilde kullanılabilir — özellikle 12 V testinde, aletsiz.

### 8.6 ✅ ÇÖZÜLDÜ: flash boşalması ve NRST kısır döngüsü

2026-08-15'te §9.4 düzeltmesi yüklenmeye çalışılırken aşağıdaki kısır döngüye
girildi; **NRST kablosu bağlanınca tek hamlede kapandı.** Kayıt olarak duruyor,
çünkü kablo bir daha çıkarsa aynı tablo aynen geri gelir — ve belirtileri
insanı yanlış yöne (bozuk dosya, bozuk çip) sürüklüyor.

Ne olmuştu:

1. Yükleme sırasında **silme başarılı oldu, programlama düştü** → flash boş kaldı
   (94.220 baytın tamamı `0xFF` olarak okundu).
2. Boş flash'ta MCU açılışta geçersiz yığın göstergesi/PC okuyor →
   **hard fault / lockup**.
3. Kilitli çekirdek flash programlamayı engelliyor
   (`Error: ST-LINK error (DEV_TARGET_NOT_HALTED)`).
4. **NRST kablosu bağlı olmadığı için** `mode=UR` çekirdeği gerçekten reset'te
   tutamıyor → 2. adıma geri dönülüyor.

Bugün daha önce yapılan üç yükleme çalışmıştı, çünkü o sırada flash'ta geçerli
ve uslu bir firmware koşuyordu. Flash boşalınca döngü kendi kendini besledi.

NRST yokken gözlemlenen yan etkiler — **hepsi kablo takılınca kayboldu**:

- **Yazma sessizce bozuluyordu.** En kritik bulgu bu: CubeProgrammer
  "File download complete" diyordu ama flash'a inen veri bir noktadan sonra
  `0x00` oluyordu. 256 baytlık kontrollü testte ilk 136 bayt doğru, kalanı
  sıfırdı; hem toplu okuma hem 32-bit okumayla, iki kez teyit edildi. 92 KB'lık
  yüklemede de aynı imza vardı (ilk ~44 KB doğru, sonrası bozuk).
- Toplu SWD okumaları da bozuluyordu (aynı 94 KB iki okumada birbirinden
  farklı çıkıyordu). Küçük register okumaları ve Device ID sağlamdı.
- Bu yüzden 94 KB'lık bir `-v` doğrulaması o hatta **asla** geçmiyordu.
- `-e all` (tam silme) tek komut olduğu için **başarıyla** çalışıyordu →
  flash denetleyicisi ve çip sağlam.
- Sektör silme "hata" veriyordu ama aslında **çalışıyordu**: silinen bölge
  `0xFF` okunuyordu. Hata, CLI'ın silme sonrası yaptığı toplu okuma
  kontrolünden geliyordu — yani **yanlış alarm**.
- Frekansı düşürmek (1800 → 480 → 125 kHz) durumu **kötüleştirdi**; 125 kHz'de
  yazma tamamen düştü. Yani sorun sinyal hızı değil, çekirdeğin reset'te
  tutulamamasıydı.
- `-halt` ile çekirdeği durdurmak da yetmedi; parçalı yazma + her parçayı
  doğrulama denendi, 5 sektörün hiçbiri 8 denemede geçmedi.

**KURTARMA — uygulandı ve çalıştı (2026-08-15):**

1. **ST-Link'in RST ucu kartın NRST'sine bağlandı.** Tek eksik buydu.
2. Sonrasında normal yükleme yetti:
   ```bash
   "$P/STM32_Programmer_CLI.exe" -c port=SWD mode=UR freq=1800 \
       -w build_hk/hk-capsule-fw.elf -v --go
   ```
   Sektör silme geçti, 92 KB yazıldı, **tam doğrulama geçti**, firmware koştu.
   NRST bağlıyken `mode=UR` çekirdeği fiziksel olarak reset'te tutuyor, kilitli
   CPU devre dışı kalıyor, silme/yazma/doğrulama güvenilir çalışıyor.

> ⚠️ **`-hardRst` + `PINRSTF` testine güvenme.** Kablonun bağlı olup olmadığını
> anlamak için RCC_CSR reset bayrakları temizlenip `-hardRst` uygulandı ve
> PINRSTF **set olmadı** — kablo bağlıyken bile. Yani bu test yanlış negatif
> veriyor. Kablonun iş görüp görmediğinin tek güvenilir ölçütü **yüklemenin
> kendisi**: `mode=UR` ile sektör silme geçiyorsa NRST çalışıyordur.

**RST pini yoksa alternatif:** `mode=POWERDOWN` ile CubeProgrammer besleme
kesilmesini bekler; komut başlatılır, 3,3 V ucu çıkarılıp takılır, program
açılış anını yakalayıp CPU hard fault'a girmeden flash'ı ele geçirir.

> ✅ **Kalıcı hasar yok.** RDP Level 0, tüm WRP sektörleri açık — çip SWD'den
> her koşulda kurtarılabilir. Kaybolan ayar da yok: sektör 11 bu iş başlamadan
> önce zaten boştu (`0xFF`).

### 8.7 🟡 Basınç sensörü kararı hâlâ bekliyor

§8.4'teki karar verilmedi. Not: BMP581 zaten I2C1'de ve o hat da arızalı
(§8.2), yani ikisi tek bir incelemede birlikte ele alınmalı.

### 8.8 ✅ ÇÖZÜLDÜ: SD kayıt çalışıyor (SPI hızı) — ve doğrulanan sensörler

Kart 12 V ile kendi beslemesinde çalışırken SD kart takıldı (2026-08-15).
`s_state.sensor_ok` (offset 124) okundu: **`0x26`**.

| Bit | Cihaz | Durum |
|---|---|---|
| 0 | BMP581 | ❌ takılı değil + I2C1 arızalı (§8.2, §8.4) |
| 1 | **SHT4x #1** (I2C2) | ✅ **ilk kez doğrulandı** |
| 2 | **SHT4x #2** (bit-bang I2C) | ✅ **ilk kez doğrulandı** |
| 3 | BMI270 | ❌ I2C1 arızalı (§8.2) |
| 4 | GPS | ❌ fix yok — kapalı alanda beklenen |
| 5 | **Batarya ölçümü** (ADC1_IN10) | ✅ **ilk kez doğrulandı** |
| 6 | SD kart | ❌ **mount olmadı** |

Yani her iki sıcaklık/nem sensörü, ayrı hatlar üzerinden, ve batarya ölçümü
çalışıyor. §8.2'nin etkisi yalnızca I2C1'deki iki cihazla sınırlı.

**SD neden sessizdi:** `task_storage` mount durumunu yalnızca **değiştiğinde**
loglıyordu ve `mounted_shown` `false` başlıyordu. Kart hiç mount olmayınca
durum hiç değişmiyor, dolayısıyla **tek satır bile yazılmıyordu** — yani log,
tam da bir şey ters gittiğinde susuyordu. Düzeltildi: ilk gözlem de raporlanıyor
ve `f_mount`'un `FRESULT` kodu `hk_storage_t.last_mount_err` alanında saklanıp
log satırına `mount_err=<n>` olarak basılıyor.

**`mount_err` kodunun okunuşu** (FatFs `FRESULT`):

| Kod | Anlam | Ne yapmalı |
|---|---|---|
| `1` | `FR_DISK_ERR` | SPI/kart haberleşmesi — kablolama, SPI hızı, 5 V rayı |
| `3` | `FR_NOT_READY` | Kart hiç ilklenmedi — besleme, yuva teması, kart arızası |
| `13` | `FR_NO_FILESYSTEM` | **Biçim yanlış** — kartı FAT32 yap |

> ⚠️ **exFAT desteklenmiyor:** `App/third_party/fatfs/ffconf.h` içinde
> `FF_FS_EXFAT = 0`. Windows 32 GB üstü kartları varsayılan olarak exFAT
> biçimlendirir; böyle bir kart `FR_NO_FILESYSTEM` verir. Kart **FAT32**
> (ya da FAT16) olmalı, sektör boyu 512 bayt (`FF_MIN_SS = FF_MAX_SS = 512`).

**Ölçüm sonucu — biçim sorunu DEĞİL, SPI hızı.** Yeni teşhisle karttan okunan:

```
[W/storage] SD unavailable (degraded), mount_err=0 err=1 drop=0
[W/storage] SD unavailable (degraded), mount_err=1 err=1 drop=47
```

Okunuşu:

- İlk deneme `mount_err=0` (**`FR_OK`**) → **mount başarılı**. Yani kart ve
  üzerindeki FAT dosya sistemi sağlam; biçim hipotezi elendi.
- Ama hemen ardından `err=1`: mount sonrası `/LOGS` oluşturma adımı düştü,
  `fs_failed()` unmount etti.
- İkinci deneme `mount_err=1` (**`FR_DISK_ERR`**) → düşük seviye SPI hatası.
- `drop=47`: yazılamadığı için kuyruk dolup kayıt atmaya başladı.

Yani kart **okunabiliyor ama yazma/sürekli haberleşme kararsız**. Bu, jumper
kablolu 5 V'luk Arduino tipi SD modüllerinde SPI saatinin yüksek olmasının
klasik belirtisidir.

**Düzeltme:** `HK_SD_DATA_HZ` **10,5 MHz → 2,625 MHz** (84 MHz / 8 yerine / 32).
Modülün seviye çeviricisi ve kablolama için bol marj bırakır. Tekrar yükseltmek
istenirse **hattı logic analyzer ile izleyerek** yapılmalı.

> ✅ **ÇÖZÜLDÜ — SD kayıt çalışıyor (2026-08-15).** Hız düşürüldükten sonra
> karttan okunan:
>
> ```
> [I/storage] SD mounted, session FL_0002
> sensor_ok      = 0x66     -> bit6 (SD) yandı: SHT1 + SHT2 + BATT + SD
> write_errors   = 0        -> tek bir yazma hatası yok
> records_written= 1169 -> 1191, artıyor
> dropped        = 896      -> yalnızca mount öncesi açılış penceresinde
> ```
>
> `dropped` sayısı bir arıza değil: kart mount olana kadar kuyruk doluyor ve
> eski kayıtlar atılıyor. Mount sonrası kayıp yok.
>
> Not: ilk turda `mount_err=3` (`FR_NOT_READY`) görülüp sonraki denemede mount
> olması normaldir — kart ilklenmesi depolama görevinin ilk turundan uzun sürer.

**Uçtan uca doğrulandı (2026-08-15).** Kart bilgisayara takılıp
`tools/hk_log_reader.py` ile çözüldü:

```
D:\LOGS\FL_0001.BIN        0 bayt   (SPI hızı düşmeden önceki başarısız oturum)
D:\LOGS\FL_0002.BIN   341.060 bayt
D:\LOGS\FL_0002.CSV    34.859 bayt  (firmware'in kendi yazdığı CSV)

7874 kayıt; stats: {'good': 7874, 'bad_crc': 0, 'resyncs': 0, 'tail_bytes': 0}
```

**Tek bozuk bayt yok** — CRC hatası, resync, artık bayt sıfır. SPI hızı
düzeltmesinin sağlamlığının en güçlü kanıtı bu.

| Kayıt türü | Adet | Durum |
|---|---|---|
| ENV | 563 | ✅ gerçek veri, 2 Hz, 313 saniye boyunca |
| IMU | 7028 | ⚠️ hepsi sıfır — BMI270 ölü (§8.2) |
| GPS | 281 | ⚠️ hepsi `valid=0, sats=0` — kapalı alanda fix yok |
| META / EVENT | 1 / 1 | ✅ |

Örnek ENV satırları (sıcaklık, ortam soğudukça fiziksel olarak makul bir eğim
izliyor; iki SHT4x boyunca ~0,2 °C içinde birbirini takip ediyor):

```
ENV,212,    SELFTEST, t1=33.67, rh1=24.1, t2=33.41, rh2=24.2, vbat=12.19, soc=0.85
ENV,171712, ATTACHED, t1=32.41, rh1=22.4, t2=32.24, rh2=22.5, vbat=12.19, soc=0.85
ENV,313212, ATTACHED, t1=31.96, rh1=22.4, t2=31.74, rh2=22.5, vbat=12.21, soc=0.86
```

Kartın kendi yazdığı `.CSV` (565 satır) çözülen `.BIN` ile birebir aynı.

> 🟡 **Küçük boşluk:** `SELFTEST -> ATTACHED` geçişi RAM logunda görünüyor ama
> `.BIN` içinde EVENT kaydı olarak **yok**. Sebep: SD ancak `t=35301`'de mount
> oluyor, o ana kadar kayıt kuyruğu doluyor ve taşan kayıtlar atılıyor
> (`dropped`). Yani **mount öncesi pencerede oluşan olaylar kaybolabilir.**
> Tasarım gereği (kuyruk dolarsa at) ama uçuşta kritik bir olay o pencereye
> denk gelirse kaydı olmaz — gözden geçirilmeli.

### 8.9 🔴 SD kart takılı ama beslemesizken firmware açılışta ASILI KALIYOR

2026-08-15'te bulundu. **Kart 12 V ile çalışırken sorun yok**; takılma yalnızca
"SD kart yuvada, ama 5 V rayı yok" durumunda oluyor — yani tam olarak
**ST-Link'in 3,3 V'uyla beslenirken**.

Belirtiler:

| Ölçüm | Değer | Anlamı |
|---|---|---|
| PC | `0x08001A28` → `HAL_Delay`, `stm32f4xx_hal.c:401` | bekleme döngüsünde |
| `uwTick` | `769`, 20 okumada **sabit** | HAL zaman sayacı donmuş → kesmeler maskeli |
| `xTickCount` | `0` | `osKernelStart()`'a hiç ulaşılmamış |
| log `wr` | `0` (magic yazılı) | `hk_log_ram_init()` koştu, tek satır bile loglanmadı |

Yani sistem ~769 ms koşup `hk_app_init()` içinde donuyor. Takılma noktası,
CONFIG.INI okumak için **zamanlayıcıdan önce** yapılan `f_mount()` bloğu
(`App/tasks/app.c`); ilk log satırı `[I/config] loaded` bu bloktan sonra
basıldığı ve `wr = 0` olduğu için oraya hiç varılamadığı kesin.

Donmuş tick + `HAL_Delay` bileşimi **§9.1'in mekanizmasının aynısı**: kesmeler
maskeliyken HAL zaman aşımları sonsuza dönüyor.

**Kontrol deneyi (tetikleyici kesinleşti).** Aynı besleme (ST-Link 3,3 V), aynı
firmware, tek fark kartın yuvadan çıkarılması:

| SD kart | Sonuç |
|---|---|
| Takılı (beslemesiz) | ❌ Açılışta asılı, `xTickCount = 0`, log `wr = 0` |
| **Çıkarılmış** | ✅ Sorunsuz açılıyor, tick ilerliyor, `sensor_ok = 0x26`, görev makinesi ATTACHED |

Kartsız açılışta log da beklendiği gibi: `mount_err=3` (**`FR_NOT_READY`**),
yani "kart yok" doğru raporlanıyor. Demek ki takılmaya sebep olan şey **kartın
yuvada olup beslenmemesi** — muhtemelen beslemesiz kartın SPI hattını tuhaf bir
duruma sokması ve SD init yolunun oradan çıkamaması.

> 🔴 **Bu, kodun kendi vaadini çürütüyor.** `hk_app_init()` içindeki yorum
> *"a missing/late-inserted card only means degraded logging, never a fault"*
> diyor. Bu senaryoda vaat tutmuyor: kart hiç açılmıyor. Bir uçuş kapsülü için
> fail-safe ihlalidir ve **uçuş öncesi çözülmelidir.** Kök neden (kesmeleri ne
> maskeliyor) henüz bulunmadı; olası yön, zamanlayıcı öncesi `f_mount()`
> çağrısını tamamen kaldırıp CONFIG.INI okumasını depolama görevine taşımak.

**Geçici çalışma kuralı:** ST-Link beslemesiyle yükleme yaparken **SD kartı
çıkar.** Yükleme etkilenmiyor (flash yazma sorunsuz), ama kart takılıyken
firmware açılışta asılı kalır ve teşhis okumaları yanıltıcı olur.

### 8.10 Sıradaki adımlar (öncelik sırasıyla)

Tamamlananlar: ~~NRST'yi bağla ve kartı kurtar~~ ✅ (§8.6) ·
~~§9.4 düzeltmesini donanımda doğrula~~ ✅ (§9.4) ·
~~IWDG yapılandırma tutarsızlığı~~ ✅ (§9.5 — hata yokmuş) ·
~~log halkasının uyarı spam'iyle dolması~~ ✅ (§15.3) ·
~~12 V → 5 V → 3,3 V zincirini doğrula~~ ✅ (§8.3 — zincir çalışıyor) ·
~~SD kayıt testi~~ ✅ (§8.8 — SPI hızı düşürülerek çözüldü)

1. **GPS'i açık havada test et** — kayıt yolu çalışıyor (281 GPS kaydı yazıldı)
   ama kapalı alanda fix olmadığı için hepsi `valid=0`. MAX-M10S'in gökyüzü
   görmesi gerekiyor; fix gelince aynı kayıtlar gerçek koordinatla dolacak.
2. **SD takılıyken açılış takılmasını çöz** (§8.9) — fail-safe ihlali,
   uçuş öncesi kapatılmalı
3. Elektronikçi I2C1 SDA hattını incelesin (§8.2). Firmware'in kendi logu bunu
   artık açıkça söylüyor: `i2c_main silent`, `bmp=IO imu=NOT_FOUND` (§15.3).
4. 3,3 V rayındaki dalgalanmayı ölçtür (§8.3.1) — kart çalışıyor ama o hâldeyken
   programlanamıyor; şimdilik ST-Link beslemesiyle yükleyerek çalışılabiliyor.
5. Basınç sensörü kararı (§8.4 / §8.7) — BMP581 de I2C1'de olduğu için 3. madde
   ile tek incelemede ele alınmalı
6. `BOR_LEV = 0x3` (brown-out reset kapalı) — uçuş öncesi gözden geçirilmeli;
   besleme düştüğünde MCU temiz reset atmıyor
7. Sonra sırayla: GPS (açık havada) → aktüatörler → IWDG testi → gece soak testi
8. Tam plan: [`docs/bringup.md`](bringup.md)

---

## 9. Devreye almada bulunan ve düzeltilen firmware hataları

Bunlar **gerçek donanım olmadan yakalanamayacak** hatalardı; kayıt olarak
duruyor ki benzerleri tekrar edilmesin. Dördü de host testlerinden geçen,
"doğru" görünen koddan çıktı.

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

### 9.4 Ölü bir I2C hattı düşük öncelikli GÖREVLERİ tamamen aç bırakıyordu

**Tespit:** 2026-08-15, çalışan kartın belleğinden okundu · **Bu, dokümanların
vaat ettiği "sensör yoksa degraded mod, asla fault değil" davranışını çürüten
en ciddi bulgudur.**

**Ölçüm** (`s_kicked`/`s_expected` adresleri `nm` ile bulunup SWD'den okundu):

```
s_kicked   = 0x11   -> yalnizca IMU + MISSION calisiyor
s_expected = 0x1F   -> IMU | ENV | GPS | CONTROL | MISSION bekleniyor
missing    = 0x0E   -> ENV, GPS, CONTROL 1,73 saniyedir hic calismamis
uptime_ms  = 0      -> task_health de hic tamamlanmamis
```

**Mekanizma:** `task_imu` önceliği 3, periyodu 10 ms. I2C1 SDA arızası yüzünden
BMI270 cevap vermiyor ve HAL bloke edici zaman aşımına giriyor (bus'a 50 ms
verilmiş). Döngü gövdesi periyodundan uzun sürünce `vTaskDelayUntil()` hiç
uyutmuyor — görev CPU'yu bırakmıyor. `task_mission` aynı öncelikte olduğu için
round-robin ile pay alıyor; **öncelik ≤ 2 olan her şey (ENV, GPS, CONTROL,
STORAGE, HEALTH) aç kalıyor.**

**İkinci dereceden sonuç:** `task_health` çalışmadığı için `hk_health_service()`
hiç koşmuyor, dolayısıyla `HAL_IWDG_Refresh()` hiç çağrılmıyor. Watchdog
mantığı "görevler ölürse resetle" diye tasarlanmıştı; burada tek bir ölü I2C
cihazı bütün sistemi o duruma sokuyor.

**Çözüm:** `sensor_manager`'a cevap vermeyen cihaz için geri çekilme (backoff)
eklendi — §15.2. Cevap vermeyen cihaz `HK_SENSOR_RETRY_MS` (2 sn) boyunca
bus'a hiç dokunulmadan atlanıyor, böylece görev periyodu korunuyor.

**Durum:** ✅ **Donanımda doğrulandı (2026-08-15).** 0 uyarı, 13/13 host testi
geçiyor, kart üzerinde de teyitli.

**Donanım ölçümü:** kart kesintisiz ~137 sn koştu (`xTickCount` düzgün
ilerliyor), `xSchedulerRunning = 1`, `uxCurrentNumberOfTasks = 10`,
`uxCriticalNesting = 0`. `s_kicked` arka arkaya örneklendiğinde şu döngü
görüldü:

```
0x3D  0x3D  0x15  0x3D  0x3D  0x01  0x3D  0x3D  0x3F  0x3D  0x3D  0x3D
```

`0x3F` = beklenen beş görevin **artı** STORAGE'ın tik attığı an; hemen ardından
`0x01`'e düşmesi akümülatörün sıfırlandığını, yani `missing = 0` olup
**IWDG'nin tazelendiğini** kanıtlıyor. Düzeltmeden önce `s_kicked` `0x11`'de
takılıydı, ENV/GPS/CONTROL hiç çalışmıyordu ve `uptime_ms` 0'da duruyordu.

> ⚠️ **Ölçüt düzeltmesi.** Bu dokümanın önceki hâli "doğrulama ölçütü
> `s_kicked` = `0x1F`" diyordu; bu yanıltıcıdır. STORAGE (bit5) de tik attığı
> için tam dolu değer **`0x3F`**'tir, üstelik `hk_health_service()` bunu görür
> görmez akümülatörü sıfırladığı için anlık okuma neredeyse hep `0x3D` yakalar.
> `0x3D`, ENV'nin iki tiki arasındaki **normal** bekleme hâlidir: ENV 500 ms
> (`HK_ENV_PERIOD_MS`), sağlık görevi 100 ms (`HK_HEALTH_PERIOD_MS`) periyotla
> çalışır, yani sağlık görevi ENV'yi çoğu turda eksik görür. **Doğru ölçüt sabit
> bir değer değil, akümülatörün periyodik olarak sıfırlanmasıdır.** Sıfırlanmayıp
> bir değerde kilitleniyorsa gerçekten bir görev ölmüştür; hangisi olduğunu
> `missing = s_expected & ~s_kicked` verir.

**Kural:** Bloke edici bir bus çağrısını, görev periyodundan uzun sürebilecek
şekilde periyodik bir göreve koyma. Cihaz cevap vermiyorsa hızlı vazgeç ve
geri çekil.

### 9.5 IWDG yapılandırması — **ÇÖZÜLDÜ: hata yoktu, ölçüm artefaktıydı**

Bu madde uzun süre "watchdog ne yapılandırılmış ne de çalışıyor" diye açık
duruyordu. **Firmware düzgün yüklenip koştuktan sonra tekrar ölçüldü ve
yapılandırmanın doğru olduğu görüldü.**

Karttan okunan (2026-08-15, firmware koşarken):

| Register | Değer | Anlamı |
|---|---|---|
| `IWDG_PR` | `0x04` | prescaler **/64** — `IWDG_PRESCALER_64` ile birebir |
| `IWDG_RLR` | `0x3E8` (1000) | `hiwdg.Init.Reload = 1000` ile birebir |
| `IWDG_SR` | `0` | yazma tamamlanmış, değerler geçerli |
| `RCC_CSR` | `0x03` | `LSION` + `LSIRDY` → LSI çalışıyor |

32 kHz ÷ 64 = 500 Hz, 1000 sayım → **2 sn timeout**, yani `Core/Src/iwdg.c`
ne diyorsa o. `PR`/`RLR` ancak `IWDG_KR`'ye `0x5555` yazıldıktan sonra
yazılabildiği için bu değerlerin oturmuş olması `HAL_IWDG_Init()`'in gerçekten
koştuğunu da kanıtlıyor — dolayısıyla watchdog **açık ve sayıyor**.

**Neden yanlış göründü:** önceki ölçüm, flash'ın boş/bozuk olduğu dönemde
(§8.6) alınmıştı. `MX_IWDG_Init()` hiç çalışmadığı için register'lar doğal
olarak reset sonrası varsayılanlarını (`PR = 0`, `RLR = 4095`) gösteriyordu.
Yani çelişki firmware'de değil, ölçümün alındığı andaydı.

> **Ders:** Kart üzerindeki register ölçümlerini yorumlamadan önce, o an
> flash'ta **geçerli ve doğrulanmış** bir firmware olduğundan emin ol. Bozuk
> flash'tan okunan çevre birimi register'ları firmware hakkında hiçbir şey
> söylemez.

> Not: `RCC_CSR` reset bayraklarına bakarak "IWDG resetlemiyor" sonucuna varma;
> `hk_health_reset_reason()` o bayrakları her açılışta siliyor (§6).

---

## 10. Bilinen tuzaklar (zaman kaybettirenler)

| Tuzak | Belirti | Çözüm |
|---|---|---|
| **Smart App Control** | `ctest` çalışmıyor, `WinError 4551` | SAC'ı kapat ya da testleri CI'da koştur |
| **CubeMX ADC/TIM'i siliyor** | `MX_ADC1_Init` üretilmiyor | §5.4 — bsp dosyası zaten çözüyor |
| **bash `printf` + `C:\Users`** | `missing unicode digit for \U` | Heredoc + ileri bölü kullan |
| **Her SWD bağlantısı resetliyor** | Sayaçlar artmıyor gibi görünür, `xTickCount = 0` okunur | Tek bağlantıda çok okuma yap (§6); açılış kaydı için RAM log halkası (§15.3) |
| **`python` komutu** | Store kısayoluna gidiyor | Tam yolu ya da `py` kullan |
| **CI'da `Service Unavailable`** | "Set up job" aşamasında kırmızı | GitHub altyapısı, kodla ilgisi yok — rerun |
| **`--coreReg` çekirdeği durdurur** | Sonraki okumalar donmuş görünür | Ölçüm sırasını buna göre kur |
| **NRST bağlı değil** | "download complete" der ama flash'a yer yer `0x00` iner; sektör silme düşer | Kabloyu tak (§8.6). Asla `-v`siz yükleme yapma |
| **Sektör silme "hata" verir** | `Sector erase operation failed` | Yanlış alarm — silinen bölgeyi oku, `0xFF` ise silme olmuştur (§8.6) |
| **`-hardRst` / `PINRSTF` testi** | NRST bağlıyken bile PINRSTF set olmaz | Bu testle karar verme; ölçüt `mode=UR` ile yüklemenin geçmesidir (§8.6) |
| **Yavaşlatmak işe yaramaz** | 480/125 kHz'de durum kötüleşir | Sorun sinyal hızı değilse frekans düşürmek yardımcı olmaz (§8.6) |
| **ST-Link'i başka program tutuyor** | `ST-LINK error (DEV_CONNECT_ERR)`, kablo/kart sağlamken | ST-Link'e aynı anda tek program erişir. Açık CubeProgrammer / ST-LINK Utility / CubeIDE debug oturumunu kapat |
| **Bozuk flash'tan register okumak** | Çevre birimi register'ları "yapılandırılmamış" görünür | Önce geçerli firmware yükle; boş flash'ta hiçbir init kodu koşmamıştır (§9.5) |
| **12 V ile beslenirken yükleme** | Silme geçer, yazma `Sector[0]`'da düşer | Yüklemeyi ST-Link 3,3 V'uyla yap, 12 V'u kes (§8.3.1) |
| **Tek kanala dayanan teşhis** | "MCU beslenmiyor" gibi yanlış sonuç | SWD'den bağımsız bir kanıt kanalı kur — buzzer bunun için var (§15.1, §8.3) |

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
6. **Nerede kaldığımızı öğrenmek için §8'i oku** — firmware kartta koşuyor,
   sıradaki iş 3,3 V rayını ölçmek (§8.3)
7. Devreye alma sırasında eklenen teşhis araçları: §15

---

## 15. Devreye alma için eklenen araçlar (2026-08-15)

Üçü de repoda, versiyonlu ve host testlerinden geçiyor. İkisi teşhis amaçlı,
biri (§15.2) kalıcı bir davranış düzeltmesi.

### 15.1 `HK_DIAG_BUZZER_ALWAYS` — aletsiz "MCU ayakta" göstergesi

`App/tasks/app_config.h` içinde, **varsayılanı `0`** ve uçuş derlemelerinde
`0` kalmalı.

`1` yapıldığında buzzer, `hk_app_init()`'in **en başında** — herhangi bir bus'a
ya da sensöre dokunulmadan önce — sürekli ton çalmaya başlar ve görev buzzer
desenleri bastırılır. Böylece init'in ilerisinde bir takılma olsa bile ses
devam eder; buzzer "**MCU beslendi ve saat ağacı kalktı**" göstergesine dönüşür.

Ne işe yaradı: §8.3'teki 12 V testinde, elde ölçü aleti olmadan kartın kendi
rayından açılıp açılmadığına karar vermek için. §8.5'te buzzer bu bayrakla
sesle doğrulandı — arıza sanılan şeyin aslında BOOT deseninin kısalığı olduğu
böyle anlaşıldı.

Ton frekansı `HK_DIAG_BUZZER_FREQ_HZ` (2000 Hz).

### 15.2 Sensör geri çekilmesi (backoff) — §9.4'ün çözümü

`App/services/sensor_manager/` içinde. Ayar: `HK_SENSOR_RETRY_MS = 2000`.

Cevap vermeyen bir cihaz **2 saniye boyunca bus'a hiç dokunulmadan atlanır**.
Her cihazın kendi zaman damgası var (`bmp_retry_at_ms`, `imu_retry_at_ms`,
`sht1_retry_at_ms`, `sht2_retry_at_ms`); `hk_sensors_init()` sırasında cevap
vermeyen parça doğrudan backoff'a alınır, ilk örnekleme turunu boşuna
harcamaz.

**Neden gerekliydi:** backoff yokken, olmayan bir cihazın her örneklenmesi bir
bloke edici HAL bus zaman aşımına (50 ms) mal oluyordu. IMU görevinin periyodu
10 ms olduğu için döngü gövdesi periyottan uzun sürüyor, `vTaskDelayUntil()`
hiç uyutmuyor ve öncelik-3 görev CPU'yu bırakmıyordu; altındaki her şey aç
kalıyordu. Ayrıntılı mekanizma ve ölçüm §9.4'te.

İnce nokta: bir turda **yalnızca gerçekten yoklanan** cihazlar raporlanır;
atlanan cihaz önceki `sensor_ok` durumunu korur, backoff yüzünden "yeni bozuldu"
gibi görünmez.

### 15.3 RAM log halkası — firmware'in çıktısını SWD'den okumak

`App/bsp/log_ram.h` + `log_ram_stm32.c`. Global: `g_hk_log_ram`.

**Neden var:** kartta debug UART yok, telemetri radyosu yok (bilinçli tasarım,
§1) ve SD kart 5 V rayına bağlı olduğu için §8.3 çözülene kadar kullanılamıyor.
Güç zinciri onarılana kadar firmware'in ne dediğini duymanın **tek yolu** bu.

Yerleşim — sabit ofsetler, okuyucunun varsayım yapmasına gerek yok:

| Ofset | Alan | Değer |
|---|---|---|
| +0 | `magic` | `0x484B4C47` (`'HKLG'`) — kurulduysa |
| +4 | `size` | `2048` (ikinin kuvveti, sarma maske ile) |
| +8 | `wr` | yazılan **toplam** bayt; monoton, hiç sıfırlanmaz |
| +12 | `buf[2048]` | halka; `wr >= size` ise en eski bayt `buf[wr % size]` |

`hk_app_init()` içinde, zamanlayıcıdan **önce** kurulur (`hk_log_ram_init()`
ardından `hk_log_init(hk_log_ram_sink, HK_LOG_INFO)`). Sink bilinçli olarak
**hiç FreeRTOS çağrısı içermez**; kritik bölge yerine PRIMASK kaydet/geri yükle
kullanır. Sebebi §9.1: zamanlayıcı başlamadan alınan bir FreeRTOS kritik bölgesi
kesmeleri kalıcı olarak maskeliyor. PRIMASK yöntemi hem zamanlayıcı öncesi hem
sonrası doğrudur ve FreeRTOS kritik bölgesinin içine güvenle yuvalanır.

**Okuma** (adres her derlemede değişir, `nm` ile tazele):

```bash
T="/c/ST/.../gnu-tools-for-stm32.../tools/bin"
"$T/arm-none-eabi-nm.exe" build_hk/hk-capsule-fw.elf | grep g_hk_log_ram
# bu derlemede: 200091cc B g_hk_log_ram

# baslik: magic | size | wr
"$P/STM32_Programmer_CLI.exe" -q -c port=SWD mode=HOTPLUG reset=HWrst freq=1800 \
    -r32 0x200091CC 12

# icerigi ASCII'ye cevir (buf = adres + 12). Denenmis, calisiyor:
"$P/STM32_Programmer_CLI.exe" -q -c port=SWD mode=HOTPLUG reset=HWrst freq=1800 \
    -r32 0x200091D8 1024 \
  | grep '^0x2' | sed 's/^[^:]*: //' | tr ' ' '\n' | grep -E '^[0-9A-F]{8}$' \
  | while read -r w; do printf '%b' "\\x${w:6:2}\\x${w:4:2}\\x${w:2:2}\\x${w:0:2}"; done \
  | tr -d '\000'
```

`-r32` çıktısı 32-bit **little-endian** kelimelerdir; yukarıdaki `while`
döngüsü baytları ters çevirip ASCII'ye döker. Halka ortasından sardığı için
ilk satır yarım çıkabilir, normaldir.

**Gerçek açılış kaydı** (2026-08-15, karttan okundu) — §8.2'nin firmware
gözünden görünüşü:

```
[I/config]  loaded (nv seq=0)
[W/sensors] i2c_main silent; clearing bus and retrying
[I/sensors] bmp=IO imu=NOT_FOUND
[I/app]     init complete, last reset = SOFT
[I/mission] BOOT -> SELFTEST (arg=0)
[I/mission] SELFTEST -> ATTACHED (arg=1)
```

İlk üç satır §8.2'yi firmware gözünden anlatıyor: I2C1 sessiz, yazılımsal
bus-clear denendi, BMP581 `IO` (hat arızalı) ve BMI270 `NOT_FOUND`. Son iki
satır ise görev durum makinesinin arızaya rağmen ilerlediğini gösteriyor —
degraded mod tasarlandığı gibi çalışıyor.

> ✅ **Çözülen sorun: halka tek bir satırla doluyordu.** İlk hâlinde sağlık
> görevi her turda koşulsuz uyarı basıyordu. Sağlık görevi 100 ms'de bir
> çalışıp ENV 500 ms'de bir tik attığı için (§9.4) `hk_health_service()` her 5
> turun 4'ünde `missing = 0x02` görüyor, yani **saniyede ~8 kez**
> `[W/health] tasks not alive, mask=0x02` yazılıyordu. 2 KB'lık halka saniyeler
> içinde yalnızca bu satırla dolup açılış kaydını taşıyordu.
>
> **Düzeltme:** `task_health` artık bir maskeyi ancak
> `HK_HEALTH_WARN_PASSES` (10 tur = 1 sn, en yavaş görev periyodunun iki
> katından uzun) boyunca **kesintisiz sürerse** bir kez raporluyor, düzelmeyi de
> bir kez yazıyor. Yavaş bir görevin normal gecikmesi artık log üretmiyor, ama
> gerçekten ölen bir görev hâlâ görünüyor. **IWDG politikası değişmedi** —
> `hk_health_service()` her tazelemede beklenen görevlerin hepsini aramaya
> devam ediyor.
>
> Düzeltme sonrası karttan okunan: `wr = 236` bayt (önce 5518 ve sürekli
> sarıyordu), sıfır uyarı satırı ve daha önce spam altında hiç görünmeyen
> `[I/mission] SELFTEST -> ATTACHED (arg=1)` satırı ortaya çıktı.
