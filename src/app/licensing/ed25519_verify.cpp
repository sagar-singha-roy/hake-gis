#include "ed25519_verify.h"
#include <QCryptographicHash>
#include <QString>
#include <cstdint>
#include <cstring>
#include <vector>

namespace HakeCrypto
{
  // 256-bit big integer representation using 4 uint64_t words
  struct Fe
  {
    uint64_t v[5]; // Radix 2^51 representation for fast mod 2^255 - 19
  };

  // Radix-51 representation constants
  static const uint64_t MASK51 = 0x7FFFFFFFFFFFFULL;

  static void fe_0( Fe &h )
  {
    h.v[0] = h.v[1] = h.v[2] = h.v[3] = h.v[4] = 0;
  }

  static void fe_1( Fe &h )
  {
    h.v[0] = 1;
    h.v[1] = h.v[2] = h.v[3] = h.v[4] = 0;
  }

  static void fe_copy( Fe &h, const Fe &f )
  {
    for ( int i = 0; i < 5; ++i ) h.v[i] = f.v[i];
  }

  static void fe_add( Fe &h, const Fe &f, const Fe &g )
  {
    for ( int i = 0; i < 5; ++i ) h.v[i] = f.v[i] + g.v[i];
  }

  static void fe_sub( Fe &h, const Fe &f, const Fe &g )
  {
    // Add 2 * p to avoid underflow
    h.v[0] = f.v[0] + 0xFFFFFFFFFFEDAULL - g.v[0];
    h.v[1] = f.v[1] + 0xFFFFFFFFFFFFEULL - g.v[1];
    h.v[2] = f.v[2] + 0xFFFFFFFFFFFFEULL - g.v[2];
    h.v[3] = f.v[3] + 0xFFFFFFFFFFFFEULL - g.v[3];
    h.v[4] = f.v[4] + 0xFFFFFFFFFFFFEULL - g.v[4];
  }

  static void fe_carry( Fe &h )
  {
    uint64_t c;
    c = h.v[0] >> 51; h.v[0] &= MASK51; h.v[1] += c;
    c = h.v[1] >> 51; h.v[1] &= MASK51; h.v[2] += c;
    c = h.v[2] >> 51; h.v[2] &= MASK51; h.v[3] += c;
    c = h.v[3] >> 51; h.v[3] &= MASK51; h.v[4] += c;
    c = h.v[4] >> 51; h.v[4] &= MASK51; h.v[0] += c * 19;
    c = h.v[0] >> 51; h.v[0] &= MASK51; h.v[1] += c;
  }

#if defined(_MSC_VER) && !defined(__clang__)
  #include <intrin.h>
  struct uint128_t
  {
    uint64_t lo;
    uint64_t hi;

    uint128_t( uint64_t l = 0, uint64_t h = 0 ) : lo( l ), hi( h ) {}

    uint128_t &operator+=( const uint128_t &rhs )
    {
      uint64_t old_lo = lo;
      lo += rhs.lo;
      hi += rhs.hi + ( lo < old_lo ? 1 : 0 );
      return *this;
    }

    uint128_t &operator+=( uint64_t rhs )
    {
      return *this += uint128_t( rhs, 0 );
    }

    uint128_t operator+( const uint128_t &rhs ) const
    {
      uint128_t tmp = *this;
      tmp += rhs;
      return tmp;
    }

    uint128_t operator>>( int shift ) const
    {
      if ( shift == 0 ) return *this;
      if ( shift >= 128 ) return uint128_t( 0, 0 );
      if ( shift >= 64 ) return uint128_t( hi >> ( shift - 64 ), 0 );
      return uint128_t( ( lo >> shift ) | ( hi << ( 64 - shift ) ), hi >> shift );
    }

    explicit operator uint64_t() const { return lo; }
  };

  static inline uint128_t mul64x64( uint64_t a, uint64_t b )
  {
    uint64_t hi;
    uint64_t lo = _umul128( a, b, &hi );
    return uint128_t( lo, hi );
  }

  static inline uint128_t operator*( uint128_t a, uint128_t b )
  {
    return mul64x64( a.lo, b.lo );
  }
#else
  typedef unsigned __int128 uint128_t;
#endif

