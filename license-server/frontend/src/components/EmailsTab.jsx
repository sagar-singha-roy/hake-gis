import React from 'react';

export default function EmailsTab({ emails, onCopy }) {
  return (
    <div className="data-card">
      <div className="table-responsive">
        <table>
          <thead>
            <tr>
              <th>Sent At (UTC)</th>
              <th>Recipient</th>
              <th>Plan</th>
              <th>Activation Code</th>
              <th>Delivery Status</th>
            </tr>
          </thead>
          <tbody>
            {!emails || emails.length === 0 ? (
              <tr>
                <td colSpan="5" style={{ textAlign: 'center', padding: '36px', color: 'var(--text-muted)' }}>
                  No transactional emails spooled yet.
                </td>
              </tr>
            ) : (
              emails.map((e, idx) => (
                <tr key={idx}>
                  <td style={{ whiteSpace: 'nowrap' }}>
                    <code style={{ fontSize: '11.5px' }}>
                      {e.sent_at ? e.sent_at.substring(0, 19).replace('T', ' ') : 'Just now'}
                    </code>
                  </td>
                  <td>
                    <strong>{e.customer_name}</strong>
                    <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>&lt;{e.to_email}&gt;</div>
                  </td>
                  <td>
                    <span className="badge badge-active">{e.plan_tier}</span>
                  </td>
                  <td>
                    <code
                      style={{ cursor: 'pointer', letterSpacing: '1px' }}
                      title="Click to copy full code"
                      onClick={() => onCopy(e.activation_code)}
                    >
                      {e.activation_code}
                    </code>
                  </td>
                  <td>
                    <span style={{ color: '#10b981', fontWeight: 600, fontSize: '12px' }}>
                      ✓ {e.relay_status}
                    </span>
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
