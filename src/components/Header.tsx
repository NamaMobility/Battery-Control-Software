import { Wifi, Zap, Power } from 'lucide-react';
import namaLogo from '../../nama-logo.svg';

interface HeaderProps {
  batteryData: any;
}

export default function Header({ batteryData }: HeaderProps) {
  return (
    <header className="bg-white shadow-lg">
      <div className="container mx-auto px-4 py-6">
        <div className="flex items-center justify-between flex-wrap gap-4">
          <div className="flex items-center gap-4">
            <img 
              src={namaLogo}
              alt="Nama Logo" 
              className="h-12"
            />
            <div>
              <h1 className="text-3xl font-bold text-gray-900">Batarya Yönetim Sistemi990</h1>
              <p className="text-gray-600 text-sm">Nama Mobility BMS Control Panel</p>
            </div>
          </div>

          <div className="flex gap-4 flex-wrap">
            <div className={`flex items-center gap-2 px-4 py-2 rounded-full ${
              true ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
            }`}>
              <Wifi size={20} />
              <span className="font-semibold">Bağlı</span>
            </div>
            <div className={`flex items-center gap-2 px-4 py-2 rounded-full ${
              batteryData.bmsSignal ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
            }`}>
              <Zap size={20} />
              <span className="font-semibold">BMS: {batteryData.bmsSignal ? 'Aktif' : 'Kapalı'}</span>
            </div>
            <div className={`flex items-center gap-2 px-4 py-2 rounded-full ${
              batteryData.contactSwitch ? 'bg-green-100 text-green-700' : 'bg-orange-100 text-orange-700'
            }`}>
              <Power size={20} />
              <span className="font-semibold">Kontak (TX2): {batteryData.contactSwitch ? 'Açık' : 'Kapalı'}</span>
            </div>
          </div>
        </div>
      </div>
    </header>
  );
}
