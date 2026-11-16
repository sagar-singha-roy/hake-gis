"""
RFC 8032 Pure-Python Ed25519 implementation.
Provides key generation, signing, and verification with zero external C-dependencies.
Works identically across all Python 3.9+ environments on Windows, macOS, and Linux.
"""

import hashlib
import os

# Field size and curve parameters for Curve25519 / Ed25519
q = 2**255 - 19
l = 2**252 + 27742317777372353535851937790883648493

def inv(z):
    return pow(z, q - 2, q)

d = -121665 * inv(121666) % q
I = pow(2, (q - 1) // 4, q)

def xrecover(y):
    xx = (y * y - 1) * inv(d * y * y + 1) % q
    x = pow(xx, (q + 3) // 8, q)
    if (x * x - xx) % q != 0:
        x = (x * I) % q
    if x % 2 != 0:
        x = q - x
    return x

By = 4 * inv(5) % q
Bx = xrecover(By)
B = [Bx, By]

def edwards_add(P, Q):
    x1, y1 = P[0], P[1]
    x2, y2 = Q[0], Q[1]
    x3 = (x1 * y2 + x2 * y1) * inv(1 + d * x1 * x2 * y1 * y2) % q
    y3 = (y1 * y2 + x1 * x2) * inv(1 - d * x1 * x2 * y1 * y2) % q
    return [x3, y3]

def edwards_double(P):
    return edwards_add(P, P)

def scalarmult(P, e):
    if e == 0:
        return [0, 1]
    Q = scalarmult(P, e // 2)
    Q = edwards_double(Q)
    if e & 1:
        Q = edwards_add(Q, P)
    return Q

def encodeint(y):
    bits = [(y >> i) & 1 for i in range(256)]
    return bytes(sum([bits[i * 8 + j] << j for j in range(8)]) for i in range(32))

def encodepoint(P):
    x, y = P[0], P[1]
    bits = [(y >> i) & 1 for i in range(255)] + [x & 1]
    return bytes(sum([bits[i * 8 + j] << j for j in range(8)]) for i in range(32))

def decodeint(b):
    return sum(256**i * b[i] for i in range(32))

def decodepoint(b):
    y = sum(2**i * bit for i, bit in enumerate([
        (b[i // 8] >> (i % 8)) & 1 for i in range(255)
    ]))
    x = xrecover(y)
    if (x & 1) != ((b[31] >> 7) & 1):
        x = q - x
    P = [x, y]
    if not isoncurve(P):
        raise ValueError("Decoding point that is not on curve")
    return P

def isoncurve(P):
    x, y = P[0], P[1]
    return (-x * x + y * y - 1 - d * x * x * y * y) % q == 0

def H(m):
    return hashlib.sha512(m).digest()

def public_key_from_secret(sk_bytes):
    """Derive 32-byte public key from 32-byte secret seed."""
    h = H(sk_bytes)
    a = 2**254 + sum(2**i * bit for i, bit in enumerate([
        (h[i // 8] >> (i % 8)) & 1 for i in range(3, 254)
    ]))
    A = scalarmult(B, a)
    return encodepoint(A)

def sign(m, sk_bytes):
    """
    Produce 64-byte Ed25519 signature over message bytes m.
    sk_bytes: 32-byte secret key seed.
    """
    if isinstance(m, str):
        m = m.encode("utf-8")
    h = H(sk_bytes)
    a = 2**254 + sum(2**i * bit for i, bit in enumerate([
        (h[i // 8] >> (i % 8)) & 1 for i in range(3, 254)
    ]))
    pk = public_key_from_secret(sk_bytes)
    r = int.from_bytes(hashlib.sha512(h[32:] + m).digest(), "little")
    R = scalarmult(B, r)
    R_bytes = encodepoint(R)
    k = int.from_bytes(hashlib.sha512(R_bytes + pk + m).digest(), "little")
    S = (r + k * a) % l
    S_bytes = encodeint(S)
    return R_bytes + S_bytes

def verify(m, sig_bytes, pk_bytes):
    """
    Verify 64-byte Ed25519 signature over message m using 32-byte public key pk_bytes.
    Returns True if valid, False otherwise.
    """
    if isinstance(m, str):
        m = m.encode("utf-8")
    if len(sig_bytes) != 64 or len(pk_bytes) != 32:
        return False
    try:
        R = decodepoint(sig_bytes[:32])
        A = decodepoint(pk_bytes)
        S = decodeint(sig_bytes[32:])
        if S >= l:
            return False
        k = int.from_bytes(hashlib.sha512(sig_bytes[:32] + pk_bytes + m).digest(), "little")
        # Verify: S * B == R + k * A
        SB = scalarmult(B, S)
        kA = scalarmult(A, k)
        RkA = edwards_add(R, kA)
        return SB[0] == RkA[0] and SB[1] == RkA[1]
    except Exception:
        return False

def generate_keypair():
    """Generates a new (secret_key_bytes, public_key_bytes) pair."""
    sk = os.urandom(32)
    pk = public_key_from_secret(sk)
    return sk, pk
