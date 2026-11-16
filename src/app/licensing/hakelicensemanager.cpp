#include "hakelicensemanager.h"
#include "ed25519_verify.h"
#include "qgis.h"
#include "qgsapplication.h"
#include "qgsauthmanager.h"
#include "qgssettings.h"
#include "qgsnetworkaccessmanager.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>
#include <QUrl>
#include <QByteArray>
#include <QTimer>

static const char *AUTH_SETTING_TOKEN_KEY = "hake_geodesk_license_token";
static const char *SETTINGS_LAST_CLOCK_KEY = "hake_licensing/last_verified_clock";
static const char *SETTINGS_LAST_CHECKIN_KEY = "hake_licensing/last_checkin_timestamp";
static const char *APP_SALT = "HakeGeoDeskEnterpriseSalt2026";

HakeLicenseManager *HakeLicenseManager::instance()
{
  static HakeLicenseManager sInstance;
  return &sInstance;
}

HakeLicenseManager::HakeLicenseManager( QObject *parent )
  : QObject( parent )
{
  loadCachedToken();
  setupRefreshTimer();
}

void HakeLicenseManager::setupRefreshTimer()
{
  // Daily background refresh timer (24 hours = 86,400,000 ms)
  mRefreshTimer = new QTimer( this );
  mRefreshTimer->setInterval( 24 * 3600 * 1000 );
  connect( mRefreshTimer, &QTimer::timeout, this, [this]() {
    if ( mCurrentLicense.isValid )
    {
      refreshLicense( nullptr );
    }
  } );
  mRefreshTimer->start();

  // On startup: if > 24 hours since last check-in, check in immediately in the background
  QgsSettings settings;
  qint64 lastCheckinEpoch = settings.value( SETTINGS_LAST_CHECKIN_KEY, 0 ).toLongLong();
  if ( lastCheckinEpoch > 0 )
  {
    mLastCheckinTime = QDateTime::fromSecsSinceEpoch( lastCheckinEpoch, Qt::UTC );
  }

  qint64 currentEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
  if ( mCurrentLicense.isValid && ( lastCheckinEpoch == 0 || ( currentEpoch - lastCheckinEpoch ) > 86400 ) )
  {
    // Defer checkin slightly to let startup finish
    QTimer::singleShot( 3000, this, [this]() {
      if ( mCurrentLicense.isValid )
      {
        refreshLicense( nullptr );
      }
    } );
  }
}

int HakeLicenseManager::offlineGraceDaysRemaining() const
{
  if ( !mCurrentLicense.isValid )
    return 0;

  QDateTime anchor = mLastCheckinTime.isValid() ? mLastCheckinTime : mCurrentLicense.issuedAt;
  if ( !anchor.isValid() )
    anchor = QDateTime::currentDateTimeUtc();

  qint64 secsOffline = anchor.secsTo( QDateTime::currentDateTimeUtc() );
  int daysOffline = static_cast<int>( secsOffline / 86400 );
  return 14 - daysOffline;
}

bool HakeLicenseManager::isGracePeriodActive() const
{
  return mCurrentLicense.isValid && offlineGraceDaysRemaining() > 0 && offlineGraceDaysRemaining() < 14;
}

QString HakeLicenseManager::hardwareId() const
{
  if ( !mHardwareId.isEmpty() )
    return mHardwareId;

  // Derive stable, anonymous hardware identifier
  QByteArray machineRaw = QSysInfo::machineUniqueId();
  if ( machineRaw.isEmpty() )
  {
    // Fallback combining host name and product type
    machineRaw = ( QSysInfo::machineHostName() + "_" + QSysInfo::productType() ).toUtf8();
  }

  QByteArray toHash = machineRaw + APP_SALT;
  mHardwareId = QCryptographicHash::hash( toHash, QCryptographicHash::Sha256 ).toHex();
  return mHardwareId;
}

QString HakeLicenseManager::machineName() const
{
  return QSysInfo::machineHostName();
}

