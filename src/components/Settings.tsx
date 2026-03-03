import { Settings as SettingsIcon, Zap, Gauge, Wifi } from 'lucide-react';
import { useState, useEffect } from 'react';
import EnergyHistory from './EnergyHistory';

interface SettingsProps {
  onServiceClick: () => void;
  batteryData: any;
}

interface Config {
  batteryType: number;
  seriesCount: number;
  cellCapacity: number;
  maxChargeCurrent: number;
  maxDischargeCurrent: number;
  prechargeTime: number;
  mosfetOnTime: number;
  shortCircuitThreshold: number;
  batteryCapacity: number;
  maxVoltage: number;
  minVoltage: number;
  sensor1Calibration: number;
  sensor2Calibration: number;
  wifiSsid?: string;
  wifiHasPass?: boolean;
}

interface WifiNetwork {
  ssid: string;
  rssi: number;
  channel: number;
  secure: boolean;
}

export default function Settings({ onServiceClick, batteryData }: SettingsProps) {
  const [config, setConfig] = useState<Config>({
    batteryType: 0,
    seriesCount: 16,
    cellCapacity: 100,
    maxChargeCurrent: 100,
    maxDischargeCurrent: 100,
    prechargeTime: 2,
    mosfetOnTime: 10,
    shortCircuitThreshold: 200,
    batteryCapacity: 0,
    maxVoltage: 0,
    minVoltage: 0,
    sensor1Calibration: 1.0,
    sensor2Calibration: 1.0,
    wifiSsid: '',
    wifiHasPass: false,
  });

  const [saved, setSaved] = useState(false);
  const [activeTab, setActiveTab] = useState<'settings' | 'pins' | 'calibration'>('settings');
  const [calibrationCurrent, setCalibrationCurrent] = useState(40);
  const [wifiPassInput, setWifiPassInput] = useState('');
  const [wifiNetworks, setWifiNetworks] = useState<WifiNetwork[]>([]);
  const [scanningNetworks, setScanningNetworks] = useState(false);

  useEffect(() => {
    const loadConfig = async () => {
      try {
        const response = await fetch('/api/config');
        const data = await response.json();
        setConfig(data);
      } catch (error) {
        console.error('Ayarlar yüklenemedi:', error);
      }
    };
    loadConfig();
  }, []);

  const scanNetworks = async () => {
    setScanningNetworks(true);
    try {
      const response = await fetch('/api/scan-networks');
      const data = await response.json();
      setWifiNetworks(data.networks || []);
    } catch (error) {
      console.error('Ağ taraması hatası:', error);
    } finally {
      setScanningNetworks(false);
    }
  };

  const saveSettings = async () => {
    try {
      const payload: Record<string, unknown> = { ...config };
      if (wifiPassInput.trim().length > 0) {
        payload.wifiPass = wifiPassInput.trim();
      }
      const response = await fetch('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'X-Service-Password': 'nama2026',
        },
        body: JSON.stringify(payload),
      });
      if (response.ok) {
        setSaved(true);
        setWifiPassInput('');
        setTimeout(() => setSaved(false), 3000);
      }
    } catch (error) {
      console.error('Kaydetme hatası:', error);
    }
  };

  const calibrateSensor = async (sensor: number) => {
    if (window.confirm(`Sensör ${sensor} kalibrasyonu başlatılsın mı? Lütfen ${calibrationCurrent}A akım uygulayın.`)) {
      try {
        const response = await fetch('/api/calibrate', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ sensor, referenceCurrent: calibrationCurrent }),
        });
        if (response.ok) {
          const data = await response.json();
          if (sensor === 1) {
            setConfig({ ...config, sensor1Calibration: data.calibrationFactor });
          } else {
            setConfig({ ...config, sensor2Calibration: data.calibrationFactor });
          }
          setSaved(true);
          setTimeout(() => setSaved(false), 3000);
        }
      } catch (error) {
        console.error('Kalibrasyon hatası:', error);
      }
    }
  };

  const clearHistory = async () => {
    if (window.confirm('Geçmiş verileri silmek istediğinize emin misiniz?')) {
      try {
        await fetch('/api/clear-history', {
          method: 'POST',
          headers: { 'X-Service-Password': 'nama2026' },
        });
        setSaved(true);
        setTimeout(() => setSaved(false), 3000);
      } catch (error) {
        console.error('Temizleme hatası:', error);
      }
    }
  };

  const batteryTypes = [
    { value: 0, label: 'LiFePO4' },
    { value: 1, label: 'Li-ion' },
    { value: 2, label: 'LiPo' },
  ];

  const pinConfig = [
    { pin: 'D19', type: 'Giriş', desc: 'Ledli buton (giriş)' },
    { pin: 'D18', type: 'Giriş', desc: 'Acil stop butonu' },
    { pin: 'TX2', type: 'Giriş', desc: 'Kontak anahtarı (GND gelince aktif)' },
    { pin: 'D4', type: 'Giriş', desc: 'Optokuplörlü BMS sinyal girişi' },
    { pin: 'D23', type: 'Çıkış', desc: 'Ledli butonun LED\'i' },
    { pin: 'D13', type: 'Çıkış', desc: 'Preşarj rölesi (HIGH\'da sürülür)' },
    { pin: 'D14', type: 'Çıkış', desc: 'Deşarj rölesi 1 (HIGH\'da sürülür)' },
    { pin: 'D27', type: 'Çıkış', desc: 'Deşarj rölesi 2 (HIGH\'da sürülür)' },
    { pin: 'D33', type: 'Çıkış', desc: 'MOSFET 1 (LOW\'da sürülür) + Hass400S akım sensörü 1' },
    { pin: 'D32', type: 'Çıkış', desc: 'MOSFET 2 (LOW\'da sürülür) + Hass400S akım sensörü 2' },
  ];

  return (
    <>
      <div className="bg-white rounded-lg shadow-lg p-8">
        <h2 className="text-2xl font-bold text-gray-900 mb-6 flex items-center gap-2">
          <SettingsIcon className="text-blue-600" />
          Sistem Ayarları (Servis Modu)
        </h2>

        <div className="flex gap-2 mb-6 border-b">
          <button
            onClick={() => setActiveTab('settings')}
            className={`px-4 py-2 font-semibold ${
              activeTab === 'settings'
                ? 'border-b-2 border-blue-600 text-blue-600'
                : 'text-gray-600 hover:text-gray-900'
            }`}
          >
            Sistem Ayarları
          </button>
          <button
            onClick={() => setActiveTab('pins')}
            className={`px-4 py-2 font-semibold ${
              activeTab === 'pins'
                ? 'border-b-2 border-blue-600 text-blue-600'
                : 'text-gray-600 hover:text-gray-900'
            }`}
          >
            Pin Konfigürasyonu
          </button>
          <button
            onClick={() => setActiveTab('calibration')}
            className={`px-4 py-2 font-semibold ${
              activeTab === 'calibration'
                ? 'border-b-2 border-blue-600 text-blue-600'
                : 'text-gray-600 hover:text-gray-900'
            }`}
          >
            Kalibrasyon
          </button>
        </div>

        {activeTab === 'settings' && (
          <>
            <div className="bg-gray-50 rounded-lg p-4 mb-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
                <Wifi size={20} />
                WiFi Ayarlari
              </h3>
              
              <div className="mb-4">
                <button
                  onClick={scanNetworks}
                  disabled={scanningNetworks}
                  className="w-full bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white font-semibold py-2 px-4 rounded-lg transition-colors flex items-center justify-center gap-2"
                >
                  <Wifi size={18} />
                  {scanningNetworks ? 'Ağlar Taranıyor...' : 'Mevcut Ağları Tara'}
                </button>
              </div>

              {wifiNetworks.length > 0 && (
                <div className="mb-4">
                  <label className="block text-sm font-semibold text-gray-700 mb-2">Kullanılabilir Ağlar</label>
                  <select
                    value={config.wifiSsid || ''}
                    onChange={(e) => setConfig({ ...config, wifiSsid: e.target.value })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                  >
                    <option value="">-- Bir ağ seçin --</option>
                    {wifiNetworks.map((network) => (
                      <option key={network.ssid} value={network.ssid}>
                        {network.ssid} {network.secure && '🔒'} ({network.rssi} dBm)
                      </option>
                    ))}
                  </select>
                </div>
              )}

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-semibold text-gray-700 mb-2">WiFi Ağ Adı (SSID)</label>
                  <input
                    type="text"
                    value={config.wifiSsid || ''}
                    onChange={(e) => setConfig({ ...config, wifiSsid: e.target.value })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    placeholder="Örnek: Ev_WiFi"
                  />
                  <p className="text-xs text-gray-500 mt-1">Yukarıdan seç veya manuel gir</p>
                </div>
                <div>
                  <label className="block text-sm font-semibold text-gray-700 mb-2">WiFi Şifresi</label>
                  <input
                    type="password"
                    value={wifiPassInput}
                    onChange={(e) => setWifiPassInput(e.target.value)}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    placeholder={config.wifiHasPass ? 'Kayıtlı şifre var' : 'Şifre girin'}
                  />
                  <p className="text-xs text-gray-500 mt-1">
                    Şifre boş bırakılırsa mevcut şifre değişmez.
                  </p>
                </div>
              </div>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6 mb-6">
              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Batarya Tipi</label>
                <select
                  value={config.batteryType}
                  onChange={(e) => setConfig({ ...config, batteryType: parseInt(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                >
                  {batteryTypes.map((type) => (
                    <option key={type.value} value={type.value}>
                      {type.label}
                    </option>
                  ))}
                </select>
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Hücre Sayısı</label>
                <input
                  type="number"
                  min="1"
                  max="100"
                  value={config.seriesCount}
                  onChange={(e) => setConfig({ ...config, seriesCount: parseInt(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Hücre Kapasitesi (Ah)</label>
                <input
                  type="number"
                  step="0.1"
                  value={config.cellCapacity}
                  onChange={(e) => setConfig({ ...config, cellCapacity: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Preşarj Süresi (sn)</label>
                <input
                  type="number"
                  step="0.1"
                  value={config.prechargeTime}
                  onChange={(e) => setConfig({ ...config, prechargeTime: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">MOSFET Açık Kalma Süresi (sn)</label>
                <input
                  type="number"
                  step="0.1"
                  value={config.mosfetOnTime}
                  onChange={(e) => setConfig({ ...config, mosfetOnTime: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Maks. Şarj Akımı (A)</label>
                <input
                  type="number"
                  step="0.1"
                  value={config.maxChargeCurrent}
                  onChange={(e) => setConfig({ ...config, maxChargeCurrent: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Maks. Deşarj Akımı (A)</label>
                <input
                  type="number"
                  step="0.1"
                  value={config.maxDischargeCurrent}
                  onChange={(e) => setConfig({ ...config, maxDischargeCurrent: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Kısa Devre Eşiği (A)</label>
                <input
                  type="number"
                  value={config.shortCircuitThreshold}
                  onChange={(e) => setConfig({ ...config, shortCircuitThreshold: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                />
              </div>
            </div>

            <div className="bg-blue-50 p-4 rounded-lg mb-6">
              <label className="block text-xs font-semibold text-gray-600 mb-1">Batarya Kapasitesi</label>
              <div className="text-2xl font-bold text-blue-600">{config.batteryCapacity.toFixed(2)} kWh</div>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
              <div className="bg-gray-50 p-4 rounded-lg">
                <span className="text-sm text-gray-600 font-semibold">Max Voltaj</span>
                <div className="text-xl font-bold text-gray-900 mt-1">{config.maxVoltage.toFixed(2)} V</div>
              </div>
              <div className="bg-gray-50 p-4 rounded-lg">
                <span className="text-sm text-gray-600 font-semibold">Min Voltaj</span>
                <div className="text-xl font-bold text-gray-900 mt-1">{config.minVoltage.toFixed(2)} V</div>
              </div>
              <div className="bg-gray-50 p-4 rounded-lg">
                <span className="text-sm text-gray-600 font-semibold">IP Adresi</span>
                <div className="text-xl font-bold text-gray-900 mt-1">192.168.25.25</div>
              </div>
            </div>
          </>
        )}

        {activeTab === 'pins' && (
          <div className="space-y-4">
            <div className="bg-blue-50 p-4 rounded-lg mb-4">
              <h3 className="font-bold text-blue-900 mb-2">ESP32 Pin Atamaları</h3>
              <p className="text-sm text-blue-700">
                Aşağıda sistemin tüm pin konfigürasyonları listelenmiştir. Bu ayarlar donanım tasarımına göre yapılandırılmıştır.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {pinConfig.map((pin, index) => (
                <div
                  key={index}
                  className={`p-4 rounded-lg border-2 ${
                    pin.type === 'Giriş' ? 'bg-green-50 border-green-200' : 'bg-orange-50 border-orange-200'
                  }`}
                >
                  <div className="flex items-center justify-between mb-2">
                    <span className="font-bold text-lg text-gray-900">{pin.pin}</span>
                    <span
                      className={`px-3 py-1 rounded-full text-xs font-semibold ${
                        pin.type === 'Giriş' ? 'bg-green-200 text-green-800' : 'bg-orange-200 text-orange-800'
                      }`}
                    >
                      {pin.type}
                    </span>
                  </div>
                  <p className="text-sm text-gray-700">{pin.desc}</p>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'calibration' && (
          <div className="space-y-6">
            <div className="bg-gray-50 rounded-lg p-4 mb-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
                <Wifi size={20} />
                WiFi Ayarlari
              </h3>
              
              <div className="mb-4">
                <button
                  onClick={scanNetworks}
                  disabled={scanningNetworks}
                  className="w-full bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white font-semibold py-2 px-4 rounded-lg transition-colors flex items-center justify-center gap-2"
                >
                  <Wifi size={18} />
                  {scanningNetworks ? 'Ağlar Taranıyor...' : 'Mevcut Ağları Tara'}
                </button>
              </div>

              {wifiNetworks.length > 0 && (
                <div className="mb-4">
                  <label className="block text-sm font-semibold text-gray-700 mb-2">Kullanılabilir Ağlar</label>
                  <select
                    value={config.wifiSsid || ''}
                    onChange={(e) => setConfig({ ...config, wifiSsid: e.target.value })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                  >
                    <option value="">-- Bir ağ seçin --</option>
                    {wifiNetworks.map((network) => (
                      <option key={network.ssid} value={network.ssid}>
                        {network.ssid} {network.secure && '🔒'} ({network.rssi} dBm)
                      </option>
                    ))}
                  </select>
                </div>
              )}

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-semibold text-gray-700 mb-2">WiFi Ağ Adı (SSID)</label>
                  <input
                    type="text"
                    value={config.wifiSsid || ''}
                    onChange={(e) => setConfig({ ...config, wifiSsid: e.target.value })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    placeholder="Örnek: Ev_WiFi"
                  />
                  <p className="text-xs text-gray-500 mt-1">Yukarıdan seç veya manuel gir</p>
                </div>
                <div>
                  <label className="block text-sm font-semibold text-gray-700 mb-2">WiFi Şifresi</label>
                  <input
                    type="password"
                    value={wifiPassInput}
                    onChange={(e) => setWifiPassInput(e.target.value)}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    placeholder={config.wifiHasPass ? 'Kayıtlı şifre var' : 'Şifre girin'}
                  />
                  <p className="text-xs text-gray-500 mt-1">
                    Şifre boş bırakılırsa mevcut şifre değişmez.
                  </p>
                </div>
              </div>
            </div>

            <div className="bg-yellow-50 p-4 rounded-lg mb-4">
              <h3 className="font-bold text-yellow-900 mb-2 flex items-center gap-2">
                <Gauge size={20} />
                Akım Sensörü Kalibrasyonu
              </h3>
              <p className="text-sm text-yellow-700">
                Kalibrasyon için bilinen bir akım değeri (örn: 40A) uygulayın ve sensörü kalibre edin.
                Kalibrasyon faktörleri otomatik hesaplanacak ve kaydedilecektir.
              </p>
            </div>

            <div className="bg-white border-2 border-gray-200 p-6 rounded-lg">
              <label className="block text-sm font-semibold text-gray-700 mb-2">Referans Akım Değeri (A)</label>
              <input
                type="number"
                step="0.1"
                value={calibrationCurrent}
                onChange={(e) => setCalibrationCurrent(parseFloat(e.target.value))}
                className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent mb-4"
                placeholder="Örn: 40"
              />
              <p className="text-xs text-gray-500 mb-4">
                Kalibrasyon sırasında bu değerde akım uygulamanız gerekmektedir.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div className="bg-gradient-to-br from-green-50 to-green-100 p-6 rounded-lg border-2 border-green-200">
                <div className="flex items-center gap-2 mb-4">
                  <Zap className="text-green-600" size={24} />
                  <h3 className="font-bold text-lg text-gray-900">Sensör 1 (Şarj)</h3>
                </div>
                <div className="mb-4">
                  <div className="text-sm text-gray-600 mb-1">Mevcut Okunan Akım</div>
                  <div className="text-3xl font-bold text-green-600">{batteryData.chargeCurrent.toFixed(2)} A</div>
                </div>
                <div className="mb-4">
                  <div className="text-sm text-gray-600 mb-1">Kalibrasyon Faktörü</div>
                  <div className="text-xl font-bold text-gray-900">{config.sensor1Calibration.toFixed(4)}</div>
                </div>
                <button
                  onClick={() => calibrateSensor(1)}
                  className="w-full bg-green-600 hover:bg-green-700 text-white font-bold py-3 px-4 rounded-lg transition-colors flex items-center justify-center gap-2"
                >
                  <Gauge size={20} />
                  Sensör 1 Kalibre Et
                </button>
              </div>

              <div className="bg-gradient-to-br from-orange-50 to-orange-100 p-6 rounded-lg border-2 border-orange-200">
                <div className="flex items-center gap-2 mb-4">
                  <Zap className="text-orange-600" size={24} />
                  <h3 className="font-bold text-lg text-gray-900">Sensör 2 (Deşarj)</h3>
                </div>
                <div className="mb-4">
                  <div className="text-sm text-gray-600 mb-1">Mevcut Okunan Akım</div>
                  <div className="text-3xl font-bold text-orange-600">{batteryData.dischargeCurrent.toFixed(2)} A</div>
                </div>
                <div className="mb-4">
                  <div className="text-sm text-gray-600 mb-1">Kalibrasyon Faktörü</div>
                  <div className="text-xl font-bold text-gray-900">{config.sensor2Calibration.toFixed(4)}</div>
                </div>
                <button
                  onClick={() => calibrateSensor(2)}
                  className="w-full bg-orange-600 hover:bg-orange-700 text-white font-bold py-3 px-4 rounded-lg transition-colors flex items-center justify-center gap-2"
                >
                  <Gauge size={20} />
                  Sensör 2 Kalibre Et
                </button>
              </div>
            </div>
          </div>
        )}

        <div className="flex flex-wrap gap-3 mt-6">
          <button
            onClick={saveSettings}
            className="flex-1 min-w-40 bg-blue-600 hover:bg-blue-700 text-white font-bold py-2 px-6 rounded-lg transition-colors"
          >
            Ayarları Kaydet
          </button>
          <button
            onClick={onServiceClick}
            className="flex-1 min-w-40 bg-orange-600 hover:bg-orange-700 text-white font-bold py-2 px-6 rounded-lg transition-colors"
          >
            Servis Modu
          </button>
          <button
            onClick={clearHistory}
            className="flex-1 min-w-40 bg-red-600 hover:bg-red-700 text-white font-bold py-2 px-6 rounded-lg transition-colors"
          >
            Geçmiş Temizle
          </button>
        </div>

        {saved && (
          <div className="mt-4 bg-green-50 border-l-4 border-green-500 p-3 rounded">
            <p className="text-green-700 font-semibold">İşlem başarılı!</p>
          </div>
        )}
      </div>

      <EnergyHistory />
    </>
  );
}
