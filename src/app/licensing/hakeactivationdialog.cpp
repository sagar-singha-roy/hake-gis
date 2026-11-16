#include "hakeactivationdialog.h"
#include "ui_hakeactivationdialogbase.h"
#include "hakelicensemanager.h"

#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QColor>

HakeActivationDialog::HakeActivationDialog( QWidget *parent, bool requireStartupLogin )
  : QDialog( parent )
  , mUi( std::make_unique<Ui::HakeActivationDialogBase>() )
  , mRequireStartupLogin( requireStartupLogin )
{
  mUi->setupUi( this );
  setModal( true );

  // Display workstation hardware id in UI header and activate form
  QString hwId = HakeLicenseManager::instance()->hardwareId();
  QString shortHw = hwId.left( 8 ) + QStringLiteral( "..." );
  mUi->lblWorkstationId->setText( tr( "Workstation: %1" ).arg( shortHw ) );
  mUi->lblActivateHwId->setText( hwId );

  // Setup interactive animated rainbow effect on brand label "Hake Technologies"
  mUi->lblBrandHakeTech->installEventFilter( this );
  mUi->lblBrandHakeTech->setMouseTracking( true );
  mRainbowTimer = new QTimer( this );
  mRainbowTimer->setInterval( 35 ); // ~30 FPS smooth wave
  connect( mRainbowTimer, &QTimer::timeout, this, &HakeActivationDialog::onRainbowTimerTick );
  mUi->lblBrandHakeTech->setText( QStringLiteral( "<span style='color:#0f172a; font-weight:800; font-size:17px; letter-spacing:0.5px;'>Hake Technologies</span>" ) );

  setupConnections();

  // Navigation logic:
  // If launching from app startup (requireStartupLogin=true), always require credentials
  if ( mRequireStartupLogin )
  {
    if ( HakeLicenseManager::instance()->isLicenseValidOffline() )
    {
      // Pre-fill email of active license and prompt for password
      QString activeEmail = HakeLicenseManager::instance()->currentLicense().customerEmail;
      mUi->txtLoginEmail->setText( activeEmail );
      mUi->txtLoginPassword->clear();
      mUi->txtLoginPassword->setFocus();
      mUi->lblLoginSubtitle->setText( tr( "Workstation License Active • Enter your password to unlock Hake GeoDesk." ) );
      setPage( PageLogin );
    }
    else
    {
      setPage( PageWelcome );
    }
  }
  else
  {
    // Opened from inside GeoDesk (Help -> License & Account)
    if ( HakeLicenseManager::instance()->isLicenseValidOffline() )
    {
      refreshDashboardView();
      setPage( PageDashboard );
    }
    else
    {
      setPage( PageWelcome );
    }
  }
}

HakeActivationDialog::~HakeActivationDialog() = default;

bool HakeActivationDialog::eventFilter( QObject *watched, QEvent *event )
{
  if ( watched == mUi->lblBrandHakeTech )
  {
    if ( event->type() == QEvent::Enter || event->type() == QEvent::MouseMove )
    {
      if ( !mRainbowTimer->isActive() )
      {
        mRainbowTimer->start();
      }
    }
    else if ( event->type() == QEvent::Leave )
    {
      mRainbowTimer->stop();
      mUi->lblBrandHakeTech->setText( QStringLiteral( "<span style='color:#0f172a; font-weight:800; font-size:17px; letter-spacing:0.5px;'>Hake Technologies</span>" ) );
    }
  }
  return QDialog::eventFilter( watched, event );
}

void HakeActivationDialog::onRainbowTimerTick()
{
  mRainbowHue = ( mRainbowHue + 7 ) % 360;
  const QString brand = QStringLiteral( "Hake Technologies" );
  QString html = QStringLiteral( "<span style='font-size:17px; font-weight:800; letter-spacing:0.5px;'>" );
  for ( int i = 0; i < brand.length(); ++i )
  {
    QChar c = brand.at( i );
    if ( c.isSpace() )
    {
      html += QStringLiteral( "&nbsp;" );
    }
    else
    {
      int hue = ( mRainbowHue + i * 22 ) % 360;
      QColor col = QColor::fromHsv( hue, 235, 240 );
      html += QStringLiteral( "<span style='color:%1;'>%2</span>" ).arg( col.name(), c );
    }
  }
  html += QStringLiteral( "</span>" );
  mUi->lblBrandHakeTech->setText( html );
}