QString HakeLicenseManager::osName() const
{
  return QSysInfo::prettyProductName();
}

QString HakeLicenseManager::clientVersion() const
{
  return Qgis::productVersionLabel();
}

HakeLicenseManager::LicenseInfo HakeLicenseManager::currentLicense() const
{
  return mCurrentLicense;
}

int HakeLicenseManager::remainingDays() const
{
  if ( !mCurrentLicense.isValid )
    return 0;

  qint64 secs = QDateTime::currentDateTimeUtc().secsTo( mCurrentLicense.expiresAt );
  if ( secs <= 0 )
    return 0;

  return static_cast<int>( secs / 86400 ) + 1;
}

bool HakeLicenseManager::isExpiringSoon() const
{
  return mCurrentLicense.isValid && remainingDays() <= 7;
}

bool HakeLicenseManager::isLicenseValidOffline()
{
  if ( !mInitialized )
  {
    loadCachedToken();
  }

  if ( !mCurrentLicense.isValid )
    return false;

  // 1. Clock rollback check: ensure current system time is not earlier than last recorded verification
  QgsSettings settings;
  qint64 lastEpoch = settings.value( SETTINGS_LAST_CLOCK_KEY, 0 ).toLongLong();
  qint64 currentEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

  // Allow 2 hours tolerance for NTP adjustments / timezone shifts
  if ( lastEpoch > 0 && currentEpoch < ( lastEpoch - 7200 ) )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "System clock tampering detected. System clock was set back." );
    return false;
  }

  // Update clock verification anchor
  settings.setValue( SETTINGS_LAST_CLOCK_KEY, currentEpoch );

  // 2. Expiration check
  if ( QDateTime::currentDateTimeUtc() > mCurrentLicense.expiresAt )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "License expired on %1." ).arg( mCurrentLicense.expiresAt.toString( Qt::ISODate ) );
    return false;
  }

  // 3. Hardware binding check
  if ( mCurrentLicense.hardwareId.toLower() != hardwareId().toLower() )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "License hardware mismatch. Bound to a different workstation." );
    return false;
  }

  // 4. 14-day offline grace period enforcement (Phase 3)
  if ( offlineGraceDaysRemaining() <= 0 )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "14-day offline grace period expired. Please connect to the internet to verify your license." );
    return false;
  }

  return true;
}

bool HakeLicenseManager::verifyTokenPayload( const QByteArray &canonicalJsonBytes, const QByteArray &signatureBytes )
{
  QString pubKeyHex = Qgis::licensePublicKey();
  QByteArray pubKeyBytes = QByteArray::fromHex( pubKeyHex.toLatin1() );

  return HakeCrypto::ed25519Verify( canonicalJsonBytes, signatureBytes, pubKeyBytes );
}

bool HakeLicenseManager::saveLicenseToken( const QJsonObject &responseObject, const QString &userName )
{
  QJsonObject tokenObj = responseObject;
  if ( responseObject.contains( "token" ) && responseObject["token"].isObject() )
  {
    tokenObj = responseObject["token"].toObject();
  }

  QString tokenB64 = tokenObj.value( "token" ).toString();
  QString sigB64 = tokenObj.value( "signature" ).toString();
  QJsonObject payloadObj = tokenObj.value( "payload" ).toObject();

  if ( tokenB64.isEmpty() || sigB64.isEmpty() || payloadObj.isEmpty() )
  {
    return false;
  }

  // Verify signature before saving
  QByteArray canonicalBytes = QByteArray::fromBase64( tokenB64.toLatin1() );
  QByteArray sigBytes = QByteArray::fromBase64( sigB64.toLatin1() );

  if ( !verifyTokenPayload( canonicalBytes, sigBytes ) )
  {
    return false;
  }

  // Persist into user profile storage
  QJsonObject toStore;
  toStore["token"] = tokenB64;
  toStore["signature"] = sigB64;
  toStore["payload"] = payloadObj;
  if ( !userName.isEmpty() )
  {
    toStore["user_name"] = userName;
  }

  QByteArray storeBytes = QJsonDocument( toStore ).toJson( QJsonDocument::Compact );

  // Try encrypted QgsAuthManager first
  bool saved = false;
  if ( QgsApplication::authManager() )
  {
    saved = QgsApplication::authManager()->storeAuthSetting( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ), QString::fromUtf8( storeBytes ) );
  }

  // Fallback to QgsSettings
  if ( !saved )
  {
    QgsSettings settings;
    settings.setValue( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ), QString::fromUtf8( storeBytes ) );
  }

  // Re-read and update current active license state
  loadCachedToken();
  emit licenseStatusChanged( mCurrentLicense.isValid, mCurrentLicense.planTier );
  return true;
}

