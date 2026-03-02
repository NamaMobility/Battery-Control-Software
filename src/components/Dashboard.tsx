import { Battery, Zap, Activity } from 'lucide-react';

interface DashboardProps {
  batteryData: any;
}

export default function Dashboard({ batteryData }: DashboardProps) {
  const rotation = (batteryData.soc / 100) * 360;

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
      <div className="bg-white rounded-lg shadow-lg p-8">
        <h2 className="text-2xl font-bold text-gray-900 mb-6 flex items-center gap-2">
          <Battery className="text-blue-600" />
          Batarya Durumu
        </h2>

        <div className="flex flex-col items-center gap-8">
          <div className="relative w-56 h-56">
            <svg className="w-full h-full transform -rotate-90" viewBox="0 0 200 200">
              <circle
                cx="100"
                cy="100"
                r="90"
                fill="none"
                stroke="#e5e7eb"
                strokeWidth="8"
              />
              <circle
                cx="100"
                cy="100"
                r="90"
                fill="none"
                stroke="#3b82f6"
                strokeWidth="8"
                strokeDasharray={`${(batteryData.soc / 100) * 565.48} 565.48`}
                className="transition-all duration-500"
              />
            </svg>
            <div className="absolute inset-0 flex flex-col items-center justify-center">
              <div className="text-5xl font-bold text-gray-900">{batteryData.soc.toFixed(1)}%</div>
              <div className="text-gray-500 font-semibold">SOC</div>
            </div>
          </div>

          <div className="w-full space-y-3">
            <div className="flex justify-between items-center p-3 bg-gray-50 rounded-lg">
              <span className="text-gray-700 font-semibold">Voltaj:</span>
              <span className="text-lg font-bold text-blue-600">{batteryData.voltage.toFixed(2)}V</span>
            </div>
            <div className="flex justify-between items-center p-3 bg-gray-50 rounded-lg">
              <span className="text-gray-700 font-semibold">Sıcaklık:</span>
              <span className="text-lg font-bold text-blue-600">{batteryData.temperature.toFixed(1)}°C</span>
            </div>
            <div className="flex justify-between items-center p-3 bg-gray-50 rounded-lg">
              <span className="text-gray-700 font-semibold">Kapasite:</span>
              <span className="text-lg font-bold text-blue-600">{batteryData.capacity.toFixed(2)} kWh</span>
            </div>
            <div className="flex justify-between items-center p-3 bg-gray-50 rounded-lg">
              <span className="text-gray-700 font-semibold">Cycle:</span>
              <span className="text-lg font-bold text-blue-600">{batteryData.cycleCount}</span>
            </div>
            <div className="flex justify-between items-center p-3 bg-gradient-to-r from-gray-100 to-gray-50 rounded-lg border-2 border-gray-200">
              <span className="text-gray-700 font-bold">Kontak (TX2):</span>
              <div className="flex items-center gap-2">
                <span className={`text-lg font-bold ${batteryData.contactSwitch ? 'text-green-600' : 'text-red-600'}`}>
                  {batteryData.contactSwitch ? 'Kapalı' : 'Açık'}
                </span>
                <div className={`w-3 h-3 rounded-full ${batteryData.contactSwitch ? 'bg-green-500 animate-pulse' : 'bg-red-400'}`} />
              </div>
            </div>
          </div>
        </div>
      </div>

      <div className="bg-white rounded-lg shadow-lg p-8">
        <h2 className="text-2xl font-bold text-gray-900 mb-6 flex items-center gap-2">
          <Zap className="text-yellow-600" />
          Akım Bilgileri
        </h2>

        <div className="space-y-6">
          <div className="bg-gradient-to-r from-green-500 to-green-600 rounded-lg p-6 text-white">
            <div className="flex items-center justify-between mb-2">
              <h3 className="font-semibold text-lg opacity-90">Şarj Akımı</h3>
              <Activity size={24} />
            </div>
            <div className="text-4xl font-bold mb-2">{batteryData.chargeCurrent.toFixed(2)} A</div>
            <div className="text-sm opacity-90">{batteryData.chargePower.toFixed(2)} kW</div>
          </div>

          <div className="bg-gradient-to-r from-orange-500 to-orange-600 rounded-lg p-6 text-white">
            <div className="flex items-center justify-between mb-2">
              <h3 className="font-semibold text-lg opacity-90">Deşarj Akımı</h3>
              <Activity size={24} />
            </div>
            <div className="text-4xl font-bold mb-2">{batteryData.dischargeCurrent.toFixed(2)} A</div>
            <div className="text-sm opacity-90">{batteryData.dischargePower.toFixed(2)} kW</div>
          </div>

          <div className="grid grid-cols-2 gap-4">
            <div className="bg-blue-50 rounded-lg p-4">
              <div className="text-xs text-gray-600 font-semibold mb-1">Toplam Enerji Girişi</div>
              <div className="text-2xl font-bold text-blue-600">{batteryData.totalEnergyIn.toFixed(2)}</div>
              <div className="text-xs text-gray-500">kWh</div>
            </div>
            <div className="bg-orange-50 rounded-lg p-4">
              <div className="text-xs text-gray-600 font-semibold mb-1">Toplam Enerji Çıkışı</div>
              <div className="text-2xl font-bold text-orange-600">{batteryData.totalEnergyOut.toFixed(2)}</div>
              <div className="text-xs text-gray-500">kWh</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
