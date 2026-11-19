import React from 'react';

export default function AuditTab({ auditLogs }) {
  const getEventBadge = (type) => {
    if (type.includes('revoked') || type.includes('tamper')) {
      return <span className="badge badge-revoked">{type}</span>;
    }
    if (type.includes('created') || type.includes('batch')) {
      return <span className="badge badge-active">{type}</span>;
    }
    if (type.includes('extended') || type.includes('reset')) {
      return <span className="badge badge-trial">{type}</span>;
    }
    return <span className="badge" style={{ background: '#334155', color: '#f1f5f9' }}>{type}</span>;
  };

  return (
    <div className="data-card">
      <div className="table-responsive">
        <table>
          <thead>
            <tr>
              <th>Timestamp (UTC)</th>
              <th>Security Event</th>
              <th>Details &amp; Audit Log</th>
              <th>Origin IP</th>
            </tr>
          </thead>
          <tbody>
            {!auditLogs || auditLogs.length === 0 ? (
              <tr>
                <td colSpan="4" style={{ textAlign: 'center', padding: '36px', color: 'var(--text-muted)' }}>
                  No audit logs recorded yet.
                </td>
              </tr>
            ) : (
              auditLogs.map((log, idx) => (
                <tr key={idx}>
                  <td style={{ whiteSpace: 'nowrap' }}>
                    <code style={{ fontSize: '11.5px' }}>{log.timestamp}</code>
                  </td>
                  <td>{getEventBadge(log.event_type)}</td>
                  <td style={{ color: '#e2e8f0' }}>{log.details}</td>
                  <td>
                    <code style={{ color: 'var(--text-muted)' }}>{log.ip_address || '127.0.0.1'}</code>
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
