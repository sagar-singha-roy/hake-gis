"""
Comprehensive Phase 1 Test Suite for Hake GeoDesk Licensing System.
Validates Crockford Base32 handling, Ed25519 digital signatures, account auth,
7-day free trial issuance & anti-abuse checks, 1-device limits, and admin operations.
"""

import os
import sys
import tempfile
import unittest

# Ensure imports resolve
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, BASE_DIR)

# Use isolated temp database for tests
temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
temp_db.close()
os.environ["LICENSE_DB_PATH"] = temp_db.name

from app.database import init_db
from app.crockford import (
    generate_license_code,
    normalize_license_code,
    hash_code,
    format_code,
)
from app.crypto import (
    init_keys,
    create_signed_token,
    verify_token,
    get_public_key,
    get_public_key_hex,
)
from app.licensing_service import (
    register_user,
    login_user,
    create_trial_license,
    activate_license_code,
    admin_create_license,
    admin_batch_generate,
    admin_extend_license,
    admin_reset_device,
    admin_revoke_license,
    admin_list_licenses,
    admin_get_audit_logs,
)
from app.api import handle_api_request

class TestPhase1Licensing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        init_db()
        init_keys()

    def test_01_crockford_base32_and_forgiving_parser(self):
        """Test Crockford Base32 generation, 16-char format, and forgiving normalizer."""
        code = generate_license_code()
        self.assertEqual(len(code), 19) # 16 chars + 3 hyphens
        self.assertEqual(code.count("-"), 3)

        # Normalization
        norm = normalize_license_code(code)
        self.assertEqual(len(norm), 16)
        self.assertEqual(format_code(norm), code)

        # Forgiving input: lower case, spaces, O -> 0, I -> 1, L -> 1
        sloppy = f"  {norm[:4].lower()}- {norm[4:8].lower()} -{norm[8:12]} -{norm[12:]}  "
        self.assertEqual(normalize_license_code(sloppy), norm)

        mistaken = "abco-defi-ghjk-lmnp" # contains 'o', 'i', 'l'
        normalized_mistaken = normalize_license_code(mistaken)
        self.assertIn("0", normalized_mistaken)
        self.assertIn("1", normalized_mistaken)

        # Invalid alphabet character 'U' (excluded in Crockford)
        with self.assertRaises(ValueError):
            normalize_license_code("ABCD-EFGH-JKLM-NPQU")

    def test_02_ed25519_signatures_and_tampering(self):
        """Test Ed25519 digital token signing, verification, and tamper resistance."""
        token_pkg = create_signed_token(
            license_id="test-lic-001",
            customer_email="analyst@geodesk.org",
            plan_tier="annual",
            hardware_id="hw-macbook-pro-m2",
            expires_at="2027-11-16T12:00:00Z"
        )
        self.assertTrue(verify_token(token_pkg))

        # Tampered payload
        tampered_pkg = dict(token_pkg)
        # Modify base64 token data
        tampered_pkg["token"] = tampered_pkg["token"][:-2] + "AA"
        self.assertFalse(verify_token(tampered_pkg))

        # Tampered signature
        tampered_sig_pkg = dict(token_pkg)
        tampered_sig_pkg["signature"] = "A" * len(token_pkg["signature"])
        self.assertFalse(verify_token(tampered_sig_pkg))

    def test_03_user_auth(self):
        """Test account registration and login."""
        user = register_user("Alice Surveyor", "alice@carto.org", "SecurePassword2026!")
        self.assertEqual(user["email"], "alice@carto.org")
        self.assertFalse(user["trial_used"])

        # Prevent duplicate registration
        with self.assertRaises(ValueError):
            register_user("Alice Clone", "alice@carto.org", "AnotherPassword2026!")

        # Valid login
        logged = login_user("alice@carto.org", "SecurePassword2026!")
        self.assertEqual(logged["full_name"], "Alice Surveyor")

        # Invalid password
        with self.assertRaises(ValueError):
            login_user("alice@carto.org", "WrongPassword!")

    def test_04_seven_day_free_trial_and_anti_abuse(self):
        """Test server-managed 7-day trial creation, 1-device binding, and abuse rejection."""
        hw_id = "test-device-uuid-1111"
        res = create_trial_license(
            email="trialuser@geodesk.org",
            full_name="Bob Geologist",
            password="TrialPassword2026!",
            hardware_id=hw_id,
            machine_name="Bob-Workstation",
            os_name="macOS Sonoma"
        )
        self.assertTrue(res["success"])
        self.assertEqual(res["license"]["plan_tier"], "trial")
        self.assertEqual(res["license"]["remaining_days"], 7)
        self.assertEqual(res["license"]["max_devices"], 1)
        self.assertTrue(verify_token(res["token"]))

        # Attempting second trial with same email must fail
        with self.assertRaises(ValueError):
            create_trial_license(
                email="trialuser@geodesk.org",
                full_name="Bob Geologist",
                password="TrialPassword2026!",
                hardware_id="another-device-uuid"
            )

        # Attempting trial on same hardware from different email must fail
        with self.assertRaises(ValueError):
            create_trial_license(
                email="trialuser_fake2@geodesk.org",
                full_name="Bob Fake",
                password="TrialPassword2026!",
                hardware_id=hw_id
            )

    def test_05_activation_and_single_device_enforcement(self):
        """Test code activation and strict 1-device limit."""
        # Staff admin creates an annual license (1 device)
        lic = admin_create_license("enterprise@giscorp.com", plan_tier="annual", max_devices=1)
        code = lic["activation_code"]

        # Activate on Device 1 -> Success
        act1 = activate_license_code(
            code=code,
            hardware_id="dev-001-workstation",
            customer_email="enterprise@giscorp.com",
            machine_name="Workstation-Alpha"
        )
        self.assertTrue(act1["success"])
        self.assertEqual(act1["license"]["plan_tier"], "annual")
        self.assertTrue(verify_token(act1["token"]))

        # Re-activating Device 1 is allowed (e.g. app restart / refresh)
        act1_repeat = activate_license_code(
            code=code,
            hardware_id="dev-001-workstation",
            machine_name="Workstation-Alpha"
        )
        self.assertTrue(act1_repeat["success"])

        # Attempting to activate Device 2 must fail due to 1-device limit
        with self.assertRaises(ValueError) as ctx:
            activate_license_code(
                code=code,
                hardware_id="dev-002-laptop",
                machine_name="Field-Laptop"
            )
        self.assertIn("Activation limit reached", str(ctx.exception))

        # Admin resets device binding
        admin_reset_device(lic["license_id"])

        # Now Device 2 can activate
        act2 = activate_license_code(
            code=code,
            hardware_id="dev-002-laptop",
            machine_name="Field-Laptop"
        )
        self.assertTrue(act2["success"])

    def test_06_admin_operations(self):
        """Test admin batch generation, extend, revoke, and audit logs."""
        batch = admin_batch_generate(count=3, plan_tier="business", email_prefix="team-seat")
        self.assertEqual(len(batch), 3)

        lic_id = batch[0]["license_id"]
        ext = admin_extend_license(lic_id, additional_days=30)
        self.assertIn("new_expires_at", ext)

        rev = admin_revoke_license(lic_id, "Testing revocation")
        self.assertEqual(rev["status"], "revoked")

        # Attempting to activate revoked license must fail
        with self.assertRaises(ValueError) as ctx:
            activate_license_code(batch[0]["activation_code"], hardware_id="any-hw-id")
        self.assertIn("revoked", str(ctx.exception))

        # Audit logs recorded
        logs = admin_get_audit_logs(limit=20)
        self.assertGreater(len(logs), 0)

    def test_07_api_dispatcher(self):
        """Test API route handling over HTTP request dispatcher."""
        # Health check
        code, body, _ = handle_api_request("GET", "/api/health", {}, {})
        self.assertEqual(code, 200)
        self.assertEqual(body["status"], "ok")

        # Public key
        code, body, _ = handle_api_request("GET", "/api/public-key", {}, {})
        self.assertEqual(code, 200)
        self.assertEqual(body["public_key"], get_public_key_hex())

if __name__ == "__main__":
    unittest.main()