bool HakeLicenseManager::loadCachedToken()
{
  mInitialized = true;
  mCurrentLicense = LicenseInfo();

  QString rawStored;
  if ( QgsApplication::authManager() )
  {
    rawStored = QgsApplication::authManager()->authSetting( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ) ).toString();
  }

  if ( rawStored.isEmpty() )
  {
    QgsSettings settings;
    rawStored = settings.value( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ) ).toString();
  }

  if ( rawStored.isEmpty() )
  {
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson( rawStored.toUtf8() );
  if ( !doc.isObject() )
    return false;

  QJsonObject obj = doc.object();
  QString tokenB64 = obj.value( "token" ).toString();
  QString sigB64 = obj.value( "signature" ).toString();
  QJsonObject payload = obj.value( "payload" ).toObject();

  QByteArray canonicalBytes = QByteArray::fromBase64( tokenB64.toLatin1() );
  QByteArray sigBytes = QByteArray::fromBase64( sigB64.toLatin1() );

  // Verify signature
  if ( !verifyTokenPayload( canonicalBytes, sigBytes ) )
  {
    mCurrentLicense.statusString = tr( "Cryptographic signature verification failed." );
    return false;
  }

  // Populate LicenseInfo
  mCurrentLicense.rawTokenBase64 = tokenB64;
  mCurrentLicense.signatureBase64 = sigB64;
  mCurrentLicense.licenseId = payload.value( "license_id" ).toString();
  mCurrentLicense.customerEmail = payload.value( "customer_email" ).toString();
  mCurrentLicense.customerName = obj.value( "user_name" ).toString( mCurrentLicense.customerEmail );
  mCurrentLicense.planTier = payload.value( "plan_tier" ).toString();
  mCurrentLicense.hardwareId = payload.value( "hardware_id" ).toString();
  mCurrentLicense.issuedAt = QDateTime::fromString( payload.value( "issued_at" ).toString(), Qt::ISODate );
  mCurrentLicense.expiresAt = QDateTime::fromString( payload.value( "expires_at" ).toString(), Qt::ISODate );

  // Check hardware binding and expiry
  if ( mCurrentLicense.hardwareId.toLower() != hardwareId().toLower() )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "Workstation hardware mismatch." );
    return false;
  }

  if ( QDateTime::currentDateTimeUtc() > mCurrentLicense.expiresAt )
  {
    mCurrentLicense.isValid = false;
    mCurrentLicense.statusString = tr( "License expired." );
    return false;
  }

  mCurrentLicense.isValid = true;
  mCurrentLicense.statusString = tr( "Active (%1)" ).arg( mCurrentLicense.planTier );
  return true;
}

void HakeLicenseManager::clearLicense()
{
  if ( QgsApplication::authManager() )
  {
    QgsApplication::authManager()->removeAuthSetting( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ) );
  }

  QgsSettings settings;
  settings.remove( QString::fromLatin1( AUTH_SETTING_TOKEN_KEY ) );

  mCurrentLicense = LicenseInfo();
  emit licenseStatusChanged( false, QString() );
}

// -------------------------------------------------------------------------
// Network API Implementation
// -------------------------------------------------------------------------