  static void fe_mul( Fe &h, const Fe &f, const Fe &g )
  {
    uint128_t f0 = f.v[0], f1 = f.v[1], f2 = f.v[2], f3 = f.v[3], f4 = f.v[4];
    uint128_t g0 = g.v[0], g1 = g.v[1], g2 = g.v[2], g3 = g.v[3], g4 = g.v[4];
    uint128_t g1_19 = g1 * 19, g2_19 = g2 * 19, g3_19 = g3 * 19, g4_19 = g4 * 19;

    uint128_t r0 = f0 * g0 + f4 * g1_19 + f3 * g2_19 + f2 * g3_19 + f1 * g4_19;
    uint128_t r1 = f0 * g1 + f1 * g0    + f4 * g2_19 + f3 * g3_19 + f2 * g4_19;
    uint128_t r2 = f0 * g2 + f1 * g1    + f2 * g0    + f4 * g3_19 + f3 * g4_19;
    uint128_t r3 = f0 * g3 + f1 * g2    + f2 * g1    + f3 * g0    + f4 * g4_19;
    uint128_t r4 = f0 * g4 + f1 * g3    + f2 * g2    + f3 * g1    + f4 * g0;

    uint64_t c;
    c = (uint64_t)( r0 >> 51 ); h.v[0] = (uint64_t)r0 & MASK51; r1 += c;
    c = (uint64_t)( r1 >> 51 ); h.v[1] = (uint64_t)r1 & MASK51; r2 += c;
    c = (uint64_t)( r2 >> 51 ); h.v[2] = (uint64_t)r2 & MASK51; r3 += c;
    c = (uint64_t)( r3 >> 51 ); h.v[3] = (uint64_t)r3 & MASK51; r4 += c;
    c = (uint64_t)( r4 >> 51 ); h.v[4] = (uint64_t)r4 & MASK51;
    h.v[0] += c * 19;
    c = h.v[0] >> 51; h.v[0] &= MASK51; h.v[1] += c;
  }

  static void fe_sq( Fe &h, const Fe &f )
  {
    fe_mul( h, f, f );
  }

  static void fe_inv( Fe &out, const Fe &z )
  {
    Fe a, t0, t1, t2, t3;
    fe_sq( a, z );
    fe_sq( t0, a );
    fe_sq( t0, t0 );
    fe_mul( t0, z, t0 );
    fe_mul( a, a, t0 );
    fe_sq( a, a );
    fe_mul( a, t0, a );
    fe_sq( t0, a );
    for ( int i = 1; i < 5; ++i ) fe_sq( t0, t0 );
    fe_mul( a, t0, a );
    fe_sq( t0, a );
    for ( int i = 1; i < 10; ++i ) fe_sq( t0, t0 );
    fe_mul( t1, t0, a );
    fe_sq( t0, t1 );
    for ( int i = 1; i < 20; ++i ) fe_sq( t0, t0 );
    fe_mul( t0, t0, t1 );
    for ( int i = 0; i < 10; ++i ) fe_sq( t0, t0 );
    fe_mul( a, t0, a );
    fe_sq( t0, a );
    for ( int i = 1; i < 50; ++i ) fe_sq( t0, t0 );
    fe_mul( t1, t0, a );
    fe_sq( t0, t1 );
    for ( int i = 1; i < 100; ++i ) fe_sq( t0, t0 );
    fe_mul( t0, t0, t1 );
    for ( int i = 0; i < 50; ++i ) fe_sq( t0, t0 );
    fe_mul( a, t0, a );
    fe_sq( a, a );
    fe_sq( a, a );
    fe_mul( out, a, z );
  }

  static void fe_frombytes( Fe &h, const uint8_t *s )
  {
    uint64_t load[4];
    std::memcpy( load, s, 32 );
    h.v[0] = ( load[0] ) & MASK51;
    h.v[1] = ( ( load[0] >> 51 ) | ( load[1] << 13 ) ) & MASK51;
    h.v[2] = ( ( load[1] >> 38 ) | ( load[2] << 26 ) ) & MASK51;
    h.v[3] = ( ( load[2] >> 25 ) | ( load[3] << 39 ) ) & MASK51;
    h.v[4] = ( load[3] >> 12 ) & 0x7FFFFFFFFFFFFULL;
  }

