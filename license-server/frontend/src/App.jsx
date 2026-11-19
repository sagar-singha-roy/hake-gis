import React, { useState, useEffect, useCallback } from 'react';
import Header from './components/Header';
import StatCards from './components/StatCards';
import LicensesTab from './components/LicensesTab';
import PaymentsTab from './components/PaymentsTab';
import AuditTab from './components/AuditTab';
import EmailsTab from './components/EmailsTab';
import { CreateLicenseModal, BatchGeneratorModal, SimulateStripeModal, AuthModal } from './components/Modals';
import Toast from './components/Toast';

export default function App() {
  const [activeTab, setActiveTab] = useState('licenses');
  const [licenses, setLicenses] = useState([]);
  const [payments, setPayments] = useState([]);
  const [auditLogs, setAuditLogs] = useState([]);
  const [emails, setEmails] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [authToken, setAuthToken] = useState(() => localStorage.getItem('hake_admin_token') || '');

  // Modals state
  const [modalOpen, setModalOpen] = useState({
    create: false,
    batch: false,
    stripe: false,
    auth: false
  });

  // Toasts state
  const [toasts, setToasts] = useState([]);

  const showToast = useCallback((message, type = 'success') => {
    const id = Date.now() + Math.random();
    setToasts((prev) => [...prev, { id, message, type }]);
    setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
    }, 4000);
  }, []);

  const dismissToast = (id) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  };

  // API Fetch helper
  const apiFetch = useCallback(async (endpoint, options = {}) => {
    options.headers = options.headers || {};
    options.headers['Content-Type'] = 'application/json';
    if (authToken) {
      options.headers['Authorization'] = `Bearer ${authToken}`;
    }

    try {
      const res = await fetch(endpoint, options);
      if (res.status === 401) {
        setModalOpen((prev) => ({ ...prev, auth: true }));
        return res;
      }
      return res;
    } catch (err) {
      console.error('API Error:', err);
      showToast('Network error connecting to license server', 'error');
      throw err;
    }
  }, [authToken, showToast]);

  // Load licenses
  const loadLicenses = useCallback(async () => {
    try {
      const res = await apiFetch('/api/admin/licenses');
      if (res.ok) {
        const data = await res.json();
        setLicenses(data);
      }
    } catch (err) {
      console.error(err);
    }
  }, [apiFetch]);

  // Load payments
  const loadPayments = useCallback(async () => {
    try {
      const res = await apiFetch('/api/admin/payments');
      if (res.ok) {
        const data = await res.json();
        setPayments(data);
      }
    } catch (err) {
      console.error(err);
    }
  }, [apiFetch]);

  // Load audit logs
  const loadAuditLogs = useCallback(async () => {
    try {
      const res = await apiFetch('/api/admin/audit-logs');
      if (res.ok) {
        const data = await res.json();
        setAuditLogs(data);
      }
    } catch (err) {
      console.error(err);
    }
  }, [apiFetch]);

  // Load emails
  const loadEmails = useCallback(async () => {
    try {
      const res = await apiFetch('/api/admin/emails');
      if (res.ok) {
        const data = await res.json();
        setEmails(data);
      }
    } catch (err) {
      console.error(err);
    }
  }, [apiFetch]);

  // Refresh all current data
  const handleRefresh = useCallback(async () => {
    setIsLoading(true);
    await Promise.all([
      loadLicenses(),
      loadPayments(),
      loadAuditLogs(),
      loadEmails()
    ]);
    setIsLoading(false);
    showToast('Dashboard data refreshed');
  }, [loadLicenses, loadPayments, loadAuditLogs, loadEmails, showToast]);

  useEffect(() => {
    handleRefresh();
  }, [authToken]);

  // Actions
  const handleExtend = async (id, days) => {
    try {
      const res = await apiFetch(`/api/admin/licenses/${id}/extend`, {
        method: 'POST',
        body: JSON.stringify({ additional_days: days })
      });
      if (res.ok) {
        showToast(`Extended license by ${days} days`);
        loadLicenses();
        loadAuditLogs();
      }
    } catch (err) {
      showToast('Failed to extend license', 'error');
    }
  };

  const handleResetDevice = async (id) => {
    try {
      const res = await apiFetch(`/api/admin/licenses/${id}/reset-device`, {
        method: 'POST'
      });
      if (res.ok) {
        showToast('Workstation hardware binding reset successfully');
        loadLicenses();
        loadAuditLogs();
      }
    } catch (err) {
      showToast('Failed to reset workstation', 'error');
    }
  };

  const handleRevoke = async (id) => {
    if (!window.confirm('Are you sure you want to permanently revoke this license?')) return;
    try {
      const res = await apiFetch(`/api/admin/licenses/${id}/revoke`, {
        method: 'POST',
        body: JSON.stringify({ reason: 'Staff administrator revocation via React dashboard' })
      });
      if (res.ok) {
        showToast('License revoked successfully', 'error');
        loadLicenses();
        loadAuditLogs();
      }
    } catch (err) {
      showToast('Failed to revoke license', 'error');
    }
  };

  const handleResetTrials = async () => {
    if (!window.confirm('Reset all 7-day trial limits so this workstation can activate another free trial?')) return;
    try {
      const res = await apiFetch('/api/admin/reset-trials', { method: 'POST' });
      if (res.ok) {
        showToast('Workstation trial limits reset! You can now activate a new trial.');
        handleRefresh();
      }
    } catch (err) {
      showToast('Failed to reset trials', 'error');
    }
  };

  const handleCreateLicense = async (email, tier, days) => {
    try {
      const res = await apiFetch('/api/admin/licenses', {
        method: 'POST',
        body: JSON.stringify({ customer_email: email, plan_tier: tier, valid_days: days })
      });
      if (res.ok) {
        const data = await res.json();
        showToast(`Created new ${tier} license for ${email}`);
        loadLicenses();
        loadAuditLogs();
        return data.activation_code;
      } else {
        showToast('Failed to create license', 'error');
      }
    } catch (err) {
      showToast('Error creating license', 'error');
    }
    return null;
  };

  const handleBatchGenerate = async (count, prefix) => {
    try {
      const res = await apiFetch('/api/admin/licenses/batch', {
        method: 'POST',
        body: JSON.stringify({ count, email_prefix: prefix, plan_tier: 'business', valid_days: 365 })
      });
      if (res.ok) {
        const list = await res.json();
        showToast(`Successfully batch-generated ${list.length} licenses!`);
        loadLicenses();
        loadAuditLogs();
        return list;
      }
    } catch (err) {
      showToast('Error generating batch', 'error');
    }
    return null;
  };

  const handleSimulateStripe = async (email, name, tier) => {
    try {
      const res = await apiFetch('/api/admin/simulate-checkout', {
        method: 'POST',
        body: JSON.stringify({ customer_email: email, customer_name: name, plan_tier: tier })
      });
      if (res.ok) {
        const data = await res.json();
        showToast(`Payment processed & code issued to ${email}`);
        handleRefresh();
        return data;
      }
    } catch (err) {
      showToast('Stripe simulation failed', 'error');
    }
    return null;
  };

  const handleAuthSubmit = async (password) => {
    try {
      const res = await fetch('/api/admin/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: 'admin@haketech.com', password })
      });
      const data = await res.json();
      if (data.token) {
        setAuthToken(data.token);
        localStorage.setItem('hake_admin_token', data.token);
        setModalOpen((prev) => ({ ...prev, auth: false }));
        showToast('Authenticated as Administrator');
        return true;
      }
    } catch (err) {
      console.error(err);
    }
    return false;
  };

  const handleCopy = (text) => {
    if (navigator.clipboard) {
      navigator.clipboard.writeText(text);
      showToast(`Copied to clipboard: ${text}`);
    }
  };

  return (
    <div className="dashboard-container">
      <Header
        onOpenCreate={() => setModalOpen((prev) => ({ ...prev, create: true }))}
        onOpenBatch={() => setModalOpen((prev) => ({ ...prev, batch: true }))}
        onOpenStripe={() => setModalOpen((prev) => ({ ...prev, stripe: true }))}
        onResetTrials={handleResetTrials}
        onRefresh={handleRefresh}
        isLoading={isLoading}
      />

      <StatCards licenses={licenses} payments={payments} />

      {/* Tabs Navigation */}
      <div className="tabs-bar">
        <div className="tabs-list">
          <button
            className={`tab-btn ${activeTab === 'licenses' ? 'active' : ''}`}
            onClick={() => setActiveTab('licenses')}
          >
            <span>📜</span> Licenses &amp; Workstations
            <span className="tab-badge">{licenses.length}</span>
          </button>
          <button
            className={`tab-btn ${activeTab === 'payments' ? 'active' : ''}`}
            onClick={() => setActiveTab('payments')}
          >
            <span>💳</span> Payments &amp; Telemetry
            <span className="tab-badge">{payments.length}</span>
          </button>
          <button
            className={`tab-btn ${activeTab === 'audit' ? 'active' : ''}`}
            onClick={() => setActiveTab('audit')}
          >
            <span>🛡️</span> Security Audit Logs
            <span className="tab-badge">{auditLogs.length}</span>
          </button>
          <button
            className={`tab-btn ${activeTab === 'emails' ? 'active' : ''}`}
            onClick={() => setActiveTab('emails')}
          >
            <span>✉️</span> Spooled Emails
            <span className="tab-badge">{emails.length}</span>
          </button>
        </div>
      </div>

      {/* Tab Panels */}
      {activeTab === 'licenses' && (
        <LicensesTab
          licenses={licenses}
          onExtend={handleExtend}
          onResetDevice={handleResetDevice}
          onRevoke={handleRevoke}
          onCopy={handleCopy}
        />
      )}

      {activeTab === 'payments' && (
        <PaymentsTab payments={payments} onCopy={handleCopy} />
      )}

      {activeTab === 'audit' && (
        <AuditTab auditLogs={auditLogs} />
      )}

      {activeTab === 'emails' && (
        <EmailsTab emails={emails} onCopy={handleCopy} />
      )}

      {/* Modals */}
      <CreateLicenseModal
        isOpen={modalOpen.create}
        onClose={() => setModalOpen((prev) => ({ ...prev, create: false }))}
        onSubmit={handleCreateLicense}
        onCopy={handleCopy}
      />

      <BatchGeneratorModal
        isOpen={modalOpen.batch}
        onClose={() => setModalOpen((prev) => ({ ...prev, batch: false }))}
        onSubmit={handleBatchGenerate}
        onCopy={handleCopy}
      />

      <SimulateStripeModal
        isOpen={modalOpen.stripe}
        onClose={() => setModalOpen((prev) => ({ ...prev, stripe: false }))}
        onSubmit={handleSimulateStripe}
        onCopy={handleCopy}
      />

      <AuthModal
        isOpen={modalOpen.auth}
        onSubmit={handleAuthSubmit}
      />

      <Toast toasts={toasts} onDismiss={dismissToast} />
    </div>
  );
}
