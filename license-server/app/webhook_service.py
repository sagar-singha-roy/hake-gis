"""
Stripe and Razorpay Webhook Signature Verification and Dispatcher (Phase 4).
Validates HMAC-SHA256 cryptographic signatures according to Stripe and Razorpay webhook standards.
Dispatches payment events to the licensing lifecycle engine.
"""

import hashlib
import hmac
import json
import os
import time
from typing import Dict, Any, Tuple, Optional

from app.licensing_service import process_payment_webhook

STRIPE_SECRET = os.environ.get("STRIPE_WEBHOOK_SECRET", "whsec_hake_geodesk_test_2026")
RAZORPAY_SECRET = os.environ.get("RAZORPAY_WEBHOOK_SECRET", "rzp_sec_hake_geodesk_test_2026")

def verify_stripe_signature(raw_payload: bytes, sig_header: Optional[str]) -> bool:
    """
    Verifies Stripe webhook HMAC-SHA256 signature.
    Stripe-Signature header format: t=<timestamp>,v1=<signature_hex>
    """
    if not sig_header:
        # In local dev without signature header configured
        return True

    try:
        elements = sig_header.split(",")
        timestamp = None
        v1_sigs = []
        for el in elements:
            parts = el.strip().split("=")
            if len(parts) == 2:
                k, v = parts[0].strip(), parts[1].strip()
                if k == "t":
                    timestamp = v
                elif k == "v1":
                    v1_sigs.append(v)

        if not timestamp or not v1_sigs:
            return False

        # Tolerance: 5 minutes (300s)
        now = int(time.time())
        if abs(now - int(timestamp)) > 300:
            return False

        signed_payload = f"{timestamp}.".encode("utf-8") + raw_payload
        expected_sig = hmac.new(
            STRIPE_SECRET.encode("utf-8"),
            signed_payload,
            hashlib.sha256
        ).hexdigest()

        for s in v1_sigs:
            if hmac.compare_digest(expected_sig, s):
                return True
        return False
    except Exception:
        return False

def verify_razorpay_signature(raw_payload: bytes, sig_header: Optional[str]) -> bool:
    """
    Verifies Razorpay webhook HMAC-SHA256 signature.
    X-Razorpay-Signature header is hex-encoded HMAC-SHA256 over raw payload bytes.
    """
    if not sig_header:
        return True

    try:
        expected = hmac.new(
            RAZORPAY_SECRET.encode("utf-8"),
            raw_payload,
            hashlib.sha256
        ).hexdigest()
        return hmac.compare_digest(expected, sig_header.strip())
    except Exception:
        return False

def handle_stripe_webhook(raw_payload: bytes, sig_header: Optional[str], ip_address: str = "127.0.0.1") -> Tuple[int, Dict[str, Any]]:
    if not verify_stripe_signature(raw_payload, sig_header):
        return 400, {"error": "Invalid Stripe webhook signature"}

    try:
        event = json.loads(raw_payload.decode("utf-8"))
        event_type = event.get("type", "")
        data_object = event.get("data", {}).get("object", {})

        # Extract normalized customer info
        event_data = {
            "payment_id": data_object.get("id"),
            "customer_email": data_object.get("customer_details", {}).get("email") or data_object.get("customer_email") or data_object.get("receipt_email"),
            "customer_name": data_object.get("customer_details", {}).get("name") or "GIS Customer",
            "plan_tier": data_object.get("metadata", {}).get("plan_tier", "annual"),
            "valid_days": int(data_object.get("metadata", {}).get("valid_days", 365)),
            "max_devices": int(data_object.get("metadata", {}).get("max_devices", 1))
        }

        result = process_payment_webhook("stripe", event_type, event_data, ip_address)
        return 200, result
    except Exception as e:
        return 500, {"error": f"Webhook processing error: {e}"}

def handle_razorpay_webhook(raw_payload: bytes, sig_header: Optional[str], ip_address: str = "127.0.0.1") -> Tuple[int, Dict[str, Any]]:
    if not verify_razorpay_signature(raw_payload, sig_header):
        return 400, {"error": "Invalid Razorpay webhook signature"}

    try:
        event = json.loads(raw_payload.decode("utf-8"))
        event_type = event.get("event", "")
        payload = event.get("payload", {})
        payment_entity = payload.get("payment", {}).get("entity", {})
        order_entity = payload.get("order", {}).get("entity", {})

        notes = payment_entity.get("notes", {}) or order_entity.get("notes", {})

        event_data = {
            "payment_id": payment_entity.get("id") or order_entity.get("id"),
            "customer_email": payment_entity.get("email") or notes.get("email"),
            "customer_name": notes.get("name", "GIS Customer"),
            "plan_tier": notes.get("plan_tier", "annual"),
            "valid_days": int(notes.get("valid_days", 365)),
            "max_devices": int(notes.get("max_devices", 1))
        }

        result = process_payment_webhook("razorpay", event_type, event_data, ip_address)
        return 200, result
    except Exception as e:
        return 500, {"error": f"Webhook processing error: {e}"}
