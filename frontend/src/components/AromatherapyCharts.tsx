import {
  Bar,
  BarChart,
  Cell,
  Pie,
  PieChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import { useCommandStore } from '@/store/commandStore';

const SCENT_COLORS: Record<string, string> = {
  lemon: '#FFD700',
  lavender: '#E6E6FA',
  woody: '#8B4513',
  floral: '#FFB6C1',
};

export default function AromatherapyCharts() {
  const deviceStatus = useCommandStore((state) => state.deviceStatus);

  const intensity = deviceStatus?.current_intensity ?? 0;
  const intensityData = [{ metric: 'Intensity', value: intensity }];

  const controlParams = deviceStatus?.control_params;
  const ratios = (controlParams && 'mixing_ratios' in controlParams ? controlParams.mixing_ratios : null) ?? {};

  const ratioData = Object.entries(ratios)
    .filter((entry): entry is [string, number] => typeof entry[1] === 'number' && entry[1] > 0)
    .map(([name, value]) => ({
      name,
      value,
      color: SCENT_COLORS[name] ?? '#9AA7B5',
    }));

  const duration = (controlParams && 'duration_minutes' in controlParams ? controlParams.duration_minutes : 0) ?? 0;
  const remaining = deviceStatus?.remaining_minutes ?? 0;
  const progressPercent =
    duration > 0 ? Math.max(0, Math.min(100, ((duration - remaining) / duration) * 100)) : 0;

  const rhythm = (controlParams && 'release_rhythm' in controlParams ? controlParams.release_rhythm : 'gradual') ?? 'gradual';

  return (
    <div className="charts-wrap">
      <div className="chart-card">
        <h3>Intensity</h3>
        <div className="chart-box">
          <ResponsiveContainer width="100%" height="100%">
            <BarChart data={intensityData}>
              <XAxis dataKey="metric" tick={{ fontSize: 11 }} />
              <YAxis domain={[0, 10]} tick={{ fontSize: 11 }} />
              <Tooltip />
              <Bar dataKey="value" fill="#1f9d84" radius={[6, 6, 0, 0]} />
            </BarChart>
          </ResponsiveContainer>
        </div>
      </div>

      <div className="chart-card">
        <h3>Mixing Ratios</h3>
        <div className="chart-box">
          {ratioData.length === 0 ? (
            <p className="muted">No mixing ratio data yet.</p>
          ) : (
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie data={ratioData} dataKey="value" nameKey="name" outerRadius={62} innerRadius={30}>
                  {ratioData.map((item) => (
                    <Cell key={item.name} fill={item.color} />
                  ))}
                </Pie>
                <Tooltip />
              </PieChart>
            </ResponsiveContainer>
          )}
        </div>
      </div>

      <div className="chart-card">
        <h3>Duration Progress</h3>
        <div className="duration-progress-track">
          <span style={{ width: `${progressPercent}%` }} />
        </div>
        <p className="muted">
          {duration > 0 ? `${Math.round(progressPercent)}% completed` : 'No active duration data.'}
        </p>
      </div>

      <div className="chart-card">
        <h3>Release Rhythm</h3>
        <div className={`rhythm-pill rhythm-${rhythm}`}>{rhythm}</div>
      </div>
    </div>
  );
}