void HakeLicenseManager::sendPostRequest(
  const QString &endpointPath,
  const QJsonObject &payload,
  std::function<void( int statusCode, const QJsonObject &responseJson, const QString &errorString )> callback )
{
  QString baseUrl = Qgis::licenseServerUrl().trimmed();
  while ( baseUrl.endsWith( QLatin1Char( '/' ) ) )
  {
    baseUrl.chop( 1 );
  }
  QUrl url( baseUrl + endpointPath );

  QNetworkRequest request( url );
  request.setHeader( QNetworkRequest::ContentTypeHeader, QStringLiteral( "application/json" ) );

  QByteArray data = QJsonDocument( payload ).toJson( QJsonDocument::Compact );

  QNetworkReply *reply = QgsNetworkAccessManager::instance()->post( request, data );

  connect( reply, &QNetworkReply::finished, this, [reply, callback]() {
    reply->deleteLater();
    int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    QByteArray body = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson( body );
    QJsonObject json = doc.isObject() ? doc.object() : QJsonObject();

    QString errorMsg;
    if ( reply->error() != QNetworkReply::NoError && statusCode == 0 )
    {
      errorMsg = reply->errorString();
    }
    else if ( json.contains( "error" ) )
    {
      errorMsg = json.value( "error" ).toString();
    }

    callback( statusCode, json, errorMsg );
  } );
}

void HakeLicenseManager::startTrial(
  const QString &fullName,
  const QString &email,
  const QString &password,
  std::function<void( bool success, const QString &message )> callback )
{
  QJsonObject payload;
  payload["email"] = email;
  payload["full_name"] = fullName;
  payload["password"] = password;
  payload["hardware_id"] = hardwareId();
  payload["machine_name"] = machineName();
  payload["os_name"] = osName();
  payload["client_version"] = clientVersion();

  sendPostRequest( QStringLiteral( "/api/trial" ), payload,
    [this, fullName, callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 || statusCode == 201 )
      {
        if ( saveLicenseToken( json, fullName ) )
        {
          callback( true, tr( "7-day free trial successfully activated!" ) );
          return;
        }
      }
      QString err = errorMsg.isEmpty() ? tr( "Could not activate free trial (Code %1)." ).arg( statusCode ) : errorMsg;
      callback( false, err );
    }
  );
}

void HakeLicenseManager::activateCode(
  const QString &code,
  const QString &email,
  std::function<void( bool success, const QString &message )> callback )
{
  QJsonObject payload;
  payload["code"] = code;
  payload["email"] = email;
  payload["hardware_id"] = hardwareId();
  payload["machine_name"] = machineName();
  payload["os_name"] = osName();
  payload["client_version"] = clientVersion();

  sendPostRequest( QStringLiteral( "/api/activate" ), payload,
    [this, callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 )
      {
        if ( saveLicenseToken( json ) )
        {
          callback( true, tr( "Hake GeoDesk activated successfully!" ) );
          return;
        }
      }
      QString err = errorMsg.isEmpty() ? tr( "License activation failed (Code %1)." ).arg( statusCode ) : errorMsg;
      callback( false, err );
    }
  );
}

void HakeLicenseManager::loginAccount(
  const QString &email,
  const QString &password,
  std::function<void( bool success, const QString &message, const QJsonObject &userData )> callback )
{
  QJsonObject payload;
  payload["email"] = email;
  payload["password"] = password;

  sendPostRequest( QStringLiteral( "/api/auth/login" ), payload,
    [callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 )
      {
        callback( true, tr( "Signed in successfully." ), json );
      }
      else
      {
        QString err = errorMsg.isEmpty() ? tr( "Sign in failed (Code %1)." ).arg( statusCode ) : errorMsg;
        callback( false, err, QJsonObject() );
      }
    }
  );
}

