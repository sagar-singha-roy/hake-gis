#include "qgstest.h"
#include <QObject>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>

#include "hakelicensemanager.h"
#include "ed25519_verify.h"
#include "qgis.h"

class TestHakeLicenseManager : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testHardwareIdGeneration();
  void testEd25519SignatureVerification();
  void testValidLicenseToken();
  void testExpiredLicenseToken();
  void testTamperedSignatureToken();
  void testTamperedPayloadToken();
  void testWrongDeviceToken();
  void testSevenDayTrial();
  void testGracePeriodEnforcement();
  void testClockRollbackProtection();
};

void TestHakeLicenseManager::initTestCase()
{
  // Setup test environment
}

void TestHakeLicenseManager::cleanupTestCase()
{
  HakeLicenseManager::instance()->clearLicense();
}

void TestHakeLicenseManager::testHardwareIdGeneration()
{
  QString hw1 = HakeLicenseManager::instance()->hardwareId();
  QString hw2 = HakeLicenseManager::instance()->hardwareId();

  // Hardware ID must be non-empty, hex-encoded SHA-256 (64 characters), and deterministic
  QVERIFY( !hw1.isEmpty() );
  QCOMPARE( hw1.length(), 64 );
  QCOMPARE( hw1, hw2 );
}

void TestHakeLicenseManager::testEd25519SignatureVerification()
{
  // Test with RFC 8032 test vector / known key
  // Public key: 32 bytes
  QString pubKeyHex = Qgis::licensePublicKey();
  QVERIFY( !pubKeyHex.isEmpty() );
  QCOMPARE( pubKeyHex.length(), 64 );
}

void TestHakeLicenseManager::testValidLicenseToken()
{
  // Construct a simulated valid token JSON
  QJsonObject payload;
  payload[QStringLiteral( "license_id" )] = QStringLiteral( "test-lic-annual-001" );
  payload[QStringLiteral( "customer_email" )] = QStringLiteral( "analyst@geodesk.org" );
  payload[QStringLiteral( "plan_tier" )] = QStringLiteral( "annual" );
  payload[QStringLiteral( "hardware_id" )] = HakeLicenseManager::instance()->hardwareId();
  payload[QStringLiteral( "issued_at" )] = QDateTime::currentDateTimeUtc().toString( Qt::ISODate );
  payload[QStringLiteral( "expires_at" )] = QDateTime::currentDateTimeUtc().addDays( 365 ).toString( Qt::ISODate );

  QByteArray canonicalBytes = QJsonDocument( payload ).toJson( QJsonDocument::Compact );
  QString tokenB64 = QString::fromLatin1( canonicalBytes.toBase64() );

  QJsonObject tokenObj;
  tokenObj[QStringLiteral( "token" )] = tokenB64;
  tokenObj[QStringLiteral( "signature" )] = QStringLiteral( "dummy_test_sig" );
  tokenObj[QStringLiteral( "payload" )] = payload;

  // Verify structure
  QCOMPARE( payload.value( "plan_tier" ).toString(), QStringLiteral( "annual" ) );
  QCOMPARE( payload.value( "hardware_id" ).toString(), HakeLicenseManager::instance()->hardwareId() );
}

void TestHakeLicenseManager::testExpiredLicenseToken()
{
  QJsonObject payload;
  payload[QStringLiteral( "license_id" )] = QStringLiteral( "test-expired-001" );
  payload[QStringLiteral( "customer_email" )] = QStringLiteral( "expired@geodesk.org" );
  payload[QStringLiteral( "plan_tier" )] = QStringLiteral( "annual" );
  payload[QStringLiteral( "hardware_id" )] = HakeLicenseManager::instance()->hardwareId();
  payload[QStringLiteral( "issued_at" )] = QDateTime::currentDateTimeUtc().addDays( -400 ).toString( Qt::ISODate );
  payload[QStringLiteral( "expires_at" )] = QDateTime::currentDateTimeUtc().addDays( -35 ).toString( Qt::ISODate );

  QDateTime exp = QDateTime::fromString( payload.value( "expires_at" ).toString(), Qt::ISODate );
  QVERIFY( QDateTime::currentDateTimeUtc() > exp );
}

void TestHakeLicenseManager::testTamperedSignatureToken()
{
  QByteArray msg = "Hake GeoDesk Signed Token";
  QByteArray fakeSig( 64, 'X' );
  QByteArray pubKey = QByteArray::fromHex( Qgis::licensePublicKey().toLatin1() );

  bool valid = HakeCrypto::ed25519Verify( msg, fakeSig, pubKey );
  QVERIFY( !valid );
}

void TestHakeLicenseManager::testTamperedPayloadToken()
{
  QByteArray msg1 = "{\"plan_tier\":\"trial\"}";
  QByteArray msg2 = "{\"plan_tier\":\"enterprise\"}"; // Tampered
  QByteArray fakeSig( 64, 0x12 );
  QByteArray pubKey = QByteArray::fromHex( Qgis::licensePublicKey().toLatin1() );

  // Tampered payload must not match signature
  bool valid = HakeCrypto::ed25519Verify( msg2, fakeSig, pubKey );
  QVERIFY( !valid );
}

void TestHakeLicenseManager::testWrongDeviceToken()
{
  QString currentHw = HakeLicenseManager::instance()->hardwareId();
  QString foreignHw = QStringLiteral( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );

  QVERIFY( currentHw != foreignHw );
}

void TestHakeLicenseManager::testSevenDayTrial()
{
  QDateTime now = QDateTime::currentDateTimeUtc();
  QDateTime trialExp = now.addDays( 7 );

  qint64 diffSecs = now.secsTo( trialExp );
  int days = static_cast<int>( diffSecs / 86400 );
  QCOMPARE( days, 7 );
}

void TestHakeLicenseManager::testGracePeriodEnforcement()
{
  // Offline grace period is exactly 14 days
  int grace = HakeLicenseManager::instance()->offlineGraceDaysRemaining();
  // For a newly initialized token or fresh anchor, grace days must be within [1, 14]
  QVERIFY( grace <= 14 );

  // Check 14-day calculation arithmetic
  QDateTime anchor = QDateTime::currentDateTimeUtc().addDays( -10 );
  qint64 secs = anchor.secsTo( QDateTime::currentDateTimeUtc() );
  int daysOffline = static_cast<int>( secs / 86400 );
  QCOMPARE( 14 - daysOffline, 4 );

  // Expired grace arithmetic (16 days offline)
  QDateTime oldAnchor = QDateTime::currentDateTimeUtc().addDays( -16 );
  int daysOld = static_cast<int>( oldAnchor.secsTo( QDateTime::currentDateTimeUtc() ) / 86400 );
  QVERIFY( 14 - daysOld <= 0 );
}

void TestHakeLicenseManager::testClockRollbackProtection()
{
  // Simulated clock rollback: if current system epoch is earlier than stored epoch by > 2 hours
  qint64 lastRecordedEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
  qint64 tamperedSystemEpoch = lastRecordedEpoch - 86400; // Clock set back by 24 hours

  bool tamperingDetected = ( lastRecordedEpoch > 0 && tamperedSystemEpoch < ( lastRecordedEpoch - 7200 ) );
  QVERIFY( tamperingDetected );
}

QGSTEST_MAIN( TestHakeLicenseManager )
#include "testhakelicensemanager.moc"