void HakeActivationDialog::setPage( Page page )
{
  hideErrors();
  mUi->stackedWidget->setCurrentIndex( static_cast<int>( page ) );
}

void HakeActivationDialog::setupConnections()
{
  // Navigation: Welcome Page
  connect( mUi->btnStartTrial, &QPushButton::clicked, this, &HakeActivationDialog::onStartTrialClicked );
  connect( mUi->btnGoActivate, &QPushButton::clicked, this, &HakeActivationDialog::onGoActivateClicked );
  connect( mUi->btnGoLogin, &QPushButton::clicked, this, &HakeActivationDialog::onGoLoginClicked );
  connect( mUi->btnGoSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onGoSignUpClicked );
  connect( mUi->btnGoPricing, &QPushButton::clicked, this, &HakeActivationDialog::onGoPricingClicked );

  // Navigation: Back Buttons
  connect( mUi->btnBackFromTrial, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );
  connect( mUi->btnBackFromLogin, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );
  connect( mUi->btnBackFromSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onGoLoginClicked );
  connect( mUi->btnBackFromPricing, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );
  connect( mUi->btnBackFromActivate, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );

  // Navigation: Inter-page shortcuts
  connect( mUi->btnLoginGoSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onGoSignUpClicked );
  connect( mUi->btnSelectTrialPlan, &QPushButton::clicked, this, &HakeActivationDialog::onStartTrialClicked );
  connect( mUi->btnSelectAnnualPlan, &QPushButton::clicked, this, &HakeActivationDialog::onGoActivateClicked );
  connect( mUi->btnSelectBusinessPlan, &QPushButton::clicked, this, &HakeActivationDialog::onGoActivateClicked );

  // Actions
  connect( mUi->btnSubmitTrial, &QPushButton::clicked, this, &HakeActivationDialog::onSubmitTrial );
  connect( mUi->btnSubmitActivate, &QPushButton::clicked, this, &HakeActivationDialog::onSubmitActivate );
  connect( mUi->btnSubmitLogin, &QPushButton::clicked, this, &HakeActivationDialog::onSubmitLogin );
  connect( mUi->btnSubmitSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onSubmitSignUp );
  connect( mUi->btnLaunchApp, &QPushButton::clicked, this, &HakeActivationDialog::onLaunchApp );
  connect( mUi->btnDashLogout, &QPushButton::clicked, this, &HakeActivationDialog::onSignOut );
  connect( mUi->btnDashChangeKey, &QPushButton::clicked, this, &HakeActivationDialog::onGoActivateClicked );
  connect( mUi->btnDashRenew, &QPushButton::clicked, this, &HakeActivationDialog::onGoPricingClicked );
  connect( mUi->btnDashRefresh, &QPushButton::clicked, this, &HakeActivationDialog::onRefreshLicenseClicked );
  connect( mUi->btnDashDeactivate, &QPushButton::clicked, this, &HakeActivationDialog::onDeactivateWorkstationClicked );
  connect( mUi->btnBuyAnnual, &QPushButton::clicked, this, &HakeActivationDialog::onBuyAnnualClicked );
  connect( mUi->btnBuyBusiness, &QPushButton::clicked, this, &HakeActivationDialog::onBuyBusinessClicked );

  // Live Auto-formatting for 16-character code
  connect( mUi->txtActivationCode, &QLineEdit::textEdited, this, &HakeActivationDialog::onActivationCodeEdited );

  // Enter key shortcuts for fast keyboard navigation
  connect( mUi->txtLoginPassword, &QLineEdit::returnPressed, this, &HakeActivationDialog::onSubmitLogin );
  connect( mUi->txtLoginEmail, &QLineEdit::returnPressed, this, &HakeActivationDialog::onSubmitLogin );
  connect( mUi->txtActivationCode, &QLineEdit::returnPressed, this, &HakeActivationDialog::onSubmitActivate );
}

void HakeActivationDialog::onStartTrialClicked()
{
  setPage( PageTrial );
}

void HakeActivationDialog::onGoActivateClicked()
{
  setPage( PageActivate );
}

void HakeActivationDialog::onGoLoginClicked()
{
  setPage( PageLogin );
}

void HakeActivationDialog::onGoSignUpClicked()
{
  setPage( PageSignUp );
}

