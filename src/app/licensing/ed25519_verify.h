#ifndef HAKE_ED25519_VERIFY_H
#define HAKE_ED25519_VERIFY_H

#include <QByteArray>

/**
 * \brief Pure C++/Qt implementation of RFC 8032 Ed25519 signature verification.
 *
 * Utilizes Qt's built-in QCryptographicHash (Sha512) and Curve25519 field arithmetic.
 * Completely self-contained: works out-of-the-box across Windows, macOS, and Linux
 * without requiring external OpenSSL, libsodium, or libcrypto library linking.
 */
namespace HakeCrypto
{
  /**
   * \brief Verifies an Ed25519 signature.
   * \param message The original message bytes that were signed.
   * \param signature64 The 64-byte Ed25519 digital signature.
   * \param publicKey32 The 32-byte Ed25519 public key.
   * \return true if the signature is valid; false if invalid or tampered.
   */
  bool ed25519Verify( const QByteArray &message,
                      const QByteArray &signature64,
                      const QByteArray &publicKey32 );

  /**
   * \brief Helper to verify using hex-encoded signature and hex-encoded public key.
   */
  bool ed25519VerifyHex( const QByteArray &message,
                         const QString &signatureHex,
                         const QString &publicKeyHex );
}

#endif // HAKE_ED25519_VERIFY_H
