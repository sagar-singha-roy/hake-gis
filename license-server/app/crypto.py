"""
Cryptographic Service for Hake GeoDesk Licensing.
Handles Ed25519 key persistence, canonical JSON token generation, digital signing,
and token verification.
"""

import base64
import json
import os
from datetime import datetime, timezone
from typing import Dict, Any, Optional

from app.ed25519_pure import (
    generate_keypair,
    public_key_from_secret,
    sign,
    verify,
)

KEY_FILE = os.environ.get(
    "LICENSE_KEY_FILE",
    os.path.join(os.path.dirname(os.path.dirname(__file__)), "server_ed25519_key.json")
)

_SECRET_KEY: Optional[bytes] = None
_PUBLIC_KEY: Optional[bytes] = None

def init_keys():
    """Load or initialize Ed25519 keypair for the license server."""
    global _SECRET_KEY, _PUBLIC_KEY

    # Check environment variable first (hex encoded 32 bytes)
    env_sk = os.environ.get("ED25519_SECRET_KEY")
    if env_sk:
        try:
            _SECRET_KEY = bytes.fromhex(env_sk)
            _PUBLIC_KEY = public_key_from_secret(_SECRET_KEY)
            return
        except Exception:
            pass

    # Check key file
    if os.path.exists(KEY_FILE):
        try:
            with open(KEY_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                _SECRET_KEY = bytes.fromhex(data["secret_key"])
                _PUBLIC_KEY = bytes.fromhex(data["public_key"])
                return
        except Exception:
            pass

    # Generate new keypair if none exists
    sk, pk = generate_keypair()
    _SECRET_KEY = sk
    _PUBLIC_KEY = pk

    try:
        data = {
            "secret_key": sk.hex(),
            "public_key": pk.hex(),
            "created_at": datetime.now(timezone.utc).isoformat()
        }
        with open(KEY_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        try:
            os.chmod(KEY_FILE, 0o600)
        except Exception:
            pass
    except Exception as e:
        print(f"Warning: Could not persist key file: {e}")

def get_secret_key() -> bytes:
    if _SECRET_KEY is None:
        init_keys()
    return _SECRET_KEY

def get_public_key() -> bytes:
    if _PUBLIC_KEY is None:
        init_keys()
    return _PUBLIC_KEY

def get_public_key_hex() -> str:
    return get_public_key().hex()

def canonical_json_bytes(obj: Dict[str, Any]) -> bytes:
    """Format dictionary into canonical JSON bytes with sorted keys and no whitespace."""
    return json.dumps(obj, sort_keys=True, separators=(",", ":")).encode("utf-8")

def create_signed_token(
    license_id: str,
    customer_email: str,
    plan_tier: str,
    hardware_id: str,
    expires_at: str,
    issued_at: Optional[str] = None
) -> Dict[str, Any]:
    """
    Creates a canonical payload and Ed25519 digital signature.
    Returns the complete token package for GeoDesk client storage.
    """
    if issued_at is None:
        issued_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    payload = {
        "customer_email": str(customer_email).strip().lower(),
        "expires_at": str(expires_at).strip(),
        "hardware_id": str(hardware_id).strip().lower(),
        "issued_at": str(issued_at).strip(),
        "license_id": str(license_id).strip(),
        "plan_tier": str(plan_tier).strip().lower()
    }

    canonical_bytes = canonical_json_bytes(payload)
    sig_bytes = sign(canonical_bytes, get_secret_key())

    token_b64 = base64.b64encode(canonical_bytes).decode("ascii")
    sig_b64 = base64.b64encode(sig_bytes).decode("ascii")

    return {
        "token": token_b64,
        "signature": sig_b64,
        "payload": payload,
        "public_key": get_public_key_hex()
    }

def verify_token(token_data: Dict[str, Any], public_key_override: Optional[bytes] = None) -> bool:
    """
    Verifies that a signed token payload has not been tampered with.
    """
    try:
        pk = public_key_override or get_public_key()
        token_b64 = token_data.get("token", "")
        sig_b64 = token_data.get("signature", "")
        canonical_bytes = base64.b64decode(token_b64)
        sig_bytes = base64.b64decode(sig_b64)
        return verify(canonical_bytes, sig_bytes, pk)
    except Exception:
        return False