void HakeActivationDialog::onGoPricingClicked()
{
  setPage( PagePricing );
}

void HakeActivationDialog::onBackToWelcomeClicked()
{
  setPage( PageWelcome );
}

void HakeActivationDialog::hideErrors()
{
  mUi->lblTrialError->hide();
  mUi->lblLoginError->hide();
  mUi->lblSignUpError->hide();
  mUi->lblActivateError->hide();
}

void HakeActivationDialog::showError( Page page, const QString &errorText )
{
  hideErrors();
  QLabel *target = nullptr;
  switch ( page )
  {
    case PageTrial:
      target = mUi->lblTrialError;
      break;
    case PageLogin:
      target = mUi->lblLoginError;
      break;
    case PageSignUp:
      target = mUi->lblSignUpError;
      break;
    case PageActivate:
      target = mUi->lblActivateError;
      break;
    default:
      break;
  }

  if ( target )
  {
    target->setText( errorText );
    target->show();
  }
}

void HakeActivationDialog::setButtonsBusy( bool busy )
{
  mUi->btnSubmitTrial->setEnabled( !busy );
  mUi->btnSubmitActivate->setEnabled( !busy );
  mUi->btnSubmitLogin->setEnabled( !busy );
  mUi->btnSubmitSignUp->setEnabled( !busy );

  setCursor( busy ? Qt::WaitCursor : Qt::ArrowCursor );
}

void HakeActivationDialog::onActivationCodeEdited( const QString &text )
{
  QString cleaned;
  for ( const QChar &ch : text )
  {
    if ( ch.isLetterOrNumber() )
    {
      cleaned.append( ch.toUpper() );
    }
  }

  if ( cleaned.length() > 16 )
  {
    cleaned = cleaned.left( 16 );
  }

  QString formatted;
  for ( int i = 0; i < cleaned.length(); ++i )
  {
    if ( i > 0 && ( i % 4 == 0 ) )
    {
      formatted.append( '-' );
    }
    formatted.append( cleaned.at( i ) );
  }

  mUi->txtActivationCode->blockSignals( true );
  mUi->txtActivationCode->setText( formatted );
  mUi->txtActivationCode->blockSignals( false );
}

void HakeActivationDialog::onSubmitTrial()
{
  QString name = mUi->txtTrialName->text().trimmed();
  QString email = mUi->txtTrialEmail->text().trimmed();
  QString password = mUi->txtTrialPassword->text();

  if ( name.isEmpty() )
  {
    showError( PageTrial, tr( "Please enter your full name." ) );
    return;
  }
  if ( email.isEmpty() || !email.contains( '@' ) )
  {
    showError( PageTrial, tr( "Please enter a valid email address." ) );
    return;
  }
  if ( password.length() < 8 )
  {
    showError( PageTrial, tr( "Password must be at least 8 characters long." ) );
    return;
  }

  setButtonsBusy( true );
  hideErrors();

  HakeLicenseManager::instance()->startTrial( name, email, password,
    [this]( bool success, const QString &message ) {
      setButtonsBusy( false );
      if ( success )
      {
        refreshDashboardView();
        setPage( PageDashboard );
      }
      else
      {
        showError( PageTrial, message );
      }
    }
  );
}

void HakeActivationDialog::onSubmitActivate()
{
  QString code = mUi->txtActivationCode->text().trimmed();
  QString email = mUi->txtActivateEmail->text().trimmed();

  if ( code.isEmpty() )
  {
    showError( PageActivate, tr( "Please enter your 16-character activation code." ) );
    return;
  }

  setButtonsBusy( true );
  hideErrors();

  HakeLicenseManager::instance()->activateCode( code, email,
    [this]( bool success, const QString &message ) {
      setButtonsBusy( false );
      if ( success )
      {
        refreshDashboardView();
        setPage( PageDashboard );
      }
      else
      {
        showError( PageActivate, message );
      }
    }
  );
}

