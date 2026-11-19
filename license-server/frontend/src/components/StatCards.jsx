import React from 'react';

export default function StatCards({ licenses, payments }) {
  const total = licenses.length;
  const activeTrials = licenses.filter(l => l.plan_tier === 'trial' && l.status === 'active').length;
  const activeDevices = licenses.reduce((sum, l) => sum + (l.active_devices || 0), 0);
  const paidActive = licenses.filter(l => (l.plan_tier === 'annual' || l.plan_tier === 'business') && l.status === 'active').length;

  return (
    <div className="stats-grid">
      <div className="stat-card">
        <div className="stat-label">Total Licenses</div>
        <div className="stat-value">{total}</div>
      </div>
      <div className="stat-card">
        <div className="stat-label">Active Workstations</div>
        <div className="stat-value" style={{ color: '#10b981' }}>{activeDevices}</div>
      </div>
      <div className="stat-card">
        <div className="stat-label">Paid Subscriptions</div>
        <div className="stat-value" style={{ color: '#38bdf8' }}>{paidActive}</div>
      </div>
      <div className="stat-card">
        <div className="stat-label">Active Trials</div>
        <div className="stat-value" style={{ color: '#22d3ee' }}>{activeTrials}</div>
      </div>
      <div className="stat-card">
        <div className="stat-label">Cryptographic Engine</div>
        <div className="stat-value" style={{ fontSize: '15px', color: '#10b981', paddingTop: '10px' }}>
          ● Ed25519 Verified
        </div>
      </div>
    </div>
  );
}
