import { Calendar, TrendingUp, TrendingDown } from 'lucide-react';
import { useState, useEffect } from 'react';

interface EnergyRecord {
  year: number;
  month: number;
  day: number;
  energyIn: number;
  energyOut: number;
}

export default function EnergyHistory() {
  const [records, setRecords] = useState<EnergyRecord[]>([]);
  const [filter, setFilter] = useState<'all' | 'week' | 'month' | 'year'>('all');

  useEffect(() => {
    const loadHistory = async () => {
      try {
        const response = await fetch('/api/energy-history');
        const data = await response.json();
        setRecords(data.records || []);
      } catch (error) {
        console.error('Geçmiş yüklenemedi:', error);
      }
    };

    loadHistory();
    const interval = setInterval(loadHistory, 60000);
    return () => clearInterval(interval);
  }, []);

  const getFilteredRecords = () => {
    if (filter === 'all') return records;
    
    const now = new Date();
    let cutoffDate = new Date();

    switch (filter) {
      case 'week':
        cutoffDate.setDate(now.getDate() - 7);
        break;
      case 'month':
        cutoffDate.setMonth(now.getMonth() - 1);
        break;
      case 'year':
        cutoffDate.setFullYear(now.getFullYear() - 1);
        break;
    }

    return records.filter((record) => {
      const recordDate = new Date(record.year, record.month - 1, record.day);
      return recordDate >= cutoffDate;
    });
  };

  const calculateStats = (data: EnergyRecord[]) => {
    if (data.length === 0) return { totalIn: 0, totalOut: 0, avg: 0 };

    const totalIn = data.reduce((sum, r) => sum + r.energyIn, 0);
    const totalOut = data.reduce((sum, r) => sum + r.energyOut, 0);
    const avg = data.length > 0 ? (totalIn + totalOut) / data.length : 0;

    return { totalIn, totalOut, avg };
  };

  const filteredRecords = getFilteredRecords();
  const stats = calculateStats(filteredRecords);

  const groupByMonth = (data: EnergyRecord[]) => {
    const grouped: { [key: string]: EnergyRecord[] } = {};
    data.forEach((record) => {
      const key = `${record.year}-${record.month}`;
      if (!grouped[key]) grouped[key] = [];
      grouped[key].push(record);
    });
    return grouped;
  };

  const monthlyData = groupByMonth(filteredRecords);

  const formatDate = (year: number, month: number, day: number) => {
    return new Date(year, month - 1, day).toLocaleDateString('tr-TR', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
    });
  };

  return (
    <div className="bg-white rounded-lg shadow-lg p-8 mt-6">
      <h2 className="text-2xl font-bold text-gray-900 mb-6 flex items-center gap-2">
        <Calendar className="text-green-600" />
        Enerji Geçmişi
      </h2>

      <div className="flex gap-2 mb-6">
        {(['all', 'week', 'month', 'year'] as const).map((f) => (
          <button
            key={f}
            onClick={() => setFilter(f)}
            className={`px-4 py-2 rounded-lg font-semibold transition-colors ${
              filter === f
                ? 'bg-blue-600 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
            }`}
          >
            {f === 'all' ? 'Tümü' : f === 'week' ? '1 Hafta' : f === 'month' ? '1 Ay' : '1 Yıl'}
          </button>
        ))}
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-8">
        <div className="bg-green-50 p-6 rounded-lg border-l-4 border-green-500">
          <div className="flex items-center gap-2 mb-2">
            <TrendingUp className="text-green-600" size={20} />
            <span className="text-sm font-semibold text-gray-600">Toplam Şarj</span>
          </div>
          <div className="text-3xl font-bold text-green-600">{stats.totalIn.toFixed(2)} kWh</div>
        </div>

        <div className="bg-orange-50 p-6 rounded-lg border-l-4 border-orange-500">
          <div className="flex items-center gap-2 mb-2">
            <TrendingDown className="text-orange-600" size={20} />
            <span className="text-sm font-semibold text-gray-600">Toplam Deşarj</span>
          </div>
          <div className="text-3xl font-bold text-orange-600">{stats.totalOut.toFixed(2)} kWh</div>
        </div>

        <div className="bg-blue-50 p-6 rounded-lg border-l-4 border-blue-500">
          <div className="flex items-center gap-2 mb-2">
            <Calendar className="text-blue-600" size={20} />
            <span className="text-sm font-semibold text-gray-600">Ortalama/Gün</span>
          </div>
          <div className="text-3xl font-bold text-blue-600">{stats.avg.toFixed(2)} kWh</div>
        </div>
      </div>

      <div className="overflow-x-auto">
        <table className="w-full text-sm">
          <thead className="bg-gray-100 border-b-2 border-gray-300">
            <tr>
              <th className="px-6 py-3 text-left font-semibold text-gray-700">Tarih</th>
              <th className="px-6 py-3 text-right font-semibold text-gray-700">Şarj (kWh)</th>
              <th className="px-6 py-3 text-right font-semibold text-gray-700">Deşarj (kWh)</th>
              <th className="px-6 py-3 text-right font-semibold text-gray-700">Net (kWh)</th>
            </tr>
          </thead>
          <tbody>
            {filteredRecords.length === 0 ? (
              <tr>
                <td colSpan={4} className="px-6 py-4 text-center text-gray-500">
                  Henüz veri yok
                </td>
              </tr>
            ) : (
              filteredRecords.map((record, idx) => (
                <tr
                  key={idx}
                  className={idx % 2 === 0 ? 'bg-white' : 'bg-gray-50'}
                >
                  <td className="px-6 py-4 text-gray-900 font-medium">
                    {formatDate(record.year, record.month, record.day)}
                  </td>
                  <td className="px-6 py-4 text-right text-green-600 font-semibold">
                    {record.energyIn.toFixed(2)}
                  </td>
                  <td className="px-6 py-4 text-right text-orange-600 font-semibold">
                    {record.energyOut.toFixed(2)}
                  </td>
                  <td className="px-6 py-4 text-right font-semibold text-blue-600">
                    {(record.energyIn - record.energyOut).toFixed(2)}
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {Object.keys(monthlyData).length > 0 && (
        <div className="mt-8 pt-8 border-t-2 border-gray-200">
          <h3 className="text-xl font-bold text-gray-900 mb-6">Aylık Özet</h3>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {Object.entries(monthlyData)
              .sort()
              .reverse()
              .map(([key, monthRecords]) => {
                const [year, month] = key.split('-');
                const monthlyIn = monthRecords.reduce((sum, r) => sum + r.energyIn, 0);
                const monthlyOut = monthRecords.reduce((sum, r) => sum + r.energyOut, 0);

                return (
                  <div
                    key={key}
                    className="bg-gradient-to-br from-gray-50 to-gray-100 p-4 rounded-lg border border-gray-200"
                  >
                    <div className="font-semibold text-gray-900 mb-3">
                      {new Date(parseInt(year), parseInt(month) - 1).toLocaleDateString('tr-TR', {
                        month: 'long',
                        year: 'numeric',
                      })}
                    </div>
                    <div className="space-y-2 text-sm">
                      <div className="flex justify-between">
                        <span className="text-gray-600">Şarj:</span>
                        <span className="font-semibold text-green-600">{monthlyIn.toFixed(2)} kWh</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-600">Deşarj:</span>
                        <span className="font-semibold text-orange-600">{monthlyOut.toFixed(2)} kWh</span>
                      </div>
                      <div className="flex justify-between pt-2 border-t border-gray-300">
                        <span className="text-gray-600 font-semibold">Net:</span>
                        <span className="font-bold text-blue-600">{(monthlyIn - monthlyOut).toFixed(2)} kWh</span>
                      </div>
                    </div>
                  </div>
                );
              })}
          </div>
        </div>
      )}
    </div>
  );
}