void HakeLicenseManager::registerAccount(
  const QString &fullName,
  const QString &email,
  const QString &password,
  std::function<void( bool success, const QString &message )> callback )
{
  QJsonObject payload;
  payload["full_name"] = fullName;
  payload["email"] = email;
  payload["password"] = password;

  sendPostRequest( QStringLiteral( "/api/auth/register" ), payload,
    [callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      Q_UNUSED( json );
      if ( statusCode == 200 || statusCode == 201 )
      {
        callback( true, tr( "Account created successfully!" ) );
      }
      else
      {
        QString err = errorMsg.isEmpty() ? tr( "Registration failed (Code %1)." ).arg( statusCode ) : errorMsg;
        callback( false, err );
      }
    }
  );
}

void HakeLicenseManager::refreshLicense( std::function<void( bool success, const QString &message )> callback )
{
  if ( mCurrentLicense.licenseId.isEmpty() )
  {
    if ( callback )
      callback( false, tr( "No active license to refresh." ) );
    return;
  }

  QJsonObject payload;
  payload["license_id"] = mCurrentLicense.licenseId;
  payload["hardware_id"] = hardwareId();
  payload["client_version"] = clientVersion();

  sendPostRequest( QStringLiteral( "/api/refresh" ), payload,
    [this, callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 && json.value( "success" ).toBool() )
      {
        // Update check-in anchor
        qint64 nowSecs = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        mLastCheckinTime = QDateTime::currentDateTimeUtc();
        QgsSettings settings;
        settings.setValue( SETTINGS_LAST_CHECKIN_KEY, nowSecs );

        if ( saveLicenseToken( json, mCurrentLicense.customerName ) )
        {
          if ( callback )
            callback( true, tr( "License successfully refreshed." ) );
          return;
        }
      }

      // Handle license revocation or suspension from server
      if ( statusCode == 403 || errorMsg.contains( "revoked", Qt::CaseInsensitive ) )
      {
        clearLicense();
        emit licenseRevoked( errorMsg );
        if ( callback )
          callback( false, errorMsg );
        return;
      }

      // Network unreachable / transient error: grace period keeps client active
      QString err = errorMsg.isEmpty() ? tr( "Check-in failed (Code %1)." ).arg( statusCode ) : errorMsg;
      if ( callback )
        callback( false, err );
    }
  );
}

void HakeLicenseManager::deactivateWorkstation( std::function<void( bool success, const QString &message )> callback )
{
  if ( mCurrentLicense.licenseId.isEmpty() )
  {
    callback( false, tr( "No active license found to deactivate." ) );
    return;
  }

  QJsonObject payload;
  payload["license_id"] = mCurrentLicense.licenseId;
  payload["hardware_id"] = hardwareId();
  payload["reason"] = QStringLiteral( "Client-initiated workstation deactivation" );

  sendPostRequest( QStringLiteral( "/api/deactivate" ), payload,
    [this, callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 && json.value( "success" ).toBool() )
      {
        clearLicense();
        callback( true, tr( "Workstation successfully deactivated. You can now activate your license on another device." ) );
      }
      else
      {
        QString err = errorMsg.isEmpty() ? tr( "Deactivation failed (Code %1)." ).arg( statusCode ) : errorMsg;
        callback( false, err );
      }
    }
  );
}

void HakeLicenseManager::purchasePlanOnline(
  const QString &planTier,
  const QString &email,
  const QString &fullName,
  std::function<void( bool success, const QString &message, const QString &activationCode )> callback )
{
  QJsonObject payload;
  payload["plan_tier"] = planTier;
  payload["email"] = email;
  payload["name"] = fullName;

  sendPostRequest( QStringLiteral( "/api/checkout/simulate" ), payload,
    [callback]( int statusCode, const QJsonObject &json, const QString &errorMsg ) {
      if ( statusCode == 200 && json.contains( "activation_code" ) )
      {
        QString code = json.value( "activation_code" ).toString();
        callback( true, tr( "Checkout completed! Your activation code has been generated and emailed." ), code );
      }
      else
      {
        QString err = errorMsg.isEmpty() ? tr( "Checkout failed (Code %1)." ).arg( statusCode ) : errorMsg;
        callback( false, err, QString() );
      }
    }
  );
}
