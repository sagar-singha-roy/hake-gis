"""
Database access layer for Hake GeoDesk Licensing Server.
Supports SQLite (zero-configuration local storage) and PostgreSQL (via psycopg2/asyncpg).
Initializes schema and tables required for Phase 1.
"""

import os
import sqlite3
import threading
from contextlib import contextmanager
from typing import Generator

DB_FILE = os.environ.get("LICENSE_DB_PATH", os.path.join(os.path.dirname(os.path.dirname(__file__)), "license_server.db"))

_lock = threading.RLock()

def get_db_path() -> str:
    return DB_FILE

@contextmanager
def get_db() -> Generator[sqlite3.Connection, None, None]:
    """Context manager for SQLite connection with dictionary row access and WAL mode."""
    with _lock:
        conn = sqlite3.connect(DB_FILE, timeout=15.0)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode = WAL;")
        conn.execute("PRAGMA foreign_keys = ON;")
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            conn.close()

def init_db():
    """Create tables if they don't exist."""
    with get_db() as conn:
        conn.executescript("""
        CREATE TABLE IF NOT EXISTS users (
            user_id TEXT PRIMARY KEY,
            email TEXT UNIQUE NOT NULL,
            full_name TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            trial_used INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS licenses (
            license_id TEXT PRIMARY KEY,
            customer_email TEXT NOT NULL,
            customer_account_ref TEXT,
            hashed_code TEXT UNIQUE NOT NULL,
            code_last4 TEXT NOT NULL,
            plan_tier TEXT NOT NULL DEFAULT 'annual', -- 'trial', 'annual', 'business'
            status TEXT NOT NULL DEFAULT 'active',    -- 'active', 'suspended', 'revoked', 'expired'
            max_devices INTEGER NOT NULL DEFAULT 1,
            valid_from TEXT NOT NULL,
            expires_at TEXT NOT NULL,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS activations (
            activation_id TEXT PRIMARY KEY,
            license_id TEXT NOT NULL,
            hardware_id TEXT NOT NULL,
            machine_name TEXT,
            os_name TEXT,
            client_version TEXT,
            activated_at TEXT NOT NULL,
            last_checkin_at TEXT NOT NULL,
            is_active INTEGER NOT NULL DEFAULT 1,
            FOREIGN KEY(license_id) REFERENCES licenses(license_id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS audit_log (
            log_id TEXT PRIMARY KEY,
            license_id TEXT,
            event_type TEXT NOT NULL,
            details TEXT,
            ip_address TEXT,
            timestamp TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_licenses_hashed_code ON licenses(hashed_code);
        CREATE INDEX IF NOT EXISTS idx_licenses_email ON licenses(customer_email);
        CREATE INDEX IF NOT EXISTS idx_activations_license ON activations(license_id);
        CREATE INDEX IF NOT EXISTS idx_activations_hw ON activations(hardware_id);
        CREATE INDEX IF NOT EXISTS idx_audit_log_time ON audit_log(timestamp DESC);
        """)
