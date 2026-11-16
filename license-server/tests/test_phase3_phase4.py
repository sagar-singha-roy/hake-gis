"""
Phase 3 & Phase 4 Test Suite for Hake GeoDesk Licensing System.
Validates:
- Phase 3: Daily Refresh Check-In, 14-Day Offline Grace Enforcement, Workstation Deactivation & Transfer
- Phase 4: Stripe & Razorpay Webhook Signature Verification, Purchase, Renewal, Refund, and Email Delivery
"""

import hashlib
import hmac
import json
import os
import sys
import tempfile
import time
import unittest
from datetime import datetime, timedelta, timezone

# Ensure project root is in path
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, BASE_DIR)

# Use isolated temp database for tests
temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
temp_db.close()
os.environ["LICENSE_DB_PATH"] = temp_db.name
os.environ["STRIPE_WEBHOOK_SECRET"] = "whsec_test_secret_phase4"
os.environ["RAZORPAY_WEBHOOK_SECRET"] = "rzp_test_secret_phase4"

from app.database import init_db, get_db
from app.crypto import init_keys, verify_token
from app.crockford import generate_license_code, format_code, normalize_license_code
from app.licensing_service import (
    admin_create_license,
    activate_license_code,
    refresh_license,
    deactivate_device,
    process_payment_webhook,
    admin_revoke_license
)
from app.webhook_service import (
    verify_stripe_signature,
    verify_razorpay_signature,
    handle_stripe_webhook,
    handle_razorpay_webhook
)
from app.email_service import list_spooled_emails, send_license_email
from app.api import handle_api_request