void HakeActivationDialog::onSubmitLogin()
{
  QString email = mUi->txtLoginEmail->text().trimmed();
  QString password = mUi->txtLoginPassword->text();

  if ( email.isEmpty() || !email.contains( '@' ) )
  {
    showError( PageLogin, tr( "Please enter your registered email address." ) );
    return;
  }
  if ( password.isEmpty() )
  {
    showError( PageLogin, tr( "Please enter your password." ) );
    return;
  }

  setButtonsBusy( true );
  hideErrors();

  HakeLicenseManager::instance()->loginAccount( email, password,
    [this, email]( bool success, const QString &message, const QJsonObject &data ) {
      Q_UNUSED( data );
      setButtonsBusy( false );
      if ( success )
      {
        mUi->txtActivateEmail->setText( email );
        if ( HakeLicenseManager::instance()->isLicenseValidOffline() )
        {
          // Valid workstation license confirmed: directly launch GeoDesk!
          accept();
        }
        else
        {
          // User account authenticated, proceed to activation code entry
          setPage( PageActivate );
        }
      }
      else
      {
        showError( PageLogin, message );
      }
    }
  );
}

void HakeActivationDialog::onSubmitSignUp()
{
  QString name = mUi->txtSignUpName->text().trimmed();
  QString email = mUi->txtSignUpEmail->text().trimmed();
  QString pass = mUi->txtSignUpPassword->text();
  QString confirm = mUi->txtSignUpConfirm->text();

  if ( name.isEmpty() )
  {
    showError( PageSignUp, tr( "Please enter your full name." ) );
    return;
  }
  if ( email.isEmpty() || !email.contains( '@' ) )
  {
    showError( PageSignUp, tr( "Please enter a valid email address." ) );
    return;
  }
  if ( pass.length() < 8 )
  {
    showError( PageSignUp, tr( "Password must be at least 8 characters long." ) );
    return;
  }
  if ( pass != confirm )
  {
    showError( PageSignUp, tr( "Passwords do not match." ) );
    return;
  }
  if ( !mUi->chkAcceptTerms->isChecked() )
  {
    showError( PageSignUp, tr( "Please accept the Terms of Service to continue." ) );
    return;
  }

  setButtonsBusy( true );
  hideErrors();

  HakeLicenseManager::instance()->registerAccount( name, email, pass,
    [this, name, email, pass]( bool success, const QString &message ) {
      setButtonsBusy( false );
      if ( success )
      {
        mUi->txtTrialName->setText( name );
        mUi->txtTrialEmail->setText( email );
        mUi->txtTrialPassword->setText( pass );
        mUi->txtActivateEmail->setText( email );
        mUi->txtLoginEmail->setText( email );
        setPage( PageLogin );
      }
      else
      {
        showError( PageSignUp, message );
      }
    }
  );
}

void HakeActivationDialog::refreshDashboardView()
{
  HakeLicenseManager::LicenseInfo info = HakeLicenseManager::instance()->currentLicense();

  QString customerStr = info.customerName;
  if ( !info.customerEmail.isEmpty() && info.customerEmail != info.customerName )
  {
    customerStr += QStringLiteral( " (%1)" ).arg( info.customerEmail );
  }
  mUi->lblValCustomer->setText( customerStr.isEmpty() ? tr( "Active User" ) : customerStr );

  QString planDisplay = info.planTier.toUpper();
  if ( info.planTier == QLatin1String( "trial" ) )
    planDisplay = tr( "7-Day Free Trial (Full Access)" );
  else if ( info.planTier == QLatin1String( "annual" ) )
    planDisplay = tr( "Annual Pro Workstation License" );
  else if ( info.planTier == QLatin1String( "business" ) )
    planDisplay = tr( "Business Team Enterprise License" );

  mUi->lblValPlan->setText( planDisplay );
  mUi->lblValExpires->setText( info.expiresAt.isValid() ? info.expiresAt.toString( Qt::ISODate ) : tr( "Perpetual" ) );

  int days = HakeLicenseManager::instance()->remainingDays();
  mUi->lblValRemaining->setText( tr( "%1 days remaining" ).arg( days ) );

  mUi->lblValDevice->setText( HakeLicenseManager::instance()->machineName() );

  if ( info.isValid )
  {
    mUi->lblDashStatusBadge->setText( tr( "ACTIVE" ) );
    mUi->lblDashStatusBadge->setStyleSheet( QStringLiteral( "background: #ecfdf5; color: #059669; border: 1px solid #a7f3d0; border-radius: 12px; padding: 4px 14px; font-weight: bold; font-size: 12px;" ) );

    // Display grace period warning if offline for > 24 hours
    if ( HakeLicenseManager::instance()->isGracePeriodActive() )
    {
      int graceRemaining = HakeLicenseManager::instance()->offlineGraceDaysRemaining();
      mUi->lblGraceWarning->setText( tr( "⚠️ Offline Grace Active: %1 days remaining before online check-in is required." ).arg( graceRemaining ) );
      mUi->lblGraceWarning->show();
    }
    else
    {
      mUi->lblGraceWarning->hide();
    }
  }
  else
  {
    mUi->lblDashStatusBadge->setText( tr( "EXPIRED" ) );
    mUi->lblDashStatusBadge->setStyleSheet( QStringLiteral( "background: #fef2f2; color: #dc2626; border: 1px solid #fecaca; border-radius: 12px; padding: 4px 14px; font-weight: bold; font-size: 12px;" ) );
    mUi->lblGraceWarning->hide();
  }
}

