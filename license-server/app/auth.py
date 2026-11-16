"""
Authentication and password security module for Hake GeoDesk Licensing Server.
Uses PBKDF2-HMAC-SHA256 with 100,000 iterations for secure password storage.
"""

import hashlib
import hmac
import os
import secrets
import time
from typing import Optional, Dict

ADMIN_USER = os.environ.get("ADMIN_USER", "admin@haketech.com")
ADMIN_PASSWORD = os.environ.get("ADMIN_PASSWORD", "HakeAdmin2026!")

# Active staff admin sessions: token -> {user, expires_at}
_admin_sessions: Dict[str, float] = {}

def hash_password(password: str) -> str:
    """Hash password with PBKDF2-HMAC-SHA256 and unique salt."""
    salt = secrets.token_bytes(16)
    iterations = 100_000
    derived = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iterations)
    return f"pbkdf2:sha256:{iterations}${salt.hex()}${derived.hex()}"

def verify_password(password: str, stored_hash: str) -> bool:
    """Verify password against stored PBKDF2 hash."""
    try:
        parts = stored_hash.split("$")
        if len(parts) != 3:
            return False
        meta, salt_hex, hash_hex = parts
        _, _, iter_str = meta.split(":")
        iterations = int(iter_str)
        salt = bytes.fromhex(salt_hex)
        expected = bytes.fromhex(hash_hex)
        actual = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iterations)
        return hmac.compare_digest(actual, expected)
    except Exception:
        return False

def authenticate_admin(username: str, password: str) -> Optional[str]:
    """Authenticate staff admin credentials and return session token."""
    if username == ADMIN_USER and password == ADMIN_PASSWORD:
        token = secrets.token_urlsafe(32)
        # Session valid for 24 hours
        _admin_sessions[token] = time.time() + 86400
        return token
    return None

def verify_admin_session(token: Optional[str]) -> bool:
    """Check if provided staff session token is valid and not expired."""
    if not token:
        return False
    expires = _admin_sessions.get(token)
    if not expires:
        return False
    if time.time() > expires:
        del _admin_sessions[token]
        return False
    return True
