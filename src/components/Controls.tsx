import { AlertCircle, Play, Square, Lightbulb, Info } from 'lucide-react';
import { useState } from 'react';

interface ControlsProps {
  batteryData: any;
  onActionComplete?: () => void | Promise<void>;
}

export default function Controls({ batteryData, onActionComplete }: ControlsProps) {
  const [loading, setLoading] = useState(false);

  const runAction = async (action: () => Promise<void>) => {
    setLoading(true);
    try {
      await action();
    } catch (error) {
      console.error('Kontrol hatası:', error);
    } finally {
      if (onActionComplete) {
        await Promise.resolve(onActionComplete());
      }
      setLoading(false);
    }
  };

  const controlRelay = async (relay: string) => {
    await runAction(async () => {
      await fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ relay, action: 'toggle' }),
      });
    });
  };

  const startSequence = async (type: string) => {
    await runAction(async () => {
      await fetch('/api/sequence', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type }),
      });
    });
  };

  const emergency = async () => {
    await runAction(async () => {
      await fetch('/api/emergency', { method: 'POST' });
    });
  };

  const triggerPhysicalButton = async () => {
    await runAction(async () => {
      await fetch('/api/button', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ durationMs: 120 }),
      });
    });
  };

  const getSystemStateColor = (state: string) => {
    switch (state) {
      case 'IDLE':
        return 'bg-gray-500';
      case 'STARTUP':
        return 'bg-blue-500 animate-pulse';
      case 'PRECHARGE':
        return 'bg-yellow-500 animate-pulse';
      case 'MOSFET_ACTIVE':
        return 'bg-orange-500 animate-pulse';
      case 'DISCHARGE_ACTIVE':
        return 'bg-green-500 animate-pulse';
      case 'RUNNING':
        return 'bg-green-600';
      case 'BYPASS_MODE':
        return 'bg-purple-500';
      case 'FAULT':
        return 'bg-red-600 animate-pulse';
      case 'EMERGENCY_STOP':
        return 'bg-red-700 animate-pulse';
      default:
        return 'bg-gray-400';
    }
  };

  const getSystemStateText = (state: string) => {
    switch (state) {
      case 'IDLE':
        return 'Bekleme';
      case 'STARTUP':
        return 'Başlatılıyor';
      case 'PRECHARGE':
        return 'Preşarj';
      case 'MOSFET_ACTIVE':
        return 'MOSFET Aktif';
      case 'DISCHARGE_ACTIVE':
        return 'Deşarj Aktif';
      case 'RUNNING':
        return 'Çalışıyor';
      case 'BYPASS_MODE':
        return 'Bypass Modu';
      case 'FAULT':
        return 'Arıza';
      case 'EMERGENCY_STOP':
        return 'Acil Durdurma';
      default:
        return 'Bilinmiyor';
    }
  };

  const getLEDAnimation = (mode: string) => {
    switch (mode) {
      case 'NORMAL':
        return 'animate-pulse';
      case 'EMERGENCY':
        return 'animate-ping';
      case 'CONTACT_CLOSED':
        return 'animate-bounce';
      case 'BYPASS':
        return '';
      case 'OFF':
        return '';
      default:
        return '';
    }
  };

  const getLEDColor = (mode: string) => {
    switch (mode) {
      case 'NORMAL':
        return 'bg-blue-500';
      case 'EMERGENCY':
        return 'bg-red-500';
      case 'CONTACT_CLOSED':
        return 'bg-orange-500';
      case 'BYPASS':
        return 'bg-purple-500';
      case 'OFF':
        return 'bg-gray-300';
      default:
        return 'bg-gray-400';
    }
  };

  const getLEDDescription = (mode: string) => {
    switch (mode) {
      case 'NORMAL':
        return '1 sn ON / 1 sn OFF';
      case 'EMERGENCY':
        return '0.25 sn ON / 0.25 sn OFF (Hızlı)';
      case 'CONTACT_CLOSED':
        return '5 sn ON / 0.15 sn OFF';
      case 'BYPASS':
        return 'Sürekli Yanık';
      case 'OFF':
        return 'Kapalı';
      default:
        return 'Bilinmiyor';
    }
  };

  const RelayItem = ({ name, active }: { name: string; active: boolean }) => (
    <div className="flex items-center justify-between p-3 bg-gray-50 rounded-lg">
      <span className="font-semibold text-gray-700">{name}</span>
      <div className={`w-4 h-4 rounded-full ${active ? 'bg-green-500' : 'bg-red-400'}`} />
    </div>
  );

  return (
    <div className="space-y-4">
      <div className="bg-white rounded-lg shadow-lg p-6">
        <h2 className="text-xl font-bold text-gray-900 mb-4">Sistem Durumu</h2>

        <div className="bg-gradient-to-r from-blue-50 to-blue-100 p-6 rounded-lg border-2 border-blue-200 mb-4">
          <div className="flex items-center justify-between mb-3">
            <span className="text-sm font-semibold text-gray-700">Durum Makinesi</span>
            <div className={`w-3 h-3 rounded-full ${getSystemStateColor(batteryData.systemState)}`} />
          </div>
          <div className="text-2xl font-bold text-gray-900">
            {getSystemStateText(batteryData.systemState)}
          </div>
        </div>

        <div className="bg-gradient-to-r from-yellow-50 to-yellow-100 p-6 rounded-lg border-2 border-yellow-200">
          <div className="flex items-center gap-2 mb-3">
            <Lightbulb className="text-yellow-600" size={24} />
            <span className="text-sm font-semibold text-gray-700">LED Durumu (D23)</span>
          </div>
          <div className="flex items-center gap-4">
            <div className={`w-8 h-8 rounded-full ${getLEDColor(batteryData.ledMode)} ${getLEDAnimation(batteryData.ledMode)}`} />
            <div>
              <div className="text-lg font-bold text-gray-900">{batteryData.ledMode}</div>
              <div className="text-xs text-gray-600">{getLEDDescription(batteryData.ledMode)}</div>
              <div className="text-xs font-semibold mt-1 text-gray-700">
                D23 Anlık: {batteryData.ledOn ? 'YANIK' : 'SÖNÜK'}
              </div>
            </div>
          </div>
        </div>

        {batteryData.bypassMode && (
          <div className="mt-4 bg-purple-50 border-l-4 border-purple-500 p-4 rounded">
            <div className="flex items-center gap-2 text-purple-700">
              <Info size={20} />
              <span className="font-semibold">Bypass Modu Aktif</span>
            </div>
            <p className="text-sm text-purple-600 mt-1">BMS sinyali olmadan çalışıyor</p>
          </div>
        )}

        {batteryData.emergencyStop && (
          <div className="mt-4 bg-red-50 border-l-4 border-red-500 p-4 rounded">
            <div className="flex items-center gap-2 text-red-700">
              <AlertCircle size={20} />
              <span className="font-semibold">Acil Stop Aktif!</span>
            </div>
            <p className="text-sm text-red-600 mt-1">Tüm çıkışlar kapatıldı</p>
          </div>
        )}

        {batteryData.faultMessage && (
          <div className="mt-4 bg-red-50 border-l-4 border-red-500 p-4 rounded">
            <div className="flex items-center gap-2 text-red-700">
              <AlertCircle size={20} />
              <span className="font-semibold">Arıza!</span>
            </div>
            <p className="text-sm text-red-600 mt-1">{batteryData.faultMessage}</p>
            <p className="text-xs text-red-500 mt-2">
              Arızayı silmek için butona 15-20 sn basılı tutup bırakın
            </p>
          </div>
        )}
      </div>

      <div className="bg-white rounded-lg shadow-lg p-6">
        <h2 className="text-xl font-bold text-gray-900 mb-4">Röle Durumu</h2>
        <div className="space-y-2">
          <RelayItem name="Şarj (D14)" active={batteryData.chargeRelay} />
          <RelayItem name="Deşarj (D27)" active={batteryData.dischargeRelay} />
          <RelayItem name="Preşarj (D13)" active={batteryData.prechargeRelay} />
          <RelayItem name="MOSFET 1 (D33)" active={batteryData.mosfet1} />
          <RelayItem name="MOSFET 2 (D32)" active={batteryData.mosfet2} />
        </div>
      </div>

      <div className="bg-white rounded-lg shadow-lg p-6">
        <h3 className="font-bold text-gray-900 mb-3">Kontrol</h3>
        <div className="space-y-2">
          <button
            onClick={triggerPhysicalButton}
            disabled={loading}
            className="w-full bg-blue-500 hover:bg-blue-600 text-white font-bold py-2 px-4 rounded-lg transition-colors disabled:opacity-50"
          >
            <span className="flex items-center justify-center gap-2">
              <span
                className={`w-3 h-3 rounded-full border border-white/60 ${batteryData.ledOn ? 'bg-lime-300 shadow-sm animate-pulse' : 'bg-slate-300'}`}
              />
              <span>Başlatma Butonu</span>
            </span>
          </button>
          <button
            onClick={() => startSequence('start')}
            disabled={loading}
            className="w-full bg-green-500 hover:bg-green-600 text-white font-bold py-2 px-4 rounded-lg flex items-center justify-center gap-2 transition-colors disabled:opacity-50"
          >
            <Play size={20} />
            Sistem Başlat
          </button>
          <button
            onClick={emergency}
            disabled={loading}
            className="w-full bg-red-500 hover:bg-red-600 text-white font-bold py-2 px-4 rounded-lg flex items-center justify-center gap-2 transition-colors disabled:opacity-50"
          >
            <Square size={20} />
            Acil Durdur
          </button>
        </div>
      </div>
    </div>
  );
}
