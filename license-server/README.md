# Hake GeoDesk Licensing Server

Enterprise licensing, subscription, and workstation activation service for **Hake GeoDesk Desktop GIS**.

## Overview
This service provides:
- **Phase 1**: FastAPI & high-concurrency standard library server, SQLite & PostgreSQL database engines, Crockford Base32 16-character code generator (`XXXX-XXXX-XXXX-XXXX`), and pure-Python RFC 8032 Ed25519 digital token signing.
- **Phase 2**: 7-day server-managed free trial engine, workstation hardware binding, and single-device seat enforcement.
- **Phase 3**: Daily periodic check-in (`POST /api/refresh`), 14-day offline grace period logic, and client-initiated workstation deactivation & migration (`POST /api/deactivate`).
- **Phase 4**: Stripe and Razorpay webhook processing (HMAC-SHA256 signature verification), transactional HTML email delivery, and instant refund revocation gate.

## Quick Start (Local Run)

### 1. Run the Server
```bash
python3 run_server.py
```
- Server API: `http://127.0.0.1:8000`
- Web Admin Dashboard: `http://127.0.0.1:8000/admin`
  - Default Staff Login: `admin@haketech.com` / `HakeAdmin2026!`

### 2. Run Automated Test Suites
```bash
# Phase 1 & 2 test suite
python3 tests/test_phase1.py

# Phase 3 & 4 test suite
python3 tests/test_phase3_phase4.py
```

### 3. Run End-to-End Complete Demonstration
```bash
python3 tests/demo_full_lifecycle.py
```

## API Endpoints

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/health` | Service health status |
| `GET` | `/api/public-key` | Server Ed25519 public key in hex |
| `POST` | `/api/auth/register` | User account registration |
| `POST` | `/api/auth/login` | User authentication |
| `POST` | `/api/trial` | Start 7-day free trial bound to workstation |
| `POST` | `/api/activate` | Activate 16-character license code |
| `POST` | `/api/refresh` | Daily check-in & token refresh |
| `POST` | `/api/deactivate` | Deactivate workstation to allow transfer |
| `POST` | `/api/webhooks/stripe` | Stripe webhook receiver (HMAC-SHA256) |
| `POST` | `/api/webhooks/razorpay` | Razorpay webhook receiver (HMAC-SHA256) |
| `POST` | `/api/checkout/simulate` | In-app purchase simulation & code delivery |
| `GET` | `/admin` | Staff administration dashboard |
| `GET` | `/api/admin/licenses` | List all licenses and active devices |
| `POST` | `/api/admin/licenses` | Create new single license |
| `POST` | `/api/admin/licenses/batch` | Batch generate multiple license codes |
| `POST` | `/api/admin/licenses/{id}/extend` | Extend license validity by $N$ days |
| `POST` | `/api/admin/licenses/{id}/reset-device` | Unbind active device seats |
| `POST` | `/api/admin/licenses/{id}/revoke` | Revoke license immediately |
| `GET` | `/api/admin/audit-logs` | Retrieve immutable security audit trail |
| `GET` | `/api/admin/emails` | List spooled transactional emails |
