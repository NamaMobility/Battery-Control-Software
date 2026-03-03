# Nama Mobility BMS - Batarya Yönetim Sistemi Web Arayüzü

ESP32 tabanlı BMS (Batarya Yönetim Sistemi) kontrol kartı için modern web arayüzü.

## Genel Bakış

Bu proje, elektrikli araçlarda batarya güvenliğini sağlamak için ESP32 tabanlı bir BMS kontrol kartının web arayüzünü içerir. Sistem, röleler, MOSFET'ler ve çeşitli giriş/çıkışları kontrol eder.

## Donanım Pin Konfigürasyonu

### Girişler
- **D19**: Ledli buton (giriş)
- **D18**: Acil stop butonu
- **TX2**: Kontak anahtarı (GND gelince aktif)
- **D4**: Optokuplörlü BMS sinyal girişi

### Çıkışlar
- **D23**: Ledli butonun LED'i
- **D13**: Preşarj rölesi (HIGH'da sürülür)
- **D14**: Deşarj rölesi 1 (HIGH'da sürülür)
- **D27**: Deşarj rölesi 2 (HIGH'da sürülür)
- **D33**: MOSFET 1 (LOW'da sürülür) + Hass400S akım sensörü 1
- **D32**: MOSFET 2 (LOW'da sürülür) + Hass400S akım sensörü 2

## Sistem Durumları

Sistem aşağıdaki durum makinesi ile çalışır:

1. **IDLE** - Bekleme
2. **STARTUP** - Başlatılıyor
3. **PRECHARGE** - Preşarj
4. **MOSFET_ACTIVE** - MOSFET Aktif
5. **DISCHARGE_ACTIVE** - Deşarj Aktif
6. **RUNNING** - Çalışıyor
7. **BYPASS_MODE** - Bypass Modu
8. **FAULT** - Arıza
9. **EMERGENCY_STOP** - Acil Durdurma

## LED Davranışları

- **Normal çalışma**: 1 sn ON / 1 sn OFF
- **Acil stop aktif**: 0.25 sn ON / 0.25 sn OFF (hızlı blink)
- **Kontak kapalı**: 5 sn ON / 0.15 sn OFF
- **Bypass modu**: Sürekli yanık
- **Kapalı**: LED sönük

## Özellikler

### Ana Özellikler
- Gerçek zamanlı batarya durumu izleme (SOC, voltaj, sıcaklık)
- Şarj ve deşarj akımı ölçümü
- Röle ve MOSFET durumlarının görüntülenmesi
- Sistem durum makinesi ve LED durumu göstergesi
- Acil stop ve hata yönetimi
- Bypass modu desteği

### Ayarlar ve Konfigürasyon
- **Sistem Ayarları**:
  - Batarya tipi seçimi (LiFePO4, Li-ion, LiPo)
  - Hücre sayısı ve kapasite ayarları
  - Preşarj ve MOSFET açık kalma süreleri
  - Maksimum şarj/deşarj akımı sınırları
  - Kısa devre koruma eşiği

- **Pin Konfigürasyonu**:
  - Tüm giriş ve çıkış pinlerinin detaylı listesi
  - Pin açıklamaları ve fonksiyonları

- **Kalibrasyon**:
  - Hass400S akım sensörlerinin kalibrasyonu
  - Referans akım değeri ile otomatik kalibrasyon
  - Kalibrasyon faktörlerinin görüntülenmesi

### Koruma Özellikleri
- BMS sinyal kontrolü
- Acil stop butonu
- Kısa devre koruması
- Akım limitleri kontrolü
- Kontak anahtarı senaryoları

## Kurulum

### Geliştirme Ortamı

```bash
npm install
npm run dev
```

### Production Build

```bash
npm run build
```

Build edilen dosyalar `dist/` klasöründe oluşturulur.

### ESP32 SPIFFS (Web Arayüzü Yükleme)

Web arayüzünü ESP32 dosya sistemine yüklemek için:

```bash
npm run build:spiffs
```

Sonrasında PlatformIO ile SPIFFS imajını yükleyin:

```bash
platformio run --target uploadfs --environment esp32dev
```

Not: Proje artık `data/` klasörünü otomatik oluşturur. Ancak web dosyalarının yüklenmesi için önce `npm run build:spiffs` çalıştırılmalıdır.

## Teknolojiler

- **React** - UI framework
- **TypeScript** - Tip güvenliği
- **Vite** - Build tool
- **Tailwind CSS** - Styling
- **Lucide React** - İkonlar

## API Endpoint'leri

Arayüz aşağıdaki API endpoint'lerini kullanır:

- `GET /api/status` - Sistem durumu
- `GET /api/config` - Konfigürasyon ayarları
- `POST /api/config` - Ayarları kaydet
- `POST /api/control` - Röle kontrolü
- `POST /api/sequence` - Sistem başlatma
- `POST /api/emergency` - Acil stop
- `POST /api/calibrate` - Sensör kalibrasyonu
- `POST /api/clear-history` - Geçmiş temizle

## Servis Modu

Servis moduna erişmek için "Servis Modu" butonuna tıklayın. Varsayılan şifre: `nama2026`

## Güvenlik Notları

- Tüm kritik işlemler için servis şifresi gereklidir
- Kısa devre koruması otomatik devrededir
- Acil stop butonu her zaman erişilebilirdir
- BMS sinyali olmadan sistem başlatılamaz (bypass modu hariç)

## Lisans

© 2024 Nama Mobility. Tüm hakları saklıdır.