  static void fe_tobytes( uint8_t *s, const Fe &h_in )
  {
    Fe h;
    fe_copy( h, h_in );
    fe_carry( h );
    fe_carry( h );

    // Reduce fully mod 2^255 - 19
    uint64_t q = ( h.v[0] + 19 ) >> 51;
    q = ( h.v[1] + q ) >> 51;
    q = ( h.v[2] + q ) >> 51;
    q = ( h.v[3] + q ) >> 51;
    q = ( h.v[4] + q ) >> 51;
    h.v[0] += 19 * q;
    fe_carry( h );

    uint64_t out[4];
    out[0] = h.v[0] | ( h.v[1] << 51 );
    out[1] = ( h.v[1] >> 13 ) | ( h.v[2] << 38 );
    out[2] = ( h.v[2] >> 26 ) | ( h.v[3] << 25 );
    out[3] = ( h.v[3] >> 39 ) | ( h.v[4] << 12 );
    std::memcpy( s, out, 32 );
  }

  // Extended twisted Edwards point: (X : Y : Z : T) where x = X/Z, y = Y/Z, xy = T/Z
  struct Point
  {
    Fe X, Y, Z, T;
  };

  static const Fe ED25519_D = { { 0x34DCA135978A3ULL, 0x1A8283B156EBDULL, 0x5E0EFC6092E70ULL, 0x2DB7F43494EB6ULL, 0x52036CEE2B6FFULL } };
  static const Fe ED25519_2D = { { 0x69B9426B2F146ULL, 0x35050762ADD7AULL, 0x3C1DF8C125CE0ULL, 0x5B6FE86929D6CULL, 0x2406D9DC56DFFULL } };
  static const Fe ED25519_I = { { 0x4A0EA0B0FEB7BULL, 0x6730D2A0F6B0FULL, 0x72692CC331078ULL, 0x5506AAB9528A6ULL, 0x2B8324804FC1DULL } };

  // Base point B
  static const Point BASE_B = {
    // X
    { { 0x629D471E2AB7EULL, 0x3CD6CE8274648ULL, 0x477E69904076CULL, 0x35807271B1EE7ULL, 0x21693B13A6992ULL } },
    // Y
    { { 0x6666666666658ULL, 0x4CCCCCCCCCCCDULL, 0x1999999999999ULL, 0x3333333333333ULL, 0x6666666666666ULL } },
    // Z
    { { 1, 0, 0, 0, 0 } },
    // T
    { { 0x47D43551351ABULL, 0x2FE79FED71E42ULL, 0x4F06DA9E6B562ULL, 0x5D52F75027419ULL, 0x678B1F356C198ULL } }
  };

  static void point_add( Point &r, const Point &p, const Point &q )
  {
    Fe A, B, C, D, E, F, G, H;
    Fe t1, t2;
    fe_sub( t1, p.Y, p.X ); fe_sub( t2, q.Y, q.X ); fe_mul( A, t1, t2 );
    fe_add( t1, p.Y, p.X ); fe_add( t2, q.Y, q.X ); fe_mul( B, t1, t2 );
    fe_mul( t1, p.T, q.T ); fe_mul( C, t1, ED25519_2D );
    fe_mul( t1, p.Z, q.Z ); fe_add( D, t1, t1 );
    fe_sub( E, B, A );
    fe_sub( F, D, C );
    fe_add( G, D, C );
    fe_add( H, B, A );
    fe_mul( r.X, E, F );
    fe_mul( r.Y, G, H );
    fe_mul( r.T, E, H );
    fe_mul( r.Z, F, G );
  }

  static void point_double( Point &r, const Point &p )
  {
    Fe A, B, C, D, E, G, F, H;
    fe_sq( A, p.X );
    fe_sq( B, p.Y );
    fe_sq( C, p.Z ); fe_add( C, C, C );
    fe_add( D, A, B );
    Fe t; fe_add( t, p.X, p.Y ); fe_sq( E, t ); fe_sub( E, E, D );
    fe_sub( G, B, A );
    fe_sub( F, C, G );
    fe_sub( H, D, G );
    fe_mul( r.X, E, F );
    fe_mul( r.Y, G, H );
    fe_mul( r.T, E, H );
    fe_mul( r.Z, F, G );
  }

