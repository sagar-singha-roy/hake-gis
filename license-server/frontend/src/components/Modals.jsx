import React, { useState } from 'react';

export function CreateLicenseModal({ isOpen, onClose, onSubmit, onCopy }) {
  const [email, setEmail] = useState('');
  const [tier, setTier] = useState('annual');
  const [days, setDays] = useState(365);
  const [generatedCode, setGeneratedCode] = useState(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!email) return;
    setIsSubmitting(true);
    const code = await onSubmit(email, tier, parseInt(days) || 365);
    setIsSubmitting(false);
    if (code) {
      setGeneratedCode(code);
    }
  };

  const handleClose = () => {
    setGeneratedCode(null);
    setEmail('');
    onClose();
  };

  return (
    <div className="modal-overlay" onClick={handleClose}>
      <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
        <div className="modal-head">
          <h3>Create New License</h3>
          <button className="modal-close" onClick={handleClose}>✕</button>
        </div>
        <div className="modal-body">
          {generatedCode ? (
            <div>
              <p style={{ fontSize: '13px', color: 'var(--text-muted)', marginBottom: '8px' }}>
                New 16-character Crockford Base32 Code (Store safely; server stores only SHA-256 hash):
              </p>
              <div className="code-box">{generatedCode}</div>
              <div style={{ display: 'flex', gap: '10px' }}>
                <button
                  className="btn btn-secondary"
                  style={{ flex: 1 }}
                  onClick={() => onCopy(generatedCode)}
                >
                  📋 Copy Code
                </button>
                <button className="btn" style={{ flex: 1 }} onClick={handleClose}>
                  Done
                </button>
              </div>
            </div>
          ) : (
            <form onSubmit={handleSubmit}>
              <div className="form-field">
                <label>Customer Licensed Email</label>
                <input
                  type="email"
                  placeholder="analyst@geospatial.com"
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  required
                  autoFocus
                />
              </div>
              <div className="form-field">
                <label>Plan Tier</label>
                <select value={tier} onChange={(e) => setTier(e.target.value)}>
                  <option value="annual">Annual Plan (365 Days, 1 Workstation)</option>
                  <option value="business">Business Plan (365 Days, 5 Seats)</option>
                  <option value="trial">7-Day Free Trial</option>
                  <option value="starter">Starter Plan (Community Free)</option>
                </select>
              </div>
              <div className="form-field">
                <label>Validity Period (Days)</label>
                <input
                  type="number"
                  min="1"
                  max="3650"
                  value={days}
                  onChange={(e) => setDays(e.target.value)}
                />
              </div>
              <button className="btn" style={{ width: '100%', marginTop: '8px' }} disabled={isSubmitting}>
                {isSubmitting ? 'Generating...' : 'Generate Activation Code'}
              </button>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}

export function BatchGeneratorModal({ isOpen, onClose, onSubmit, onCopy }) {
  const [count, setCount] = useState(5);
  const [prefix, setPrefix] = useState('enterprise-seat');
  const [batchResults, setBatchResults] = useState(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    setIsSubmitting(true);
    const results = await onSubmit(parseInt(count) || 5, prefix);
    setIsSubmitting(false);
    if (results) {
      setBatchResults(results);
    }
  };

  const handleDownloadCsv = () => {
    if (!batchResults || batchResults.length === 0) return;
    const csvContent =
      'Email,ActivationCode,PlanTier,ValidDays\n' +
      batchResults.map((b) => `${b.customer_email},${b.activation_code},business,365`).join('\n');
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.setAttribute('download', `hake_geodesk_batch_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const handleClose = () => {
    setBatchResults(null);
    onClose();
  };

  return (
    <div className="modal-overlay" onClick={handleClose}>
      <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
        <div className="modal-head">
          <h3>Batch License Key Generator</h3>
          <button className="modal-close" onClick={handleClose}>✕</button>
        </div>
        <div className="modal-body">
          {batchResults ? (
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>
                <span style={{ fontSize: '13px', color: 'var(--text-muted)' }}>
                  Generated {batchResults.length} Enterprise Keys:
                </span>
                <button className="btn btn-success btn-sm" onClick={handleDownloadCsv}>
                  📥 Download CSV
                </button>
              </div>
              <div
                style={{
                  maxHeight: '220px',
                  overflowY: 'auto',
                  background: '#090d16',
                  borderRadius: '8px',
                  padding: '12px',
                  marginBottom: '16px',
                  border: '1px solid var(--card-border)'
                }}
              >
                {batchResults.map((item, idx) => (
                  <div
                    key={idx}
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      fontSize: '12.5px',
                      padding: '4px 0',
                      borderBottom: '1px solid rgba(255,255,255,0.05)'
                    }}
                  >
                    <span style={{ color: 'var(--text-muted)' }}>{item.customer_email}</span>
                    <code
                      style={{ cursor: 'pointer' }}
                      title="Click to copy"
                      onClick={() => onCopy(item.activation_code)}
                    >
                      {item.activation_code}
                    </code>
                  </div>
                ))}
              </div>
              <button className="btn" style={{ width: '100%' }} onClick={handleClose}>
                Done
              </button>
            </div>
          ) : (
            <form onSubmit={handleSubmit}>
              <div className="form-field">
                <label>Number of Codes to Issue (1 - 50)</label>
                <input
                  type="number"
                  min="1"
                  max="50"
                  value={count}
                  onChange={(e) => setCount(e.target.value)}
                  required
                />
              </div>
              <div className="form-field">
                <label>Customer Identifier / Email Prefix</label>
                <input
                  type="text"
                  placeholder="enterprise-seat"
                  value={prefix}
                  onChange={(e) => setPrefix(e.target.value)}
                  required
                />
              </div>
              <button className="btn" style={{ width: '100%', marginTop: '8px' }} disabled={isSubmitting}>
                {isSubmitting ? 'Generating Batch...' : 'Generate Batch Codes'}
              </button>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}

export function SimulateStripeModal({ isOpen, onClose, onSubmit, onCopy }) {
  const [email, setEmail] = useState('analyst@geodesk.org');
  const [name, setName] = useState('Alex Rivera');
  const [tier, setTier] = useState('annual');
  const [result, setResult] = useState(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    setIsSubmitting(true);
    const data = await onSubmit(email, name, tier);
    setIsSubmitting(false);
    if (data) {
      setResult(data);
    }
  };

  const handleClose = () => {
    setResult(null);
    onClose();
  };

  return (
    <div className="modal-overlay" onClick={handleClose}>
      <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
        <div className="modal-head">
          <h3>💳 Simulate Stripe Checkout</h3>
          <button className="modal-close" onClick={handleClose}>✕</button>
        </div>
        <div className="modal-body">
          {result ? (
            <div>
              <div
                style={{
                  background: 'rgba(16, 185, 129, 0.15)',
                  border: '1px solid var(--success)',
                  padding: '14px',
                  borderRadius: '10px',
                  marginBottom: '16px'
                }}
              >
                <div style={{ color: '#34d399', fontWeight: 700, marginBottom: '6px' }}>
                  ✓ Webhook Processed &amp; License Issued!
                </div>
                <div className="code-box" style={{ margin: '8px 0' }}>
                  {result.activation_code}
                </div>
                <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
                  Delivery receipt spooled to <strong>{result.customer_email}</strong>. Workstation can now activate this key.
                </div>
              </div>
              <div style={{ display: 'flex', gap: '10px' }}>
                <button
                  className="btn btn-secondary"
                  style={{ flex: 1 }}
                  onClick={() => onCopy(result.activation_code)}
                >
                  📋 Copy Code
                </button>
                <button className="btn" style={{ flex: 1 }} onClick={handleClose}>
                  Done
                </button>
              </div>
            </div>
          ) : (
            <form onSubmit={handleSubmit}>
              <div className="form-field">
                <label>Customer Email</label>
                <input
                  type="email"
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  required
                />
              </div>
              <div className="form-field">
                <label>Customer Full Name</label>
                <input
                  type="text"
                  value={name}
                  onChange={(e) => setName(e.target.value)}
                  required
                />
              </div>
              <div className="form-field">
                <label>Plan Tier</label>
                <select value={tier} onChange={(e) => setTier(e.target.value)}>
                  <option value="annual">Annual Pro Plan ($249 - 1 Seat)</option>
                  <option value="business">Business Team Plan ($799 - 5 Seats)</option>
                </select>
              </div>
              <button
                className="btn btn-success"
                style={{ width: '100%', marginTop: '8px' }}
                disabled={isSubmitting}
              >
                {isSubmitting ? 'Simulating...' : 'Simulate Payment & Trigger Webhook'}
              </button>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}

export function AuthModal({ isOpen, onSubmit }) {
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!password) return;
    setIsSubmitting(true);
    setError('');
    const success = await onSubmit(password);
    setIsSubmitting(false);
    if (!success) {
      setError('Invalid staff credentials. Default: HakeAdmin2026! or admin');
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-dialog">
        <div className="modal-head">
          <h3>🛡️ Staff Administrator Authentication</h3>
        </div>
        <div className="modal-body">
          <p style={{ fontSize: '13px', color: 'var(--text-muted)', marginBottom: '14px' }}>
            Please enter the staff administrator password to access the Hake GeoDesk Licensing Dashboard:
          </p>
          {error && (
            <div style={{ color: '#f87171', fontSize: '12px', marginBottom: '12px' }}>
              ⚠️ {error}
            </div>
          )}
          <form onSubmit={handleSubmit}>
            <div className="form-field">
              <label>Administrator Password</label>
              <input
                type="password"
                placeholder="Enter admin password..."
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                autoFocus
                required
              />
            </div>
            <button className="btn" style={{ width: '100%' }} disabled={isSubmitting}>
              {isSubmitting ? 'Authenticating...' : 'Sign In as Administrator'}
            </button>
          </form>
        </div>
      </div>
    </div>
  );
}
