import React from 'react';

export default function PaymentsTab({ payments, onCopy }) {
  return (
    <div className="data-card">
      <div className="table-responsive">
        <table>
          <thead>
            <tr>
              <th>Timestamp (UTC)</th>
              <th>Customer</th>
              <th>Plan &amp; Amount</th>
              <th>Payment ID</th>
              <th>Issued Key Last 4</th>
              <th>Status</th>
              <th>Details</th>
            </tr>
          </thead>
          <tbody>
            {!payments || payments.length === 0 ? (
              <tr>
                <td colSpan="7" style={{ textAlign: 'center', padding: '36px', color: 'var(--text-muted)' }}>
                  No payment transactions recorded yet. Click "💳 Simulate Stripe Buy" above to simulate an automated checkout!
                </td>
              </tr>
            ) : (
              payments.map((p, idx) => (
                <tr key={p.payment_id || idx}>
                  <td>
                    <code style={{ fontSize: '11.5px' }}>
                      {p.created_at ? p.created_at.substring(0, 19).replace('T', ' ') : 'Just now'}
                    </code>
                  </td>
                  <td>
                    <strong>{p.customer_email}</strong>
                  </td>
                  <td>
                    <span style={{ fontWeight: 700, color: '#38bdf8' }}>
                      {p.plan_tier ? p.plan_tier.toUpperCase() : 'LICENSE'}
                    </span>{' '}
                    <span style={{ color: '#94a3b8', fontSize: '12px' }}>({p.amount})</span>
                  </td>
                  <td>
                    <code>{p.payment_id}</code>
                  </td>
                  <td>
                    <code
                      style={{ cursor: 'pointer' }}
                      title="Click to copy"
                      onClick={() => onCopy(`••••-••••-••••-${p.code_last4}`)}
                    >
                      ••••-••••-••••-{p.code_last4}
                    </code>
                  </td>
                  <td>
                    <span className={`badge ${p.status === 'active' ? 'badge-active' : 'badge-revoked'}`}>
                      {p.status === 'active' ? 'PAID & ACTIVE' : p.status.toUpperCase()}
                    </span>
                  </td>
                  <td>
                    <small style={{ color: 'var(--text-muted)' }}>{p.details}</small>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