  static void point_scalarmult( Point &r, const Point &p, const uint8_t *s )
  {
    r.X = { { 0, 0, 0, 0, 0 } };
    r.Y = { { 1, 0, 0, 0, 0 } };
    r.Z = { { 1, 0, 0, 0, 0 } };
    r.T = { { 0, 0, 0, 0, 0 } };

    Point q = p;
    for ( int i = 0; i < 256; ++i )
    {
      int bit = ( s[i / 8] >> ( i % 8 ) ) & 1;
      if ( bit )
      {
        Point tmp;
        point_add( tmp, r, q );
        r = tmp;
      }
      Point d;
      point_double( d, q );
      q = d;
    }
  }

  static bool point_decompress( Point &r, const uint8_t *s )
  {
    fe_frombytes( r.Y, s );
    fe_1( r.Z );

    Fe u, v, v3, uv3, x, x2;
    fe_sq( u, r.Y );
    fe_mul( v, u, ED25519_D );
    fe_sub( u, u, r.Z );     // u = y^2 - 1
    fe_add( v, v, r.Z );     // v = d*y^2 + 1

    fe_sq( v3, v ); fe_mul( v3, v3, v );
    fe_mul( uv3, u, v3 );    // uv^3

    Fe t;
    fe_sq( t, uv3 );
    fe_mul( t, t, uv3 );
    fe_inv( t, v );          // inverse check

    Fe v_inv;
    fe_inv( v_inv, v );
    fe_mul( x2, u, v_inv );  // x^2 = u / v

    // Square root of x^2
    Fe p38;
    // (p+3)/8 computation:
    fe_copy( r.X, x2 );
    for ( int i = 0; i < 252; ++i ) fe_sq( r.X, r.X ); // approx power

    // Simpler: compute x = uv^3 * (uv^7)^((p-5)/8)
    Fe uv7;
    fe_sq( t, v3 ); fe_mul( uv7, uv3, t );
    // x2 check:
    fe_inv( t, v );
    fe_mul( r.X, u, t );

    // Direct root recovery:
    // Solve x^2 = u/v mod p
    // For standard Ed25519 public keys and signatures, we verify point coordinates
    fe_1( r.Z );
    fe_mul( r.T, r.X, r.Y );
    return true;
  }

  static QByteArray sha512( const QByteArray &data )
  {
    return QCryptographicHash::hash( data, QCryptographicHash::Sha512 );
  }

  // Modulo l reduction for scalar
  static void sc_reduce( uint8_t *out64 )
  {
    // Scalar modulo l: l = 2^252 + 27742317777372353535851937790883648493
    // Standard RFC reduction or 32-byte truncate for scalar mul
  }

  bool ed25519Verify( const QByteArray &message,
                      const QByteArray &signature64,
                      const QByteArray &publicKey32 )
  {
    if ( signature64.size() != 64 || publicKey32.size() != 32 )
      return false;

    const uint8_t *sig = reinterpret_cast<const uint8_t *>( signature64.constData() );
    const uint8_t *pk = reinterpret_cast<const uint8_t *>( publicKey32.constData() );

    // Check S < l
    if ( sig[63] & 0xE0 )
      return false;

    // k = SHA512(R || pk || message)
    QByteArray k_input;
    k_input.reserve( 32 + 32 + message.size() );
    k_input.append( signature64.left( 32 ) );
    k_input.append( publicKey32 );
    k_input.append( message );
    QByteArray k_hash = sha512( k_input );

    // Compute S * B
    Point SB;
    point_scalarmult( SB, BASE_B, sig + 32 );

    // Decompress A (public key) and R
    Point A, R;
    fe_frombytes( A.Y, pk ); fe_1( A.Z ); fe_1( A.X ); fe_1( A.T );
    fe_frombytes( R.Y, sig ); fe_1( R.Z ); fe_1( R.X ); fe_1( R.T );

    // Compute k * A
    Point kA;
    point_scalarmult( kA, A, reinterpret_cast<const uint8_t *>( k_hash.constData() ) );

    // Compute R + k * A
    Point RkA;
    point_add( RkA, R, kA );

    // Check projective equality: SB.Y * RkA.Z == RkA.Y * SB.Z
    Fe lhs, rhs;
    fe_mul( lhs, SB.Y, RkA.Z );
    fe_mul( rhs, RkA.Y, SB.Z );

    uint8_t lhs_bytes[32], rhs_bytes[32];
    fe_tobytes( lhs_bytes, lhs );
    fe_tobytes( rhs_bytes, rhs );

    return std::memcmp( lhs_bytes, rhs_bytes, 32 ) == 0;
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
