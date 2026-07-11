# Donanım Bring-up Planı — PCB Geldiğinde

> Sıra önemlidir: her adım bir öncekinin üstüne kurulur. Bir adım başarısızsa
> ilerleme; önce sebebi bul.
>
> **Kart planı (EE cevapları, 2026-07-11):** önce **Şükrü'nün kartı** (SWD
> doğru, varsayılan firmware varyantı). Çalışmazsa ilk kart (`HK_BOARD_REV2A`
> sembolüyle derle + §3 UART bootloader yolu — o kartta SWD yanlış bağlı).
> Solenoid polaritesi TEYİTLİ: enerjisiz = kilitli; §9'daki test yine yapılır.

## 0. Şimdiden sipariş edilecekler (kargo süresi!)

| Kalem | Not |
|---|---|
| **ST-Link V2** (+1 yedek) | Klon olur; J4 konektörüne. SWD sorunu çıkarsa da lazım |
| **USB-UART dönüştürücü (3.3V)** | CP2102/FT232RL; SWD bozuksa tek kurtarma yolu |
| 2× **microSD kart** (8–32 GB, A1/C10, markalı) + USB okuyucu | FAT32 formatlı |
| **Catalex tipi 5V microSD modülü** | J3'e; **ayrıca 1 adet 3.3V (level-shiftersız) modül** yedek al — 5V modüllerin MISO'su sorun çıkarabiliyor |
| **Akım limitli masa güç kaynağı** (0–15 V, ≥3 A) | İlk güç YALNIZ bununla verilir |
| **3S Li-ion paket + şarj cihazı** + XT60/JST kablo | Saha için |
| Multimetre, ince uçlu havya, flux, **30 AWG tel** | Bodge ihtimali (S1) |
| Ucuz **8 kanallı logic analyzer** (24 MHz) | I2C/SPI decode — SD ve I2C ayıklamada paha biçilmez |
| Jumper kablolar + 2.54 mm header | BOOT0 ve test noktaları |

## 1. İlk güç verme (batarya DEĞİL — PSU ile)

1. Büyüteçle görsel muayene: LQFP100 lehim köprüleri, ters kondansatör, eksik parça.
2. GPS, SD modülü ve servo **takılı değilken**: PSU 12 V, **akım limiti 100 mA** → kart beslemesine bağla.
3. Sırayla ölç: 5V rayı (XL4015 çıkışı), 3.3V (AMS1117), SERVO_5V (ikinci XL4015). LED'ler (D6, 3V_LED1) yanmalı.
4. Akım tüketimini not et (beklenen boşta: ~50–150 mA). Isınan parça var mı elle kontrol (dikkatli).
5. Sorun yoksa limiti 500 mA'ya çıkar.

## 2. Programlayıcı bağlantısı

