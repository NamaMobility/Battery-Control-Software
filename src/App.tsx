import { useState, useEffect, useCallback, useRef } from 'react';
import Header from './components/Header';
import Dashboard from './components/Dashboard';
import Controls from './components/Controls';
import Settings from './components/Settings';
import ServiceModal from './components/ServiceModal';

type SystemState =
  | 'IDLE'
  | 'STARTUP'
  | 'PRECHARGE'
  | 'MOSFET_ACTIVE'
  | 'DISCHARGE_ACTIVE'
  | 'RUNNING'
  | 'BYPASS_MODE'
  | 'FAULT'
  | 'EMERGENCY_STOP';

type LEDMode = 'NORMAL' | 'EMERGENCY' | 'CONTACT_CLOSED' | 'BYPASS' | 'OFF';

interface BatteryData {
  soc: number;
  voltage: number;
  temperature: number;
  capacity: number;
  cycleCount: number;
  chargeCurrent: number;
  chargePower: number;
  dischargeCurrent: number;
  dischargePower: number;
  totalEnergyIn: number;
  totalEnergyOut: number;
  chargeRelay: boolean;
  dischargeRelay: boolean;
  prechargeRelay: boolean;
  mosfet1: boolean;
  mosfet2: boolean;
  bmsSignal: boolean;
  contactSwitch: boolean;
  bypassMode: boolean;
  emergencyStop: boolean;
  systemState: SystemState;
  ledMode: LEDMode;
  ledOn: boolean;
  faultMessage: string;
  mac: string;
}

function App() {
  const [batteryData, setBatteryData] = useState<BatteryData>({
    soc: 0,
    voltage: 0,
    temperature: 0,
    capacity: 0,
    cycleCount: 0,
    chargeCurrent: 0,
    chargePower: 0,
    dischargeCurrent: 0,
    dischargePower: 0,
    totalEnergyIn: 0,
    totalEnergyOut: 0,
    chargeRelay: false,
    dischargeRelay: false,
    prechargeRelay: false,
    mosfet1: false,
    mosfet2: false,
    bmsSignal: false,
    contactSwitch: false,
    bypassMode: false,
    emergencyStop: false,
    systemState: 'IDLE',
    ledMode: 'OFF',
    ledOn: false,
    faultMessage: '',
    mac: '',
  });

  const [showServiceModal, setShowServiceModal] = useState(false);
  const statusRequestInFlight = useRef(false);

  const fetchStatus = useCallback(async () => {
    if (statusRequestInFlight.current) return;
    statusRequestInFlight.current = true;
    try {
      const response = await fetch('/api/status');
      const data = await response.json();
      setBatteryData(data);
    } catch (error) {
      console.error('Veri güncellemesi başarısız:', error);
    } finally {
      statusRequestInFlight.current = false;
    }
  }, []);

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 250);
    return () => clearInterval(interval);
  }, [fetchStatus]);

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-900 via-blue-800 to-blue-900">
      <Header batteryData={batteryData} />

      <div className="container mx-auto px-4 py-8">
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-6">
          <div className="lg:col-span-2">
            <Dashboard batteryData={batteryData} />
          </div>
          <div>
            <Controls batteryData={batteryData} onActionComplete={fetchStatus} />
          </div>
        </div>

        <div className="grid grid-cols-1 gap-6">
          <Settings onServiceClick={() => setShowServiceModal(true)} batteryData={batteryData} />
        </div>
      </div>

      {showServiceModal && (
        <ServiceModal onClose={() => setShowServiceModal(false)} />
      )}

      <footer className="py-4 text-center text-sm text-blue-100/90">
        nama mobility 2026. v1.151
      </footer>
    </div>
  );
}

export default App;
