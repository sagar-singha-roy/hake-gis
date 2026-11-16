"""
Core Licensing Service for Hake GeoDesk.
Enforces 7-day trial issuance rules, Crockford code activation, 1-device limits,
signed Ed25519 token generation, and administrative lifecycle management.
"""

import uuid
from datetime import datetime, timedelta, timezone
from typing import Dict, Any, List, Optional

from app.database import get_db
from app.crockford import (
    generate_license_code,
    normalize_license_code,
    hash_code,
    get_code_last4,
    format_code,
)
from app.crypto import create_signed_token
from app.auth import hash_password, verify_password

def now_iso() -> str:
    """Current UTC timestamp in ISO 8601 format."""
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

def log_audit(license_id: Optional[str], event_type: str, details: str, ip_address: Optional[str] = None):
    """Record an audit trail event."""
    log_id = str(uuid.uuid4())
    ts = now_iso()
    with get_db() as conn:
        conn.execute(
            """INSERT INTO audit_log (log_id, license_id, event_type, details, ip_address, timestamp)
               VALUES (?, ?, ?, ?, ?, ?)""",
            (log_id, license_id, event_type, details, ip_address or "127.0.0.1", ts)
        )

# =========================================================================
# User Account Authentication
# =========================================================================

def register_user(full_name: str, email: str, password: str, ip_address: Optional[str] = None) -> Dict[str, Any]:
    full_name = full_name.strip()
    email = email.strip().lower()

    if not full_name:
        raise ValueError("Full name is required.")
    if not email or "@" not in email:
        raise ValueError("A valid email address is required.")
    if len(password) < 8:
        raise ValueError("Password must be at least 8 characters long.")

    pw_hash = hash_password(password)
    user_id = str(uuid.uuid4())
    created = now_iso()

    with get_db() as conn:
        existing = conn.execute("SELECT user_id FROM users WHERE email = ?", (email,)).fetchone()
        if existing:
            raise ValueError(f"An account with email '{email}' already exists.")

        conn.execute(
            "INSERT INTO users (user_id, email, full_name, password_hash, trial_used, created_at) VALUES (?, ?, ?, ?, 0, ?)",
            (user_id, email, full_name, pw_hash, created)
        )

    log_audit(None, "user_registered", f"Registered account for {email}", ip_address)
    return {
        "user_id": user_id,
        "email": email,
        "full_name": full_name,
        "trial_used": False
    }

def login_user(email: str, password: str, ip_address: Optional[str] = None) -> Dict[str, Any]:
    email = email.strip().lower()
    with get_db() as conn:
        row = conn.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()
        if not row:
            log_audit(None, "login_failed", f"Unknown email: {email}", ip_address)
            raise ValueError("Invalid email or password.")

        if not verify_password(password, row["password_hash"]):
            log_audit(None, "login_failed", f"Failed password attempt for {email}", ip_address)
            raise ValueError("Invalid email or password.")

    log_audit(None, "login_success", f"Successful login for {email}", ip_address)
    return {
        "user_id": row["user_id"],
        "email": row["email"],
        "full_name": row["full_name"],
        "trial_used": bool(row["trial_used"])
    }

# =========================================================================
# 7-Day Free Trial Management
# =========================================================================

