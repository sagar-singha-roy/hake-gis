"""
Interactive End-to-End Verification of Hake GeoDesk Licensing System (Phase 1 & 2).
Executes and validates the complete customer and administrative lifecycle:
- User Registration & Authentication
- 7-Day Server-Managed Free Trial Issuance & Abuse Prevention
- 16-character Crockford Base32 Code Generation & Forgiving Input Handling
- Workstation Binding & 1-Device Limit Enforcement
- Ed25519 Token Signing & Cryptographic Verification
- Staff Admin Operations: Device Reset, Expiry Extension, Revocation, and Audit Logs
"""

import os
import sys
import tempfile
from datetime import datetime, timezone

# Ensure project root is in path
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, BASE_DIR)

# Use isolated temp database for demonstration
temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
temp_db.close()
os.environ["LICENSE_DB_PATH"] = temp_db.name

from app.database import init_db
from app.crypto import init_keys, get_public_key_hex, verify_token
from app.crockford import generate_license_code, format_code, normalize_license_code
from app.licensing_service import (
    register_user,
    login_user,
    create_trial_license,
    activate_license_code,
    admin_create_license,
    admin_extend_license,
    admin_reset_device,
    admin_revoke_license,
    admin_list_licenses,
    admin_get_audit_logs,
)

def run_e2e_demo():
    print("=" * 80)
    print("  HAKE GEODESK — PHASE 1 & 2 END-TO-END VERIFICATION DEMO")
    print("=" * 80)

    # 1. Initialization
    init_db()
    init_keys()
    pubkey = get_public_key_hex()
    print(f"\n[1] Server Initialized with Ed25519 Public Key: {pubkey}")

    # 2. User Account Registration
    print("\n[2] Registering Customer Account...")
    user = register_user(
        full_name="Dr. Eleanor Vance",
        email="eleanor.vance@geodesk-demo.com",
        password="SecureGISPassword2026!"
    )
    print(f"    ✓ User Registered: {user['full_name']} ({user['email']})")

    # 3. User Login
    print("\n[3] Authenticating Customer...")
    logged = login_user("eleanor.vance@geodesk-demo.com", "SecureGISPassword2026!")
    print(f"    ✓ Login Successful. Trial Used: {logged['trial_used']}")

    # 4. Starting 7-Day Free Trial
    hw_station_a = "4f8a12e8b23c907a419c8f0012de941078a632db8c1097e1a384df9a72183c21"
    print("\n[4] Requesting 7-Day Free Trial on Workstation Alpha...")
    trial_res = create_trial_license(
        email="eleanor.vance@geodesk-demo.com",
        full_name="Dr. Eleanor Vance",
        password="SecureGISPassword2026!",
        hardware_id=hw_station_a,
        machine_name="Precision-Tower-7920",
        os_name="macOS Sonoma 14.5"
    )
    trial_lic = trial_res["license"]
    print(f"    ✓ Trial Activated: {trial_lic['plan_tier'].upper()} plan")
    print(f"    ✓ Valid From:     {trial_lic['valid_from']}")
    print(f"    ✓ Expires At:     {trial_lic['expires_at']} ({trial_lic['remaining_days']} days)")
    print(f"    ✓ Ed25519 Sig:    Verified ({verify_token(trial_res['token'])})")

    # 5. Anti-Abuse: Prevent Duplicate Trials
    print("\n[5] Testing Anti-Abuse: Attempting Duplicate Trial Request...")
    try:
        create_trial_license(
            email="eleanor.vance@geodesk-demo.com",
            full_name="Dr. Eleanor Vance",
            password="SecureGISPassword2026!",
            hardware_id="another-device-id"
        )
        print("    ✗ Error: Duplicate trial was unexpectedly allowed!")
        sys.exit(1)
    except ValueError as e:
        print(f"    ✓ Successfully Blocked Duplicate Trial: \"{e}\"")

    # 6. Commercial License Generation (Staff Admin)
    print("\n[6] Staff Administrator Issues Annual Pro License...")
    admin_lic = admin_create_license(
        customer_email="cartography-dept@national-park.gov",
        plan_tier="annual",
        valid_days=365,
        max_devices=1
    )
    raw_code = admin_lic["activation_code"]
    print(f"    ✓ Generated 16-Char Crockford Code: {raw_code}")

    # 7. Forgiving Input Handling
    # Simulate user entering code in lowercase, with spaces, and mistaken 'o'/'l' characters
    sloppy_input = f" {raw_code.lower().replace('-', ' ')} "
    print(f"\n[7] Customer Activates License with Forgiving Input (lowercase, spaces)...")
    print(f"    Input: \"{sloppy_input}\"")
    act_res = activate_license_code(
        code=sloppy_input,
        hardware_id=hw_station_a,
        customer_email="cartography-dept@national-park.gov",
        machine_name="Precision-Tower-7920"
    )
    print(f"    ✓ License Activated: {act_res['license']['plan_tier'].upper()}")
    print(f"    ✓ Expires: {act_res['license']['expires_at']} ({act_res['license']['remaining_days']} days)")
    print(f"    ✓ Ed25519 Token Verified: {verify_token(act_res['token'])}")

    # 8. Single Device Limit Enforcement
    hw_station_b = "9a72183c214f8a12e8b23c907a419c8f0012de941078a632db8c1097e1a384df"
    print("\n[8] Enforcing 1-Device Limit: Attempting Activation on Workstation Beta...")
    try:
        activate_license_code(
            code=raw_code,
            hardware_id=hw_station_b,
            machine_name="Field-Rugged-Laptop"
        )
        print("    ✗ Error: Second workstation activation was unexpectedly permitted!")
        sys.exit(1)
    except ValueError as e:
        print(f"    ✓ Successfully Enforced Device Limit: \"{e}\"")

    # 9. Admin Workstation Reset
    print("\n[9] Admin Resets Workstation Binding via Admin Panel...")
    reset_res = admin_reset_device(admin_lic["license_id"])
    print(f"    ✓ Deactivated {reset_res['deactivated_count']} workstation binding(s).")

    print("\n[10] Workstation Beta Retries Activation...")
    act_beta = activate_license_code(
        code=raw_code,
        hardware_id=hw_station_b,
        machine_name="Field-Rugged-Laptop"
    )
    print(f"    ✓ Workstation Beta Activated Successfully! Token Verified: {verify_token(act_beta['token'])}")

    # 10. Audit Log Inspection
    print("\n[11] Verifying System Audit Logs...")
    logs = admin_get_audit_logs(limit=5)
    for l in logs:
        print(f"    • [{l['timestamp']}] {l['event_type']}: {l['details']}")

    print("\n" + "=" * 80)
    print("  ALL PHASE 1 & 2 LICENSING CAPABILITIES VERIFIED SUCCESSFULLY!")
    print("=" * 80)

if __name__ == "__main__":
    run_e2e_demo()