void HakeActivationDialog::onRefreshLicenseClicked()
{
  mUi->btnDashRefresh->setEnabled( false );
  mUi->btnDashRefresh->setText( tr( "Checking in..." ) );

  HakeLicenseManager::instance()->refreshLicense( [this]( bool success, const QString &message ) {
    mUi->btnDashRefresh->setEnabled( true );
    mUi->btnDashRefresh->setText( tr( "↻ Check-in" ) );

    if ( success )
    {
      refreshDashboardView();
      QMessageBox::information( this, tr( "License Check-in" ), tr( "Workstation successfully verified with the license server." ) );
    }
    else
    {
      refreshDashboardView();
      QMessageBox::warning( this, tr( "Check-in Status" ), message );
    }
  } );
}

void HakeActivationDialog::onDeactivateWorkstationClicked()
{
  QMessageBox::StandardButton reply = QMessageBox::question(
    this,
    tr( "Deactivate Workstation" ),
    tr( "Are you sure you want to deactivate Hake GeoDesk on this computer?\n\n"
        "This will unbind your license seat from this workstation and allow you to activate it on another device." ),
    QMessageBox::Yes | QMessageBox::No,
    QMessageBox::No
  );

  if ( reply == QMessageBox::Yes )
  {
    HakeLicenseManager::instance()->deactivateWorkstation( [this]( bool success, const QString &message ) {
      if ( success )
      {
        QMessageBox::information( this, tr( "Deactivation Successful" ), message );
        setPage( PageWelcome );
      }
      else
      {
        QMessageBox::warning( this, tr( "Deactivation Failed" ), message );
      }
    } );
  }
}

void HakeActivationDialog::onBuyAnnualClicked()
{
  QString serverUrl = Qgis::licenseServerUrl().trimmed();
  while ( serverUrl.endsWith( QLatin1Char( '/' ) ) )
    serverUrl.chop( 1 );

  QString email = mUi->txtLoginEmail->text().trimmed();
  if ( email.isEmpty() )
    email = HakeLicenseManager::instance()->currentLicense().customerEmail;

  QString urlStr = QStringLiteral( "%1/checkout?plan=annual" ).arg( serverUrl );
  if ( !email.isEmpty() )
    urlStr += QStringLiteral( "&email=" ) + QString::fromLatin1( QUrl::toPercentEncoding( email ) );

  QDesktopServices::openUrl( QUrl( urlStr ) );
  setPage( PageActivate );
}

void HakeActivationDialog::onBuyBusinessClicked()
{
  QString serverUrl = Qgis::licenseServerUrl().trimmed();
  while ( serverUrl.endsWith( QLatin1Char( '/' ) ) )
    serverUrl.chop( 1 );

  QString email = mUi->txtLoginEmail->text().trimmed();
  if ( email.isEmpty() )
    email = HakeLicenseManager::instance()->currentLicense().customerEmail;

  QString urlStr = QStringLiteral( "%1/checkout?plan=business" ).arg( serverUrl );
  if ( !email.isEmpty() )
    urlStr += QStringLiteral( "&email=" ) + QString::fromLatin1( QUrl::toPercentEncoding( email ) );

  QDesktopServices::openUrl( QUrl( urlStr ) );
  setPage( PageActivate );
}

void HakeActivationDialog::onLaunchApp()
{
  accept(); // Closes dialog with QDialog::Accepted to launch GeoDesk
}

void HakeActivationDialog::onSignOut()
{
  HakeLicenseManager::instance()->clearLicense();
  setPage( PageWelcome );
}
