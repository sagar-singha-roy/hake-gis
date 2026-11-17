"""
API Route Handlers for Hake GeoDesk Licensing Server.
Includes FastAPI app definition, as well as a pure Python fallback request dispatcher.
"""

import json
import os
import re
import time
from typing import Dict, Any, Tuple, Optional

from app.database import get_db, init_db
from app.crypto import get_public_key_hex, init_keys
from app.auth import (
    authenticate_admin,
    verify_admin_session,
)
from app.licensing_service import (
    register_user,
    login_user,
    create_trial_license,
    activate_license_code,
    refresh_license,
    deactivate_device,
    process_payment_webhook,
    admin_create_license,
    admin_batch_generate,
    admin_extend_license,
    admin_reset_device,
    admin_revoke_license,
    admin_list_licenses,
    admin_get_audit_logs,
    admin_list_payments,
    log_audit,
)
from app.webhook_service import handle_stripe_webhook, handle_razorpay_webhook
from app.email_service import list_spooled_emails

ADMIN_UI_HTML = os.path.join(os.path.dirname(__file__), "admin_ui.html")

def handle_api_request(
    method: str,
    path: str,
    body: Dict[str, Any],
    headers: Dict[str, str],
    client_ip: str = "127.0.0.1"
) -> Tuple[int, Dict[str, Any], Optional[str]]:
    """
    Unified router for both FastAPI and Pure-Python HTTP server.
    Returns: (status_code, response_json_or_headers, optional_html_body)
    """
    method = method.upper()

    # Public / Health endpoints
    if method == "GET" and (path == "/api/health" or path == "/health"):
        return 200, {"status": "ok", "service": "Hake GeoDesk Licensing Server", "version": "2026.0.1"}, None

    if method == "GET" and path == "/api/public-key":
        return 200, {"public_key": get_public_key_hex()}, None

    # Admin Panel Web UI (serve on / as well as /admin)
    if method == "GET" and (path == "/" or path == "/admin" or path == "/admin/"):
        if os.path.exists(ADMIN_UI_HTML):
            with open(ADMIN_UI_HTML, "r", encoding="utf-8") as f:
                html = f.read()
            return 200, {"Content-Type": "text/html; charset=utf-8"}, html
        return 404, {"error": "Admin UI not found"}, None

    # Hosted Customer Checkout Portal (Stripe / Razorpay Simulator)
    CHECKOUT_HTML = os.path.join(os.path.dirname(__file__), "checkout.html")
    if method == "GET" and (path == "/checkout" or path.startswith("/checkout")):
        if os.path.exists(CHECKOUT_HTML):
            with open(CHECKOUT_HTML, "r", encoding="utf-8") as f:
                html = f.read()
            return 200, {"Content-Type": "text/html; charset=utf-8"}, html
        return 404, {"error": "Checkout UI not found"}, None

    # Account registration
    if method == "POST" and path == "/api/auth/register":
        try:
            res = register_user(
                full_name=body.get("full_name", ""),
                email=body.get("email", ""),
                password=body.get("password", ""),
                ip_address=client_ip
            )
            return 201, res, None
        except ValueError as e:
            return 400, {"error": str(e)}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # Account login
    if method == "POST" and path == "/api/auth/login":
        try:
            res = login_user(
                email=body.get("email", ""),
                password=body.get("password", ""),
                ip_address=client_ip
            )
            return 200, res, None
        except ValueError as e:
            return 401, {"error": str(e)}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # 7-Day Free Trial
    if method == "POST" and path == "/api/trial":
        try:
            res = create_trial_license(
                email=body.get("email", ""),
                full_name=body.get("full_name", ""),
                password=body.get("password"),
                hardware_id=body.get("hardware_id", ""),
                machine_name=body.get("machine_name", "Workstation"),
                os_name=body.get("os_name", "Desktop"),
                client_version=body.get("client_version", "2026.0.1"),
                ip_address=client_ip
            )
            return 201, res, None
        except ValueError as e:
            return 400, {"error": str(e)}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # Activate 16-character license code
    if method == "POST" and path == "/api/activate":
        try:
            res = activate_license_code(
                code=body.get("code", ""),
                hardware_id=body.get("hardware_id", ""),
                customer_email=body.get("email"),
                machine_name=body.get("machine_name", "Workstation"),
                os_name=body.get("os_name", "Desktop"),
                client_version=body.get("client_version", "2026.0.1"),
                ip_address=client_ip
            )
            return 200, res, None
        except ValueError as e:
            msg = str(e)
            status = 409 if "limit reached" in msg else 400
            return status, {"error": msg}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # Phase 3: Daily Refresh Check-In
    if method == "POST" and path == "/api/refresh":
        try:
            res = refresh_license(
                license_id=body.get("license_id", ""),
                hardware_id=body.get("hardware_id", ""),
                client_version=body.get("client_version", "2026.0.1"),
                ip_address=client_ip
            )
            return 200, res, None
        except ValueError as e:
            return 403, {"error": str(e)}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # Phase 3: Workstation Deactivation / License Transfer
    if method == "POST" and path == "/api/deactivate":
        try:
            res = deactivate_device(
                license_id=body.get("license_id", ""),
                hardware_id=body.get("hardware_id", ""),
                reason=body.get("reason", "Client initiated deactivation"),
                ip_address=client_ip
            )
            return 200, res, None
        except ValueError as e:
            return 400, {"error": str(e)}, None
        except Exception as e:
            return 500, {"error": f"Internal server error: {e}"}, None

    # Phase 4: Stripe Webhook
    if method == "POST" and path == "/api/webhooks/stripe":
        raw_body = json.dumps(body).encode("utf-8")
        sig_header = headers.get("Stripe-Signature", headers.get("stripe-signature"))
        status, res = handle_stripe_webhook(raw_body, sig_header, client_ip)
        return status, res, None

    # Phase 4: Razorpay Webhook
    if method == "POST" and path == "/api/webhooks/razorpay":
        raw_body = json.dumps(body).encode("utf-8")
        sig_header = headers.get("X-Razorpay-Signature", headers.get("x-razorpay-signature"))
        status, res = handle_razorpay_webhook(raw_body, sig_header, client_ip)
        return status, res, None

    # Phase 4: Customer Checkout Simulator (for local purchasing from the desktop client)
    if method == "POST" and path == "/api/checkout/simulate":
        try:
            email = body.get("email", "buyer@geodesk.org")
            name = body.get("name", "Valued Customer")
            plan_tier = body.get("plan_tier", "annual")
            res = process_payment_webhook(
                provider="stripe_simulated",
                event_type="checkout.session.completed",
                event_data={
                    "customer_email": email,
                    "customer_name": name,
                    "plan_tier": plan_tier,
                    "payment_id": f"sim_pay_{int(time.time())}" if 'time' in globals() else "sim_pay_001"
                },
                ip_address=client_ip
            )
            return 200, res, None
        except Exception as e:
            return 500, {"error": f"Checkout simulation error: {e}"}, None

    # Staff Admin Login
    if method == "POST" and path == "/api/admin/login":
        token = authenticate_admin(body.get("username", ""), body.get("password", ""))
        if token:
            return 200, {"token": token}, None
        return 401, {"error": "Invalid staff credentials"}, None

    # -------------------------------------------------------------
    # Protected Admin Endpoints (requires Bearer token or localhost dev mode)
    # -------------------------------------------------------------
    if path.startswith("/api/admin/"):
        auth_hdr = headers.get("Authorization", headers.get("authorization", ""))
        token = auth_hdr.replace("Bearer ", "").strip() if "Bearer " in auth_hdr else ""
        is_authenticated = verify_admin_session(token) or client_ip in ("127.0.0.1", "::1", "localhost")

        if not is_authenticated:
            return 401, {"error": "Staff authentication required"}, None

        if method == "GET" and path == "/api/admin/licenses":
            return 200, admin_list_licenses(), None

        if method == "POST" and path == "/api/admin/licenses":
            res = admin_create_license(
                customer_email=body.get("customer_email", ""),
                plan_tier=body.get("plan_tier", "annual"),
                valid_days=int(body.get("valid_days", 365)),
                max_devices=int(body.get("max_devices", 1)),
                ip_address=client_ip
            )
            return 201, res, None

        if method == "POST" and path == "/api/admin/licenses/batch":
            res = admin_batch_generate(
                count=int(body.get("count", 5)),
                plan_tier=body.get("plan_tier", "business"),
                valid_days=int(body.get("valid_days", 365)),
                email_prefix=body.get("email_prefix", "enterprise"),
                ip_address=client_ip
            )
            return 201, res, None

        extend_match = re.match(r"^/api/admin/licenses/([^/]+)/extend$", path)
        if method == "POST" and extend_match:
            lic_id = extend_match.group(1)
            days = int(body.get("additional_days", 30))
            return 200, admin_extend_license(lic_id, days, client_ip), None

        reset_match = re.match(r"^/api/admin/licenses/([^/]+)/reset-device$", path)
        if method == "POST" and reset_match:
            lic_id = reset_match.group(1)
            return 200, admin_reset_device(lic_id, client_ip), None

        revoke_match = re.match(r"^/api/admin/licenses/([^/]+)/revoke$", path)
        if method == "POST" and revoke_match:
            lic_id = revoke_match.group(1)
            reason = body.get("reason", "Admin revocation")
            return 200, admin_revoke_license(lic_id, reason, client_ip), None

        if method == "GET" and path == "/api/admin/audit-logs":
            return 200, admin_get_audit_logs(), None

        if method == "GET" and path == "/api/admin/emails":
            return 200, list_spooled_emails(), None

        if method == "GET" and path == "/api/admin/payments":
            return 200, admin_list_payments(), None

        if method == "POST" and path == "/api/admin/reset-trials":
            with get_db() as conn:
                conn.execute("DELETE FROM activations WHERE license_id IN (SELECT license_id FROM licenses WHERE plan_tier = 'trial')")
                conn.execute("DELETE FROM licenses WHERE plan_tier = 'trial'")
                conn.execute("UPDATE users SET trial_used = 0")
            log_audit(None, "admin_reset_trials", "Reset all trial limits for device testing", client_ip)
            return 200, {"success": True, "message": "All trial locks reset successfully"}, None

        if method == "POST" and path == "/api/admin/simulate-checkout":
            res = process_payment_webhook(
                provider="stripe",
                event_type="checkout.session.completed",
                event_data={
                    "customer_email": body.get("customer_email", "customer@enterprise.com"),
                    "customer_name": body.get("customer_name", "Enterprise Client"),
                    "plan_tier": body.get("plan_tier", "annual"),
                    "payment_id": f"cs_test_{int(time.time())}"
                },
                ip_address=client_ip
            )
            return 200, res, None

        if method == "POST" and path == "/api/admin/simulate-refund":
            res = process_payment_webhook(
                provider="stripe",
                event_type="charge.refunded",
                event_data={
                    "customer_email": body.get("customer_email"),
                    "payment_id": body.get("payment_id")
                },
                ip_address=client_ip
            )
            return 200, res, None

    return 404, {"error": f"Endpoint not found: {method} {path}"}, None

# =========================================================================
# FastAPI Application (Standard deployment)
# =========================================================================
try:
    from fastapi import FastAPI, Request, Response
    from fastapi.responses import HTMLResponse, JSONResponse
    from fastapi.middleware.cors import CORSMiddleware

    app = FastAPI(title="Hake GeoDesk Licensing Server", version="2026.0.1")

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.on_event("startup")
    def on_startup():
        init_db()
        init_keys()

    @app.api_route("/{path_name:path}", methods=["GET", "POST", "PUT", "DELETE"])
    async def universal_route(path_name: str, request: Request):
        path = "/" + path_name.lstrip("/")
        body = {}
        if request.method in ("POST", "PUT"):
            try:
                body = await request.json()
            except Exception:
                body = {}

        client_ip = request.client.host if request.client else "127.0.0.1"
        status, data, html = handle_api_request(
            method=request.method,
            path=path,
            body=body,
            headers=dict(request.headers),
            client_ip=client_ip
        )
        if html is not None:
            return HTMLResponse(content=html, status_code=status)
        return JSONResponse(content=data, status_code=status)

except ImportError:
    app = None
