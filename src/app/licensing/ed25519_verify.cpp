#include "ed25519_verify.h"
#include <QString>
#include <openssl/evp.h>

namespace HakeCrypto
{
  bool ed25519Verify( const QByteArray &message,
                      const QByteArray &signature64,
                      const QByteArray &publicKey32 )
  {
    if ( signature64.size() != 64 || publicKey32.size() != 32 )
      return false;

    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519,
      nullptr,
      reinterpret_cast<const unsigned char *>( publicKey32.constData() ),
      32
    );
    if ( !pkey )
      return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if ( !ctx )
    {
      EVP_PKEY_free( pkey );
      return false;
    }

    int ret = EVP_DigestVerifyInit( ctx, nullptr, nullptr, nullptr, pkey );
    if ( ret == 1 )
    {
      ret = EVP_DigestVerify(
        ctx,
        reinterpret_cast<const unsigned char *>( signature64.constData() ),
        64,
        reinterpret_cast<const unsigned char *>( message.constData() ),
        message.size()
      );
    }

    EVP_MD_CTX_free( ctx );
    EVP_PKEY_free( pkey );

    return ( ret == 1 );
  }

  bool ed25519VerifyHex( const QByteArray &message,
                         const QString &signatureHex,
                         const QString &publicKeyHex )
  {
    return ed25519Verify(
      message,
      QByteArray::fromHex( signatureHex.toLatin1() ),
      QByteArray::fromHex( publicKeyHex.toLatin1() )
    );
  }
}