1. ST-Link'i J4'e bağla (3.3V, SWDIO, SWCLK, NRST, GND).
2. STM32CubeProgrammer → Connect. **Şükrü kartında bağlanması beklenir** (SWD PA13/PA14'te, S1/S6 teyitli) → §4'e geç.
3. Bağlanmıyorsa: kablo/pinout kontrolü; ısrarla olmuyorsa §3.
4. **İlk karta (REV2A) düşülürse:** SWD orada yanlış bağlı — doğrudan §3 ile yükleme yapılır.

## 3. SWD çalışmıyorsa / REV2A kartı: kurtarma yolları (sırayla dene)

**A. Sistem bootloader'ı (UART):**
1. BOOT0'ı 3.3V'a çek (R5'in MCU tarafı padinden jumper; S5 cevabına göre kolay erişim olabilir).
2. USB-UART'ı **PB10 (TX) / PB11 (RX)** padlerine bağla (SHT4x_1 hattı; USART3). GND ortak.
   - Neden USART1 değil: PA9/PA10'da GPS modülü var; GPS'in TX'i hatta bağlıyken bootloader konuşamaz. GPS henüz lehimlenmemişse PA9/PA10 da olur.
3. Reset at; CubeProgrammer → UART, **115200, Even parity** → Connect.
4. Firmware .bin'i 0x08000000'e yaz. Çalışıyor — ama her yüklemede bu tören gerekir; kalıcı çözüm için D'ye bak.
5. Ayar değişiklikleri için reflash gerekmez: SD karta **CONFIG.INI** koy (aşağıda §8).

**B. USB DFU:** PA11/PA12 padlerine el yapımı USB koparması (data hatları) — F405 DFU destekler; pad erişimi varsa B, A'dan pratik olabilir.

**C. Bodge tel:** LQFP100 **pin 72 (PA13) → J4 SWDIO**, **pin 76 (PA14) → J4 SWCLK**; 30 AWG, 0.5 mm pitch — deneyimli el ister.

**D. Kalıcı:** Elektronik ekibi revizyon panelinde PA13/PA14'ü J4'e çeker.

## 4. Çekirdek doğrulama

1. `docs/cubemx-setup.md` ile üretilmiş firmware'i yükle.
2. **Saat doğrulama:** PA8'de (servo pini, servo takılı değilken) 50 Hz PWM ölç (logic analyzer/osiloskop). 50 Hz değilse kristal 8 MHz değildir → S2, saat ağacını düzelt.
3. Buzzer'dan boot cıvıltısı gelmeli (BOOT/SELFTEST deseni) — "yaşıyor" sinyali.
4. Log için: ST-Link varsa SWO/RTT; yoksa SD kartındaki CSV/BIN dosyaları tek gözlem aracı.

## 5. I2C ve sensörler

1. `hk_i2c_scan` çıktısına bak (log): **I2C1 → 0x68 (BMI270) + 0x76 (BMP280)**; **I2C2 → 0x44 (SHT4x_1)**; **bit-bang → 0x44 (SHT4x_2)**.
2. Adres eksikse: pull-up'ları (S8), lehimleri, besleme pinlerini ölç.
3. BMP280 basıncı yereldeki gerçek basınçla karşılaştır (METAR/telefon barometresi, ±300 Pa makul).
4. SHT4x×2 oda sıcaklığı/nem mantıklı mı; iki sensör birbirine yakın mı.
5. BMI270: config blob yüklemesi gerçek busta ilk kez çalışacak (riskli adım). Kart masada düz → az ≈ +9.81 m/s². Sonra PC4 EXTI yolunu etkinleştir.

## 6. GPS ve batarya

1. GPS: kapalı alanda NMEA cümleleri akmalı (fix olmasa da `$GNGGA,,,...`); açık alanda fix + uydu sayısı ≥ 4.
2. Batarya bölücüsü (**yalnız REV2A kartında var**; Şükrü kartında PC0 boş → bu adım atlanır): PSU'yu 9.0 / 11.1 / 12.6 V'a ayarla, `vbat` logunu multimetreyle karşılaştır. Sapma varsa `CONFIG.INI` → `bat_divider_ratio` ile kalibre et (reflash yok!).

## 7. SD kart

1. Modülü J3'e tak (CS/SCK/MOSI/MISO/5V/GND sırasını S7 ile teyit et), FAT32 kart tak.
2. Boot sonrası logta `SD mounted, session FL_0001` ara; kartı söküp PC'de `/LOGS/FL_0001.CSV` ve `.BIN` kontrol et: `python tools/hk_log_reader.py FL_0001.BIN`.
3. Sorunda: logic analyzer ile 400 kHz init aşamasını izle (CMD0 → R1=0x01 görülmeli). 5V modül MISO seviyesi şüpheliyse 3.3V yedek modüle geç.
4. Kart YOKKEN boot: sistem çalışmaya devam etmeli, log "degraded" demeli, kart takılınca ~5 sn'de mount olmalı (hot-insert).

## 8. CONFIG.INI ile saha ayarı (reflash'sız)

Kartın köküne `CONFIG.INI` koy; boot'ta NV değerlerinin üstüne yazılır:
```ini
# örnek — anahtar listesi: App/services/config/config.c
arm_altitude_m = 40
release_freefall_g = 0.30
landed_hold_ms = 5000
bat_divider_ratio = 11.08
```

## 9. Aktüatörler

1. **Servo** (PSU akım limitli): SERVO_5V rayı ayrı regülatörden; hold↔release açılarını gözle; mekanik sınırlara çarpmıyor mu.
2. **Buzzer** desenleri: her mission durumunun sesi ayırt edilebilir mi.
3. **Fanlar**: S4 cevabından sonra; termostat eşiğini ısı tabancası/elle test et (40°C aç / 35°C kapat).
4. **Solenoid — EN SON:** S3 cevabıyla teyitli: enerjisiz = KİLİTLİ, enerjili (~0.41 A) = bırakma; flyback diyotu var. Firmware bobini yalnız RELEASE penceresinde (varsayılan 1.5 sn) enerjiler. PSU akım limitiyle mekanik bırakmayı gözle; istersen S3'teki PWM tutma-akımı optimizasyonu ileride eklenebilir.

## 10. Sistem testi

1. **IWDG:** bir görevi kasıtlı kilitleyen test build'i ile reset atıldığını doğrula (log: reset reason = IWDG).
2. **Gece soak testi:** tam sistem 8+ saat; sabah SD logunda kopukluk/CRC hatası var mı: `hk_log_reader.py` istatistikleri.
3. **Masa uçuşu:** ARM eşiğini `CONFIG.INI` ile 2 m'ye çek; kapsülü elle kaldır-indir-salla-serbest bırak (yakalayarak!); SD logundaki EVENT dizisi `test_flight_e2e` beklentileriyle örtüşmeli.
4. **Saha provası:** gerçek ayrılma mekanizmasıyla düşük irtifa testi; sonrasında log analizi.

## Kabul ölçütleri

- [ ] Tüm raylar nominal, boşta akım kayıtlı
- [ ] Firmware yüklenebiliyor (SWD veya UART yolu belgeli)
- [ ] i2c_scan üç busta doğru adresleri buluyor
- [ ] 4 sensör + GPS mantıklı veri üretiyor
- [ ] SD oturum dosyaları PC'de açılıyor, CRC temiz
- [ ] Aktüatörler komutla doğru tepki veriyor (solenoid fail-safe doğrulanmış)
- [ ] IWDG + soak + masa uçuşu geçildi
