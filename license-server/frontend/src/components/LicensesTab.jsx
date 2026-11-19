import React, { useState, useMemo } from 'react';

export default function LicensesTab({
  licenses,
  onExtend,
  onResetDevice,
  onRevoke,
  onCopy
}) {
  const [search, setSearch] = useState('');
  const [tierFilter, setTierFilter] = useState('all');
  const [statusFilter, setStatusFilter] = useState('all');

  const filtered = useMemo(() => {
    return licenses.filter(l => {
      const matchesSearch =
        l.customer_email.toLowerCase().includes(search.toLowerCase()) ||
        (l.code_last4 && l.code_last4.toLowerCase().includes(search.toLowerCase()));

      const matchesTier = tierFilter === 'all' || l.plan_tier === tierFilter;
      const matchesStatus = statusFilter === 'all' || l.status === statusFilter;

      return matchesSearch && matchesTier && matchesStatus;
    });
  }, [licenses, search, tierFilter, statusFilter]);

  const getTierBadge = (tier) => {
    switch (tier) {
      case 'trial': return <span className="badge badge-trial">Trial (7d)</span>;
      case 'business': return <span className="badge badge-business">Business (5 Seat)</span>;
      case 'annual': return <span className="badge badge-active">Annual Pro</span>;
      case 'starter': return <span className="badge" style={{ background: '#0284c7', color: '#fff' }}>Starter</span>;
      default: return <span className="badge badge-active">{tier}</span>;
    }
  };

  const getStatusBadge = (status) => {
    switch (status) {
      case 'active': return <span className="badge badge-active">Active</span>;
      case 'revoked': return <span className="badge badge-revoked">Revoked</span>;
      case 'expired': return <span className="badge badge-expired">Expired</span>;
      default: return <span className="badge">{status}</span>;
    }
  };

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '14px', flexWrap: 'wrap', gap: '10px' }}>
        <div style={{ display: 'flex', gap: '10px', alignItems: 'center', flexWrap: 'wrap' }}>
          <input
            type="text"
            className="search-input"
            placeholder="Search email or code last 4..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
          <select
            className="filter-select"
            value={tierFilter}
            onChange={(e) => setTierFilter(e.target.value)}
          >
            <option value="all">All Plan Tiers</option>
            <option value="annual">Annual</option>
            <option value="business">Business</option>
            <option value="trial">Trial</option>
            <option value="starter">Starter</option>
          </select>
          <select
            className="filter-select"
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value)}
          >
            <option value="all">All Statuses</option>
            <option value="active">Active Only</option>
            <option value="expired">Expired</option>
            <option value="revoked">Revoked</option>
          </select>
        </div>
        <div style={{ fontSize: '12.5px', color: 'var(--text-muted)' }}>
          Showing <strong>{filtered.length}</strong> of <strong>{licenses.length}</strong> licenses
        </div>
      </div>

      <div className="data-card">
        <div className="table-responsive">
          <table>
            <thead>
              <tr>
                <th>Customer Email</th>
                <th>Plan Tier</th>
                <th>Code Key</th>
                <th>Workstations</th>
                <th>Expires (UTC)</th>
                <th>Status</th>
                <th style={{ textAlign: 'right' }}>Actions</th>
              </tr>
            </thead>
            <tbody>
              {filtered.length === 0 ? (
                <tr>
                  <td colSpan="7" style={{ textAlign: 'center', padding: '36px', color: 'var(--text-muted)' }}>
                    No licenses match the selected criteria.
                  </td>
                </tr>
              ) : (
                filtered.map((l) => (
                  <tr key={l.license_id}>
                    <td>
                      <div style={{ fontWeight: 600, color: '#fff' }}>{l.customer_email}</div>
                      <div style={{ fontSize: '11px', color: 'var(--text-dim)', fontFamily: 'monospace' }}>
                        ID: {l.license_id}
                      </div>
                    </td>
                    <td>{getTierBadge(l.plan_tier)}</td>
                    <td>
                      <code
                        style={{ cursor: 'pointer', title: 'Click to copy' }}
                        onClick={() => onCopy(`••••-••••-••••-${l.code_last4}`)}
                      >
                        ••••-{l.code_last4}
                      </code>
                    </td>
                    <td>
                      <span style={{ fontWeight: 600, color: (l.active_devices || 0) > 0 ? '#10b981' : 'var(--text-muted)' }}>
                        {l.active_devices || 0}
                      </span>
                      <span style={{ color: 'var(--text-dim)' }}> / {l.max_devices}</span>
                    </td>
                    <td>{l.expires_at ? l.expires_at.substring(0, 10) : 'Permanent'}</td>
                    <td>{getStatusBadge(l.status)}</td>
                    <td style={{ textAlign: 'right' }}>
                      <div style={{ display: 'inline-flex', gap: '6px' }}>
                        <button
                          className="btn btn-secondary btn-sm"
                          title="Extend license validity by 30 days"
                          onClick={() => onExtend(l.license_id, 30)}
                        >
                          +30d
                        </button>
                        <button
                          className="btn btn-success btn-sm"
                          title="Unbind active workstation so user can move machine"
                          onClick={() => onResetDevice(l.license_id)}
                        >
                          Reset Device
                        </button>
                        {l.status === 'active' && (
                          <button
                            className="btn btn-danger btn-sm"
                            title="Revoke license immediately"
                            onClick={() => onRevoke(l.license_id)}
                          >
                            Revoke
                          </button>
                        )}
                      </div>
                    </td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
