#ifndef HAKE_LICENSE_MANAGER_H
#define HAKE_LICENSE_MANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <functional>
#include "qgis_app.h"

/**
 * \brief Client-side licensing and subscription manager for Hake GeoDesk Desktop GIS.
 *
 * Enforces offline signature verification, hardware binding, clock integrity checks,
 * encrypted token storage via QgsAuthManager, and asynchronous activation calls.
 */
class APP_EXPORT HakeLicenseManager : public QObject
{
  Q_OBJECT

public:
  struct LicenseInfo
  {
    QString licenseId;
    QString customerEmail;
    QString customerName;
    QString planTier; // "trial", "annual", "business"
    QString hardwareId;
    QDateTime issuedAt;
    QDateTime expiresAt;
    QString rawTokenBase64;
    QString signatureBase64;
    bool isValid = false;
    QString statusString;
  };

  //! Returns singleton instance
  static HakeLicenseManager *instance();

  //! Client hardware fingerprint (SHA-256 of system unique id + salt)
  QString hardwareId() const;

  //! Hostname of current workstation
  QString machineName() const;

  //! Operating system description
  QString osName() const;

  //! Product version label
  QString clientVersion() const;

  /**
   * \brief Verifies the locally cached license token offline.
   * Checks Ed25519 digital signature, hardware binding, expiration date, and clock rollback.
   * \return true if a valid, unexpired license is active; false otherwise.
   */
  bool isLicenseValidOffline();

  //! Returns information about the currently active license
  LicenseInfo currentLicense() const;

  //! Returns remaining days of subscription or trial (0 if expired)
  int remainingDays() const;

  //! True if license is active but expires in 7 days or less
  bool isExpiringSoon() const;

  /**
   * \brief Remaining days in the 14-day offline grace period (Phase 3).
   * Returns <= 0 if the 14-day offline grace period has expired.
   */
  int offlineGraceDaysRemaining() const;

  //! True if running offline for more than 24h and currently within the 14-day grace window
  bool isGracePeriodActive() const;

  //! Clear active license token and sign out
  void clearLicense();

  //! Persist a newly received server token
  bool saveLicenseToken( const QJsonObject &responseObject, const QString &userName = QString() );

  // -------------------------------------------------------------------------
  // Asynchronous Backend API Calls
  // -------------------------------------------------------------------------

  /**
   * \brief Starts a server-managed 7-day free trial bound to this workstation.
   */
  void startTrial( const QString &fullName,
                   const QString &email,
                   const QString &password,
                   std::function<void( bool success, const QString &message )> callback );

  /**
   * \brief Activates a 16-character Crockford Base32 license code.
   */
  void activateCode( const QString &code,
                     const QString &email,
                     std::function<void( bool success, const QString &message )> callback );

  /**
   * \brief Signs in to an existing Hake GeoDesk account.
   */
  void loginAccount( const QString &email,
                     const QString &password,
                     std::function<void( bool success, const QString &message, const QJsonObject &userData )> callback );

  /**
   * \brief Registers a new user account.
   */
  void registerAccount( const QString &fullName,
                        const QString &email,
                        const QString &password,
                        std::function<void( bool success, const QString &message )> callback );

  /**
   * \brief Performs periodic check-in with the license server (Phase 3).
   * Verifies status, updates check-in timestamp, and refreshes signed token.
   */
  void refreshLicense( std::function<void( bool success, const QString &message )> callback = nullptr );

  /**
   * \brief Deactivates this workstation, freeing the license seat for migration to another PC (Phase 3).
   */
  void deactivateWorkstation( std::function<void( bool success, const QString &message )> callback );

  /**
   * \brief Online checkout simulator / trigger for Annual Pro and Business Team plans (Phase 4).
   */
  void purchasePlanOnline( const QString &planTier,
                           const QString &email,
                           const QString &fullName,
                           std::function<void( bool success, const QString &message, const QString &activationCode )> callback );

signals:
  void licenseStatusChanged( bool isValid, const QString &planTier );
  void licenseRevoked( const QString &message );

private:
  explicit HakeLicenseManager( QObject *parent = nullptr );
  ~HakeLicenseManager() override = default;

  bool loadCachedToken();
  bool verifyTokenPayload( const QByteArray &canonicalJsonBytes, const QByteArray &signatureBytes );
  void sendPostRequest( const QString &endpointPath,
                        const QJsonObject &payload,
                        std::function<void( int statusCode, const QJsonObject &responseJson, const QString &errorString )> callback );
  void setupRefreshTimer();

  mutable QString mHardwareId;
  LicenseInfo mCurrentLicense;
  QDateTime mLastCheckinTime;
  class QTimer *mRefreshTimer = nullptr;
  bool mInitialized = false;
};

#endif // HAKE_LICENSE_MANAGER_H
