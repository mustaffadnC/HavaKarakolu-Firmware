# Elektronik Ekibine Sorular — ÇARGE Rev-2 PCB

> Yazılım ekibinden. PCB üretimdeyken netleşmesi gereken maddeler; **S1 acildir**,
> cevabına göre kart programlanamayabilir ve üretimden çıkmadan önlem gerekebilir.
> Cevapları bu dosyaya işleyin veya doğrudan yazılım sorumlusuna iletin.

## 🔴 S1 — SWD debug hattı (KRİTİK, ACİL)

Şemada `SWDIO`/`SWCLK` netleri **PB13 (pin 52) / PB14 (pin 53)** üzerinde
görünüyor ve **PA14 (pin 76)** boşta (NC) işaretli.

STM32'de SWD debug portu **silikonda PA13 (pin 72) / PA14 (pin 76)'ya sabittir,
başka pine taşınamaz.** J4 program konektöründeki SWDIO/SWCLK gerçekten
PB13/PB14'e gidiyorsa kart **ST-Link ile programlanamaz.**

- a) Bu bir şema etiket hatası mı, yoksa bakır da böyle mi çekildi?
- b) PA13 ve PA14, J4 konektörüne fiziksel olarak bağlı mı? (Netlist'ten teyit edin)
- c) Bağlı değilse: LQFP100 pin 72/76'dan J4'e bodge tel atılabilir mi,
     yoksa panelden revizyon mu planlanır?
- d) KiCad netlist dosyasını (`.net` veya şema PDF'inin tamamını) yazılım
     ekibiyle paylaşabilir misiniz?

**Yazılım tarafı önlemi:** doğrulanana kadar PB13/PB14 firmware'de hiçbir işleve
atanmayacak. Kurtarma planı: BOOT0=1 + dahili bootloader ile UART'tan yükleme (S5).

## 🟠 S2 — Kristal Y2 frekansı

Y2 kristalinin değeri şemada okunamadı (15pF yük kapasitörleri görünüyor).
- Frekans kaç MHz? (Saat ağacı **8 MHz** varsayımıyla kuruldu; farklıysa PLL
  ayarları değişecek.)
- LSE (32.768 kHz) yok, doğru mu?

## 🟠 S3 — Solenoid fail-safe polaritesi

PB0 (`Solenoid_Tetik`) → AO3406 low-side + 10K gate pulldown. MCU reset'teyken
PB0 LOW → MOSFET kapalı → **solenoid enerjisiz.**
- Solenoid enerjisizken mekanik olarak **KİLİTLİ mi, AÇIK mı?**
  (Güvenlik için istenen: enerjisiz = kilitli. Değilse yazılım stratejisi değişir.)
- Bobin gerilimi/akımı nedir, sürekli enerjili kalabilir mi (duty sınırı)?
- Flyback diyotu D4 (SS34) var, teyit ✓.

## 🟡 S4 — Fan besleme rayı

FAN1/FAN2 konektörlerinde "12V" neti var; ancak güç sayfasında 12V üreten
regülatör görünmüyor.
- "12V" = batarya direkt mi (`LION_PROTECTED`, 3S için 9.0–12.6V)?
- Fanlar bu aralıkta çalışmaya uygun mu?
- Solenoid hangi raydan besleniyor?

## 🟡 S5 — BOOT0 erişimi (SWD bozuksa kurtarma yolu)

BOOT0 (pin 94), R5 10K ile GND'de. Dahili bootloader'a girmek için BOOT0'ı
3.3V'a çekmek gerekiyor.
- R5'in pad'ine ya da BOOT0'a erişilebilir bir test noktası/jumper var mı?
- Yoksa: R5'in MCU tarafındaki pad'inden 3.3V'a geçici tel atmak mümkün mü?

## 🟡 S6 — PA13 durumu

PA14 şemada NC işaretli; **PA13 (pin 72)** de mi boşta? (S1 ile bağlantılı.)

## 🟢 S7 — J3 SD kart modül konektörü

J3: CS / SCK / MOSI / MISO / 5V / GND.
- Hedeflenen modül 5V beslemeli, level-shifter'lı Catalex tipi mi?
- MISO hattında seri direnç var mı? (Bazı modüllerde MISO 5V'a çekilir;
  STM32 pini 5V-tolerant ama teyit iyi olur.)

## 🟢 S8 — I2C1 pull-up teyidi

R10/R11 = 4.7K → 3.3V, PB6/PB7 üzerinde (BMI270 + BMP280 busı). Şemada
görünüyor ✓ — bakırda da böyle mi?

## 🟢 S9 — BMP280 bağlantı teyidi

CSB → 3.3V (I2C modu), SDO → GND (adres **0x76**). Şemadan böyle okundu ✓ —
teyit edin.

## 🟢 S10 — BMI270 INT hattı

PC4 ← `BMI270_INT`. Şemada **INT1 (pin 4)** görünüyor.
- INT1 doğru mu? Push-pull / aktif-yüksek varsayıyoruz.

## 🟢 S11 — Batarya bölücü toleransı

R3 100K / R22 10K (BAT_TEST, ÷11).
- Dirençler %1 mi? (Değilse her kart için yazılımda kalibrasyon yapılacak.)
- Batarya 3S Li-ion, doğru mu?

## 🟢 S12 — 3.3V güç bütçesi

AMS1117-3.3 (5V buck'tan besleniyor): MCU + GPS + sensörler + **SD kart yazma
tepeleri (~100–200 mA)** toplamını kaldırır mı? Termal olarak sorun var mı?

## 🟢 S13 — Toprak ortaklığı

SERVO_5V üreten XL4015'in çıkış GND'si lojik GND ile ortak mı?
(Servo PWM dönüş yolu için gerekli.)

---

| # | Konu | Cevap | Tarih |
|---|---|---|---|
| S1 | SWD PA13/PA14 | | |
| S2 | Y2 frekansı | | |
| S3 | Solenoid polaritesi | | |
| S4 | Fan rayı | | |
| S5 | BOOT0 erişimi | | |
| S6 | PA13 NC mi | | |
| S7 | SD modül tipi | | |
| S8 | I2C1 pull-up | | |
| S9 | BMP280 CSB/SDO | | |
| S10 | BMI270 INT1 | | |
| S11 | Bölücü toleransı | | |
| S12 | 3.3V bütçesi | | |
| S13 | GND ortaklığı | | |