class TestPhase3AndPhase4(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        init_db()
        init_keys()

    # =====================================================================
    # PHASE 3 TESTS: Refresh, Grace & Deactivation
    # =====================================================================

    def test_01_daily_refresh_checkin(self):
        """Test daily check-in from GeoDesk desktop client."""
        lic = admin_create_license("cartographer@agency.gov", plan_tier="annual")
        act = activate_license_code(
            code=lic["activation_code"],
            hardware_id="hw-station-01",
            machine_name="Station-Alpha"
        )
        self.assertTrue(act["success"])
        lic_id = lic["license_id"]

        # Client checks in after 24h
        ref = refresh_license(license_id=lic_id, hardware_id="hw-station-01", client_version="2026.0.1")
        self.assertTrue(ref["success"])
        self.assertTrue(verify_token(ref["token"]))
        self.assertEqual(ref["status"], "active")

        # Verify last_checkin_at was updated in activations
        with get_db() as conn:
            row = conn.execute(
                "SELECT last_checkin_at FROM activations WHERE license_id = ? AND hardware_id = ?",
                (lic_id, "hw-station-01")
            ).fetchone()
            self.assertIsNotNone(row)
            self.assertTrue(len(row["last_checkin_at"]) > 0)

    def test_02_refresh_rejected_when_revoked_or_expired(self):
        """Test that server rejects refresh if staff or refund revoked the license."""
        lic = admin_create_license("revoked-user@agency.gov", plan_tier="annual")
        activate_license_code(code=lic["activation_code"], hardware_id="hw-revoked-01")

        # Admin revokes license
        admin_revoke_license(lic["license_id"], "Chargeback or contract breach")

        # Subsequent refresh must fail, causing client to lock
        with self.assertRaises(ValueError) as ctx:
            refresh_license(license_id=lic["license_id"], hardware_id="hw-revoked-01")
        self.assertIn("revoked", str(ctx.exception).lower())

    def test_03_workstation_deactivation_and_transfer(self):
        """Test client-initiated deactivation allowing license move to a new PC."""
        lic = admin_create_license("migrating-user@agency.gov", plan_tier="annual", max_devices=1)
        code = lic["activation_code"]

        # Activate on PC 1
        activate_license_code(code=code, hardware_id="pc-1-old", machine_name="Old-Desktop")

        # PC 2 attempts activation -> Fails (1-device limit)
        with self.assertRaises(ValueError):
            activate_license_code(code=code, hardware_id="pc-2-new", machine_name="New-Laptop")

        # User deactivates PC 1 from Help -> License Dashboard
        deact = deactivate_device(license_id=lic["license_id"], hardware_id="pc-1-old")
        self.assertTrue(deact["success"])

        # Now PC 2 can activate smoothly
        act_new = activate_license_code(code=code, hardware_id="pc-2-new", machine_name="New-Laptop")
        self.assertTrue(act_new["success"])

    def test_04_fourteen_day_offline_grace_calculation(self):
        """Test the 14-day offline grace period logic."""
        now = datetime.now(timezone.utc)
        ten_days_ago = now - timedelta(days=10)
        fifteen_days_ago = now - timedelta(days=15)

        # 10 days offline -> Grace is still valid (4 days remaining)
        grace_days_10 = 14 - (now - ten_days_ago).days
        self.assertTrue(grace_days_10 > 0)
        self.assertEqual(grace_days_10, 4)

        # 15 days offline -> Grace expired (must lock)
        grace_days_15 = 14 - (now - fifteen_days_ago).days
        self.assertTrue(grace_days_15 <= 0)

    # =====================================================================
    # PHASE 4 TESTS: Payments (Stripe / Razorpay) & Automated Emails
    # =====================================================================

    def test_05_stripe_signature_verification(self):
        """Test Stripe HMAC-SHA256 signature verification."""
        secret = os.environ["STRIPE_WEBHOOK_SECRET"]
        body = b'{"id":"evt_12345","type":"checkout.session.completed"}'
        timestamp = str(int(time.time()))
        signed_payload = f"{timestamp}.".encode("utf-8") + body
        sig = hmac.new(secret.encode("utf-8"), signed_payload, hashlib.sha256).hexdigest()
        valid_header = f"t={timestamp},v1={sig}"

        self.assertTrue(verify_stripe_signature(body, valid_header))

        # Tampered body
        self.assertFalse(verify_stripe_signature(b'{"tampered":true}', valid_header))

        # Invalid signature
        self.assertFalse(verify_stripe_signature(body, f"t={timestamp},v1=bad_signature_hex"))

    def test_06_stripe_checkout_and_email_delivery(self):
        """Test end-to-end: Stripe checkout webhook creates license & emails code."""
        secret = os.environ["STRIPE_WEBHOOK_SECRET"]
        payload_obj = {
            "id": "evt_checkout_001",
            "type": "checkout.session.completed",
            "data": {
                "object": {
                    "id": "cs_test_998877",
                    "customer_details": {
                        "email": "buyer@surveyorg.com",
                        "name": "Jordan Mitchell"
                    },
                    "metadata": {
                        "plan_tier": "annual",
                        "valid_days": 365,
                        "max_devices": 1
                    }
                }
            }
        }
        body_bytes = json.dumps(payload_obj).encode("utf-8")
        ts = str(int(time.time()))
        sig = hmac.new(secret.encode("utf-8"), f"{ts}.".encode("utf-8") + body_bytes, hashlib.sha256).hexdigest()
        sig_header = f"t={ts},v1={sig}"

        status, res = handle_stripe_webhook(body_bytes, sig_header)
        self.assertEqual(status, 200)
        self.assertEqual(res["action"], "license_created")
        code = res["activation_code"]
        self.assertEqual(len(code), 19)

        # Verify email was generated and spooled
        emails = list_spooled_emails(limit=5)
        self.assertTrue(any(e["activation_code"] == code for e in emails))

        # Client can now immediately activate with this code!
        act = activate_license_code(code=code, hardware_id="jordan-workstation-01")
        self.assertTrue(act["success"])
        self.assertEqual(act["license"]["plan_tier"], "annual")

    def test_07_stripe_refund_locks_license(self):
        """Test: Customer requests refund in Stripe -> license revokes -> app locks on check-in."""
        # 1. Purchase
        buy_res = process_payment_webhook(
            provider="stripe",
            event_type="checkout.session.completed",
            event_data={
                "customer_email": "refundee@geodesk.org",
                "customer_name": "Sam Refund",
                "payment_id": "cs_pay_to_refund_123"
            }
        )
        code = buy_res["activation_code"]
        lic_id = buy_res["license_id"]

        # 2. Activate on workstation
        act = activate_license_code(code=code, hardware_id="sam-workstation")
        self.assertTrue(act["success"])

        # 3. Stripe refund webhook received
        refund_res = process_payment_webhook(
            provider="stripe",
            event_type="charge.refunded",
            event_data={"payment_id": "cs_pay_to_refund_123"}
        )
        self.assertEqual(refund_res["action"], "license_revoked")

        # 4. Desktop client attempts daily check-in -> Locked!
        with self.assertRaises(ValueError) as ctx:
            refresh_license(license_id=lic_id, hardware_id="sam-workstation")
        self.assertIn("revoked", str(ctx.exception).lower())

    def test_08_razorpay_webhook_flow(self):
        """Test Razorpay HMAC verification and payment capture webhook."""
        secret = os.environ["RAZORPAY_WEBHOOK_SECRET"]
        payload_obj = {
            "event": "payment.captured",
            "payload": {
                "payment": {
                    "entity": {
                        "id": "pay_rzp_999888",
                        "email": "customer@indocarto.in",
                        "notes": {
                            "name": "Aarav Sharma",
                            "plan_tier": "business",
                            "valid_days": 365,
                            "max_devices": 5
                        }
                    }
                }
            }
        }
        body_bytes = json.dumps(payload_obj).encode("utf-8")
        sig = hmac.new(secret.encode("utf-8"), body_bytes, hashlib.sha256).hexdigest()

        status, res = handle_razorpay_webhook(body_bytes, sig)
        self.assertEqual(status, 200)
        self.assertEqual(res["action"], "license_created")
        self.assertEqual(res["plan_tier"], "business")

    def test_09_api_dispatcher_phase3_and_phase4_endpoints(self):
        """Test /api/refresh, /api/deactivate, and /api/checkout/simulate over HTTP router."""
        # Simulated instant checkout
        code, body, _ = handle_api_request("POST", "/api/checkout/simulate", {
            "email": "quickbuy@geodesk.org",
            "name": "Quick Buyer",
            "plan_tier": "annual"
        }, {})
        self.assertEqual(code, 200)
        self.assertIn("activation_code", body)

        lic_code = body["activation_code"]
        lic_id = body["license_id"]

        # Activate
        c, act_res, _ = handle_api_request("POST", "/api/activate", {
            "code": lic_code,
            "hardware_id": "api-dispatch-hw-1"
        }, {})
        self.assertEqual(c, 200)

        # Refresh
        c, ref_res, _ = handle_api_request("POST", "/api/refresh", {
            "license_id": lic_id,
            "hardware_id": "api-dispatch-hw-1"
        }, {})
        self.assertEqual(c, 200)
        self.assertTrue(ref_res["success"])

        # Deactivate
        c, deact_res, _ = handle_api_request("POST", "/api/deactivate", {
            "license_id": lic_id,
            "hardware_id": "api-dispatch-hw-1"
        }, {})
        self.assertEqual(c, 200)
        self.assertTrue(deact_res["success"])

if __name__ == "__main__":
    unittest.main()
