"""
Crockford Base32 16-character License Code Generator and Normalizer.
Alphabet: 0123456789ABCDEFGHJKMNPQRSTVWXYZ (excludes I, L, O, U)
Grouped: XXXX-XXXX-XXXX-XXXX
Entropy: 16 characters * 5 bits = 80 bits
"""

import hashlib
import secrets

ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
ALPHABET_MAP = {c: i for i, c in enumerate(ALPHABET)}

# Forgiving substitution map for common reading/typing mistakes
SUBSTITUTIONS = {
    "O": "0",
    "o": "0",
    "I": "1",
    "i": "1",
    "L": "1",
    "l": "1",
}

def generate_license_code() -> str:
    """
    Generate a 16-character Crockford Base32 license activation code
    formatted as XXXX-XXXX-XXXX-XXXX with 80 bits of cryptographic entropy.
    """
    # 16 random characters chosen from 32-symbol alphabet
    raw = "".join(secrets.choice(ALPHABET) for _ in range(16))
    return f"{raw[0:4]}-{raw[4:8]}-{raw[8:12]}-{raw[12:16]}"

def normalize_license_code(code: str) -> str:
    """
    Normalizes user input:
    - Strips whitespace, dashes, and delimiters
    - Converts to uppercase
    - Translates forgiving character mistakes (O -> 0, I/L -> 1)
    - Verifies valid 16-character Crockford Base32 string
    Raises ValueError if invalid.
    """
    if not code:
        raise ValueError("Activation code cannot be empty.")

    cleaned = []
    for ch in code:
        if ch in " \t\r\n-_:/":
            continue
        up = ch.upper()
        if up in SUBSTITUTIONS:
            up = SUBSTITUTIONS[up]
        if up not in ALPHABET_MAP:
            raise ValueError(f"Invalid character '{ch}' in license code. Crockford Base32 does not use I, L, O, or U.")
        cleaned.append(up)

    result = "".join(cleaned)
    if len(result) != 16:
        raise ValueError(f"License code must contain exactly 16 characters (got {len(result)}).")

    return result

def format_code(normalized_16: str) -> str:
    """Format a 16-character normalized string into XXXX-XXXX-XXXX-XXXX."""
    return f"{normalized_16[0:4]}-{normalized_16[4:8]}-{normalized_16[8:12]}-{normalized_16[12:16]}"

def hash_code(normalized_16: str) -> str:
    """SHA-256 hash of the normalized 16-character activation code."""
    return hashlib.sha256(normalized_16.encode("ascii")).hexdigest()

def get_code_last4(normalized_16: str) -> str:
    """Returns the last 4 characters for support lookups."""
    return normalized_16[-4:]
