import React from 'react';

export default function Header({
  onOpenCreate,
  onOpenBatch,
  onOpenStripe,
  onResetTrials,
  onRefresh,
  isLoading
}) {
  return (
    <header className="header">
      <div className="brand-section">
        <div className="brand-icon">H</div>
        <div className="brand-titles">
          <h1>
            Hake GeoDesk
            <span className="badge-react">⚛️ React Admin</span>
          </h1>
          <div className="brand-sub">Enterprise Cryptographic License &amp; Fleet Workstation Console</div>
        </div>
      </div>
      <div className="header-actions">
        <button className="btn" onClick={onOpenCreate}>
          <span>+</span> Create License
        </button>
        <button className="btn btn-secondary" onClick={onOpenBatch}>
          <span>📋</span> Batch Generator
        </button>
        <button className="btn btn-success" onClick={onOpenStripe}>
          <span>💳</span> Simulate Stripe Buy
        </button>
        <button className="btn btn-secondary" style={{ color: '#94a3b8' }} onClick={onResetTrials}>
          <span>🔄</span> Reset Trials
        </button>
        <button className="btn btn-secondary" onClick={onRefresh} disabled={isLoading}>
          <span style={{ display: 'inline-block', transform: isLoading ? 'rotate(360deg)' : 'none', transition: 'transform 0.5s' }}>↻</span>
          {isLoading ? ' Refreshing...' : ' Refresh'}
        </button>
      </div>
    </header>
  );
}
