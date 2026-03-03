import { X, Lock } from 'lucide-react';
import { useState } from 'react';

interface ServiceModalProps {
  onClose: () => void;
}

export default function ServiceModal({ onClose }: ServiceModalProps) {
  const [authenticated, setAuthenticated] = useState(false);
  const [password, setPassword] = useState('');
  const [socReference, setSocReference] = useState('1.0');
  const [error, setError] = useState('');
  const [success, setSuccess] = useState(false);

  const handleAuthenticate = () => {
    if (password === 'nama2026') {
      setAuthenticated(true);
      setError('');
    } else {
      setError('Yanlış şifre!');
    }
  };

  const handleSave = async () => {
    try {
      await fetch('/api/service', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'X-Service-Password': 'nama2026',
        },
        body: JSON.stringify({
          socReference: parseFloat(socReference),
        }),
      });

      setSuccess(true);
      setTimeout(() => {
        onClose();
        setSuccess(false);
      }, 2000);
    } catch (err) {
      setError('Kaydetme hatası!');
    }
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <div className="bg-white rounded-lg shadow-2xl max-w-md w-full mx-4">
        <div className="flex items-center justify-between p-6 border-b border-gray-200">
          <h2 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Lock className="text-orange-600" size={24} />
            Servis Modu
          </h2>
          <button
            onClick={onClose}
            className="text-gray-500 hover:text-gray-700 transition-colors"
          >
            <X size={24} />
          </button>
        </div>

        <div className="p-6 space-y-4">
          {!authenticated ? (
            <>
              <p className="text-sm text-gray-600 mb-4">Servis moduna erişmek için şifre girin:</p>
              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">Servis Şifresi</label>
                <input
                  type="password"
                  value={password}
                  onChange={(e) => {
                    setPassword(e.target.value);
                    setError('');
                  }}
                  onKeyPress={(e) => e.key === 'Enter' && handleAuthenticate()}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                  placeholder="Şifreyi girin"
                  autoFocus
                />
              </div>

              {error && (
                <div className="bg-red-50 border-l-4 border-red-500 p-3 rounded">
                  <p className="text-red-700 font-semibold text-sm">{error}</p>
                </div>
              )}

              <button
                onClick={handleAuthenticate}
                className="w-full bg-orange-600 hover:bg-orange-700 text-white font-bold py-2 px-4 rounded-lg transition-colors"
              >
                Giriş Yap
              </button>
            </>
          ) : (
            <>
              <div className="bg-green-50 border-l-4 border-green-500 p-3 rounded mb-4">
                <p className="text-green-700 font-semibold text-sm">Kimlik doğrulandı</p>
              </div>

              <div>
                <label className="block text-sm font-semibold text-gray-700 mb-2">SOC Referans Değeri (HASS400S)</label>
                <input
                  type="number"
                  step="0.01"
                  value={socReference}
                  onChange={(e) => setSocReference(e.target.value)}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                  placeholder="Akım sensör kalibrasyonu"
                />
                <p className="text-xs text-gray-500 mt-1">
                  HASS400S sensörünün referans voltaj değerini girin (genellikle 1.0-1.2)
                </p>
              </div>

              {success && (
                <div className="bg-green-50 border-l-4 border-green-500 p-3 rounded">
                  <p className="text-green-700 font-semibold text-sm">Ayarlar kaydedildi!</p>
                </div>
              )}

              <div className="flex gap-2">
                <button
                  onClick={handleSave}
                  className="flex-1 bg-blue-600 hover:bg-blue-700 text-white font-bold py-2 px-4 rounded-lg transition-colors"
                >
                  Kaydet
                </button>
                <button
                  onClick={onClose}
                  className="flex-1 bg-gray-600 hover:bg-gray-700 text-white font-bold py-2 px-4 rounded-lg transition-colors"
                >
                  Kapat
                </button>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