def create_trial_license(
    email: str,
    full_name: str,
    password: Optional[str],
    hardware_id: str,
    machine_name: str = "Workstation",
    os_name: str = "Desktop",
    client_version: str = "2026.0.1",
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """
    Creates and activates an official 7-day server-managed free trial.
    Enforces that an account/email cannot repeatedly register for free trials.
    Binds the trial to the single requesting device.
    """
    email = email.strip().lower()
    hardware_id = hardware_id.strip().lower()

    if not email or "@" not in email:
        raise ValueError("Valid email address is required to start a trial.")
    if not hardware_id:
        raise ValueError("Hardware identifier is missing.")

    with get_db() as conn:
        user = conn.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()

        if user:
            # If user already exists, verify trial status
            if user["trial_used"]:
                raise ValueError("A 7-day free trial has already been used for this account. Please activate an annual license.")
            user_id = user["user_id"]
            user_name = user["full_name"]
            conn.execute("UPDATE users SET trial_used = 1 WHERE user_id = ?", (user_id,))
        else:
            # Create user on the fly if password is supplied
            user_id = str(uuid.uuid4())
            user_name = full_name.strip() if full_name else email.split("@")[0]
            pw = password if password and len(password) >= 8 else "HakeTrialTmp#2026"
            pw_hash = hash_password(pw)
            conn.execute(
                "INSERT INTO users (user_id, email, full_name, password_hash, trial_used, created_at) VALUES (?, ?, ?, ?, 1, ?)",
                (user_id, email, user_name, pw_hash, now_iso())
            )

        # Check if this hardware_id already has a trial license from another email (prevents device hopping)
        hw_trial = conn.execute(
            """SELECT l.license_id, l.expires_at FROM activations a
               JOIN licenses l ON a.license_id = l.license_id
               WHERE a.hardware_id = ? AND l.plan_tier = 'trial' AND a.is_active = 1""",
            (hardware_id,)
        ).fetchone()
        if hw_trial:
            raise ValueError("This device has already utilized a 7-day free trial. Please enter an activation code to continue.")

        # Create 7-day trial license
        license_id = str(uuid.uuid4())
        raw_code = generate_license_code()
        norm_code = normalize_license_code(raw_code)
        code_h = hash_code(norm_code)
        code_l4 = get_code_last4(norm_code)

        valid_from_dt = datetime.now(timezone.utc)
        expires_at_dt = valid_from_dt + timedelta(days=7)
        valid_from = valid_from_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        expires_at = expires_at_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        ts = now_iso()

        conn.execute(
            """INSERT INTO licenses
               (license_id, customer_email, customer_account_ref, hashed_code, code_last4, plan_tier, status, max_devices, valid_from, expires_at, created_at, updated_at)
               VALUES (?, ?, ?, ?, ?, 'trial', 'active', 1, ?, ?, ?, ?)""",
            (license_id, email, user_id, code_h, code_l4, valid_from, expires_at, ts, ts)
        )

        # Record device activation
        activation_id = str(uuid.uuid4())
        conn.execute(
            """INSERT INTO activations
               (activation_id, license_id, hardware_id, machine_name, os_name, client_version, activated_at, last_checkin_at, is_active)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1)""",
            (activation_id, license_id, hardware_id, machine_name, os_name, client_version, ts, ts)
        )

    # Generate signed Ed25519 token
    token_pkg = create_signed_token(
        license_id=license_id,
        customer_email=email,
        plan_tier="trial",
        hardware_id=hardware_id,
        expires_at=expires_at,
        issued_at=valid_from
    )

    log_audit(license_id, "trial_created", f"Created 7-day trial for {email} on device {machine_name}", ip_address)

    return {
        "success": True,
        "token": token_pkg,
        "license": {
            "license_id": license_id,
            "customer_email": email,
            "customer_name": user_name,
            "plan_tier": "trial",
            "status": "active",
            "max_devices": 1,
            "valid_from": valid_from,
            "expires_at": expires_at,
            "remaining_days": 7
        }
    }

# =========================================================================
# License Activation (16-character Crockford code)
# =========================================================================

def activate_license_code(
    code: str,
    hardware_id: str,
    customer_email: Optional[str] = None,
    machine_name: str = "Workstation",
    os_name: str = "Desktop",
    client_version: str = "2026.0.1",
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """
    Activates an existing 16-character license code.
    Normalizes Crockford Base32 input (converting O->0, I/L->1).
    Enforces maximum device seat limits (default 1).
    Returns signed Ed25519 license token.
    """
    norm_code = normalize_license_code(code)
    code_h = hash_code(norm_code)
    hardware_id = hardware_id.strip().lower()

    if not hardware_id:
        raise ValueError("Client hardware identifier is required for activation.")

    ts = now_iso()

    with get_db() as conn:
        license_row = conn.execute(
            "SELECT * FROM licenses WHERE hashed_code = ?", (code_h,)
        ).fetchone()

        if not license_row:
            log_audit(None, "activate_failed", f"Code hash {code_h[:10]}... not found", ip_address)
            raise ValueError("Invalid activation code. Please check your code and try again.")

        license_id = license_row["license_id"]
        status = license_row["status"]
        expires_at_str = license_row["expires_at"]
        max_devices = license_row["max_devices"]
        plan_tier = license_row["plan_tier"]
        license_email = license_row["customer_email"]

        if status == "revoked":
            log_audit(license_id, "activate_rejected", "License is revoked", ip_address)
            raise ValueError("This license has been revoked. Please contact Hake GeoDesk support.")

        if status == "suspended":
            log_audit(license_id, "activate_rejected", "License is suspended", ip_address)
            raise ValueError("This license is currently suspended. Please contact customer support.")

        # Expiry check
        exp_dt = datetime.strptime(expires_at_str, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        if datetime.now(timezone.utc) > exp_dt:
            log_audit(license_id, "activate_rejected", "License expired", ip_address)
            raise ValueError(f"This license expired on {expires_at_str[:10]}. Please renew your subscription.")

        # Device limit enforcement
        activations = conn.execute(
            "SELECT * FROM activations WHERE license_id = ? AND is_active = 1", (license_id,)
        ).fetchall()

        matched_activation = None
        for act in activations:
            if act["hardware_id"].lower() == hardware_id:
                matched_activation = act
                break

        if matched_activation:
            # Re-activating same device: update timestamp
            conn.execute(
                "UPDATE activations SET last_checkin_at = ?, machine_name = ?, os_name = ?, client_version = ? WHERE activation_id = ?",
                (ts, machine_name, os_name, client_version, matched_activation["activation_id"])
            )
        else:
            # New device: enforce max_devices limit
            if len(activations) >= max_devices:
                log_audit(
                    license_id,
                    "activate_limit_exceeded",
                    f"Device limit ({max_devices}) reached for device {machine_name}",
                    ip_address
                )
                raise ValueError(
                    f"Activation limit reached ({len(activations)} of {max_devices} devices used). "
                    "Please deactivate an existing workstation from the admin dashboard or upgrade your plan."
                )

            # Record new activation
            act_id = str(uuid.uuid4())
            conn.execute(
                """INSERT INTO activations
                   (activation_id, license_id, hardware_id, machine_name, os_name, client_version, activated_at, last_checkin_at, is_active)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1)""",
                (act_id, license_id, hardware_id, machine_name, os_name, client_version, ts, ts)
            )

        # Update license updated_at
        conn.execute("UPDATE licenses SET updated_at = ? WHERE license_id = ?", (ts, license_id))

    # Issue signed token
    token_pkg = create_signed_token(
        license_id=license_id,
        customer_email=license_email,
        plan_tier=plan_tier,
        hardware_id=hardware_id,
        expires_at=expires_at_str
    )

    remaining_days = max(0, (exp_dt - datetime.now(timezone.utc)).days)

    log_audit(license_id, "code_activated", f"Activated on device {machine_name} ({hardware_id[:8]})", ip_address)

    return {
        "success": True,
        "token": token_pkg,
        "license": {
            "license_id": license_id,
            "customer_email": license_email,
            "plan_tier": plan_tier,
            "status": "active",
            "max_devices": max_devices,
            "expires_at": expires_at_str,
            "remaining_days": remaining_days
        }
    }

# =========================================================================
# Administrative Operations
# =========================================================================

def admin_create_license(
    customer_email: str,
    plan_tier: str = "annual",
    valid_days: int = 365,
    max_devices: int = 1,
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """Generates a new license and returns the plaintext Crockford Base32 code once."""
    raw_code = generate_license_code()
    norm_code = normalize_license_code(raw_code)
    code_h = hash_code(norm_code)
    code_l4 = get_code_last4(norm_code)

    now_dt = datetime.now(timezone.utc)
    exp_dt = now_dt + timedelta(days=valid_days)
    valid_from = now_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
    expires_at = exp_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
    license_id = str(uuid.uuid4())
    ts = now_iso()

    with get_db() as conn:
        conn.execute(
            """INSERT INTO licenses
               (license_id, customer_email, customer_account_ref, hashed_code, code_last4, plan_tier, status, max_devices, valid_from, expires_at, created_at, updated_at)
               VALUES (?, ?, NULL, ?, ?, ?, 'active', ?, ?, ?, ?, ?)""",
            (license_id, customer_email.strip().lower(), code_h, code_l4, plan_tier, max_devices, valid_from, expires_at, ts, ts)
        )

    log_audit(license_id, "admin_license_created", f"Created {plan_tier} license for {customer_email}", ip_address)

    return {
        "license_id": license_id,
        "activation_code": format_code(norm_code),
        "customer_email": customer_email,
        "plan_tier": plan_tier,
        "max_devices": max_devices,
        "expires_at": expires_at
    }

def admin_batch_generate(
    count: int,
    plan_tier: str = "business",
    valid_days: int = 365,
    email_prefix: str = "enterprise",
    ip_address: Optional[str] = None
) -> List[Dict[str, Any]]:
    """Batch generates multiple licenses for enterprise or bulk licensing."""
    count = min(max(1, count), 100)
    results = []
    for i in range(count):
        cust_email = f"{email_prefix}+{i+1}@haketech.com"
        lic = admin_create_license(cust_email, plan_tier, valid_days, max_devices=1, ip_address=ip_address)
        results.append(lic)

    log_audit(None, "admin_batch_created", f"Generated batch of {count} licenses", ip_address)
    return results

def admin_extend_license(license_id: str, additional_days: int, ip_address: Optional[str] = None) -> Dict[str, Any]:
    with get_db() as conn:
        row = conn.execute("SELECT * FROM licenses WHERE license_id = ?", (license_id,)).fetchone()
        if not row:
            raise ValueError(f"License '{license_id}' not found.")

        current_exp = datetime.strptime(row["expires_at"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        base_dt = max(datetime.now(timezone.utc), current_exp)
        new_exp_dt = base_dt + timedelta(days=additional_days)
        new_exp = new_exp_dt.strftime("%Y-%m-%dT%H:%M:%SZ")

        conn.execute(
            "UPDATE licenses SET expires_at = ?, status = 'active', updated_at = ? WHERE license_id = ?",
            (new_exp, now_iso(), license_id)
        )

    log_audit(license_id, "admin_license_extended", f"Extended by {additional_days} days to {new_exp}", ip_address)
    return {"license_id": license_id, "new_expires_at": new_exp}

def admin_reset_device(license_id: str, ip_address: Optional[str] = None) -> Dict[str, Any]:
    """Deactivates active workstation bindings so user can register a replacement machine."""
    with get_db() as conn:
        res = conn.execute("UPDATE activations SET is_active = 0 WHERE license_id = ?", (license_id,))
        count = res.rowcount

    log_audit(license_id, "admin_device_reset", f"Deactivated {count} existing device activations", ip_address)
    return {"license_id": license_id, "deactivated_count": count}

def admin_revoke_license(license_id: str, reason: str = "Admin revocation", ip_address: Optional[str] = None) -> Dict[str, Any]:
    with get_db() as conn:
        conn.execute("UPDATE licenses SET status = 'revoked', updated_at = ? WHERE license_id = ?", (now_iso(), license_id))
        conn.execute("UPDATE activations SET is_active = 0 WHERE license_id = ?", (license_id,))

    log_audit(license_id, "admin_license_revoked", f"Revoked: {reason}", ip_address)
    return {"license_id": license_id, "status": "revoked"}

def admin_list_licenses(limit: int = 100) -> List[Dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            """SELECT l.*,
                      (SELECT COUNT(*) FROM activations a WHERE a.license_id = l.license_id AND a.is_active = 1) AS active_devices
               FROM licenses l
               ORDER BY l.created_at DESC LIMIT ?""",
            (limit,)
        ).fetchall()

        results = []
        for r in rows:
            results.append({
                "license_id": r["license_id"],
                "customer_email": r["customer_email"],
                "code_last4": r["code_last4"],
                "plan_tier": r["plan_tier"],
                "status": r["status"],
                "max_devices": r["max_devices"],
                "active_devices": r["active_devices"],
                "valid_from": r["valid_from"],
                "expires_at": r["expires_at"],
                "created_at": r["created_at"]
            })
        return results

def admin_get_audit_logs(limit: int = 100) -> List[Dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            "SELECT * FROM audit_log ORDER BY timestamp DESC LIMIT ?", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]

# =========================================================================
# Phase 3: Daily Refresh, Grace Check & Device Deactivation
# =========================================================================

def refresh_license(
    license_id: str,
    hardware_id: str,
    client_version: str = "2026.0.1",
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """
    Handles periodic check-in from active GeoDesk installations (Phase 3).
    Verifies that the license is not revoked or suspended.
    Updates last_checkin_at timestamp and issues an updated signed token.
    """
    license_id = str(license_id).strip()
    hardware_id = str(hardware_id).strip().lower()
    ts = now_iso()

    with get_db() as conn:
        lic = conn.execute("SELECT * FROM licenses WHERE license_id = ?", (license_id,)).fetchone()
        if not lic:
            raise ValueError(f"License '{license_id}' not found.")

        status = lic["status"]
        if status == "revoked":
            log_audit(license_id, "refresh_rejected", "License is revoked", ip_address)
            raise ValueError("Your license has been revoked. Hake GeoDesk is locked.")
        if status == "suspended":
            log_audit(license_id, "refresh_rejected", "License is suspended", ip_address)
            raise ValueError("Your license is currently suspended. Please contact support.")

        expires_at_str = lic["expires_at"]
        exp_dt = datetime.strptime(expires_at_str, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        if datetime.now(timezone.utc) > exp_dt:
            log_audit(license_id, "refresh_rejected", "License is expired", ip_address)
            raise ValueError(f"License expired on {expires_at_str[:10]}. Please renew to continue.")

        # Update or record check-in
        conn.execute(
            """UPDATE activations
               SET last_checkin_at = ?, client_version = ?
               WHERE license_id = ? AND hardware_id = ? AND is_active = 1""",
            (ts, client_version, license_id, hardware_id)
        )

        customer_email = lic["customer_email"]
        plan_tier = lic["plan_tier"]

    # Issue refreshed signed token
    token_pkg = create_signed_token(
        license_id=license_id,
        customer_email=customer_email,
        plan_tier=plan_tier,
        hardware_id=hardware_id,
        expires_at=expires_at_str,
        issued_at=ts
    )

    remaining_days = max(0, (exp_dt - datetime.now(timezone.utc)).days)
    log_audit(license_id, "license_refreshed", f"Refreshed for device {hardware_id[:8]}", ip_address)

    return {
        "success": True,
        "token": token_pkg,
        "license_id": license_id,
        "plan_tier": plan_tier,
        "status": status,
        "remaining_days": remaining_days,
        "refreshed_at": ts
    }

def deactivate_device(
    license_id: str,
    hardware_id: str,
    reason: str = "Client initiated deactivation",
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """
    Deactivates the license on the requesting workstation (Phase 3).
    Marks activation as inactive, freeing the seat for activation on another machine.
    """
    license_id = str(license_id).strip()
    hardware_id = str(hardware_id).strip().lower()

    with get_db() as conn:
        res = conn.execute(
            """UPDATE activations
               SET is_active = 0
               WHERE license_id = ? AND hardware_id = ? AND is_active = 1""",
            (license_id, hardware_id)
        )
        if res.rowcount == 0:
            raise ValueError("No active workstation binding found for this device and license.")

        conn.execute("UPDATE licenses SET updated_at = ? WHERE license_id = ?", (now_iso(), license_id))

    log_audit(license_id, "device_deactivated", f"Deactivated {hardware_id[:8]}: {reason}", ip_address)
    return {
        "success": True,
        "license_id": license_id,
        "hardware_id": hardware_id,
        "message": "Workstation deactivated successfully. You may now activate GeoDesk on another device."
    }

# =========================================================================
# Phase 4: Payments & Webhook Processing (Stripe / Razorpay)
# =========================================================================

def process_payment_webhook(
    provider: str,
    event_type: str,
    event_data: Dict[str, Any],
    ip_address: Optional[str] = None
) -> Dict[str, Any]:
    """
    Processes incoming payment webhooks from Stripe and Razorpay (Phase 4).
    Handles:
    - Purchase / Checkout: creates license, generates 16-char code, sends code email.
    - Renewal / Invoice paid: extends subscription by 365 days.
    - Refund: revokes license immediately (app locks on next checkin).
    - Dispute: suspends license.
    """
    from app.email_service import send_license_email

    provider = provider.lower()
    ts = now_iso()

    # 1. Purchase / Checkout completed
    if event_type in ("checkout.session.completed", "payment_intent.succeeded", "payment.captured", "order.paid"):
        email = event_data.get("customer_email") or event_data.get("email") or "customer@example.com"
        name = event_data.get("customer_name") or event_data.get("name") or email.split("@")[0]
        plan_tier = event_data.get("plan_tier", "annual")
        payment_id = event_data.get("payment_id") or event_data.get("id", "pay_simulated")
        max_devices = int(event_data.get("max_devices", 1 if plan_tier != "business" else 5))
        valid_days = int(event_data.get("valid_days", 365))

        raw_code = generate_license_code()
        norm_code = normalize_license_code(raw_code)
        code_h = hash_code(norm_code)
        code_l4 = get_code_last4(norm_code)
        formatted_code = format_code(norm_code)

        now_dt = datetime.now(timezone.utc)
        exp_dt = now_dt + timedelta(days=valid_days)
        valid_from = now_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        expires_at = exp_dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        license_id = str(uuid.uuid4())

        with get_db() as conn:
            conn.execute(
                """INSERT INTO licenses
                   (license_id, customer_email, customer_account_ref, hashed_code, code_last4, plan_tier, status, max_devices, valid_from, expires_at, created_at, updated_at)
                   VALUES (?, ?, ?, ?, ?, ?, 'active', ?, ?, ?, ?, ?)""",
                (license_id, email.lower(), payment_id, code_h, code_l4, plan_tier, max_devices, valid_from, expires_at, ts, ts)
            )

        log_audit(license_id, "payment_license_created", f"{provider} {event_type} payment {payment_id} -> Code issued", ip_address)

        # Dispatch code delivery email
        email_record = send_license_email(
            to_email=email,
            customer_name=name,
            activation_code=formatted_code,
            plan_tier=plan_tier,
            expires_at=expires_at
        )

        return {
            "status": "processed",
            "action": "license_created",
            "license_id": license_id,
            "activation_code": formatted_code,
            "customer_email": email,
            "plan_tier": plan_tier,
            "expires_at": expires_at,
            "email_sent": True
        }

    # 2. Subscription Renewal
    if event_type in ("invoice.paid", "subscription.renewed", "subscription.charged"):
        email = event_data.get("customer_email") or event_data.get("email")
        payment_ref = event_data.get("subscription_id") or event_data.get("payment_id")

        with get_db() as conn:
            lic = None
            if payment_ref:
                lic = conn.execute("SELECT * FROM licenses WHERE customer_account_ref = ?", (payment_ref,)).fetchone()
            if not lic and email:
                lic = conn.execute("SELECT * FROM licenses WHERE customer_email = ? ORDER BY created_at DESC", (email.lower(),)).fetchone()

            if not lic:
                raise ValueError(f"No active license found to renew for {email or payment_ref}")

            lic_id = lic["license_id"]
            current_exp = datetime.strptime(lic["expires_at"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
            base_dt = max(datetime.now(timezone.utc), current_exp)
            new_exp_dt = base_dt + timedelta(days=365)
            new_exp = new_exp_dt.strftime("%Y-%m-%dT%H:%M:%SZ")

            conn.execute("UPDATE licenses SET expires_at = ?, status = 'active', updated_at = ? WHERE license_id = ?", (new_exp, ts, lic_id))

        log_audit(lic_id, "payment_license_renewed", f"{provider} {event_type} -> Renewed to {new_exp}", ip_address)
        return {"status": "processed", "action": "license_renewed", "license_id": lic_id, "new_expires_at": new_exp}

    # 3. Refund -> Revoke license immediately
    if event_type in ("charge.refunded", "refund.processed", "payment.refunded"):
        payment_id = event_data.get("payment_id") or event_data.get("charge_id")
        email = event_data.get("customer_email") or event_data.get("email")

        with get_db() as conn:
            lic = None
            if payment_id:
                lic = conn.execute("SELECT * FROM licenses WHERE customer_account_ref = ?", (payment_id,)).fetchone()
            if not lic and email:
                lic = conn.execute("SELECT * FROM licenses WHERE customer_email = ? AND status = 'active' ORDER BY created_at DESC", (email.lower(),)).fetchone()

            if not lic:
                return {"status": "ignored", "reason": "No license associated with refund"}

            lic_id = lic["license_id"]
            conn.execute("UPDATE licenses SET status = 'revoked', updated_at = ? WHERE license_id = ?", (ts, lic_id))
            conn.execute("UPDATE activations SET is_active = 0 WHERE license_id = ?", (lic_id,))

        log_audit(lic_id, "payment_license_refunded", f"{provider} {event_type} -> Revoked immediately", ip_address)
        return {"status": "processed", "action": "license_revoked", "license_id": lic_id}

    # 4. Dispute / Chargeback -> Suspend license
    if event_type in ("charge.dispute.created", "dispute.opened"):
        payment_id = event_data.get("charge_id") or event_data.get("payment_id")
        with get_db() as conn:
            lic = conn.execute("SELECT * FROM licenses WHERE customer_account_ref = ?", (payment_id,)).fetchone() if payment_id else None
            if lic:
                lic_id = lic["license_id"]
                conn.execute("UPDATE licenses SET status = 'suspended', updated_at = ? WHERE license_id = ?", (ts, lic_id))
                log_audit(lic_id, "payment_dispute_suspended", f"{provider} dispute -> Suspended", ip_address)
                return {"status": "processed", "action": "license_suspended", "license_id": lic_id}

    return {"status": "ignored", "reason": f"Unhandled event type: {event_type}"}
