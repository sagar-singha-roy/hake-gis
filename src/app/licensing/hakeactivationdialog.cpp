#include "hakeactivationdialog.h"
#include "ui_hakeactivationdialogbase.h"
#include "hakelicensemanager.h"
#include "hakeglobewidget.h"

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

  // Embed the animated 3D Earth vector widget inside the welcome hero panel
  mGlobeWidget = new HakeGlobeWidget( mUi->globeContainer );
  mUi->globeContainerLayout->addWidget( mGlobeWidget );

  // Load and display official Hake Technologies logo on welcome header and global top bar
  QPixmap logoPixmap( QStringLiteral( ":/images/icons/hake_logo.png" ) );
  if ( logoPixmap.isNull() )
  {
    logoPixmap.load( QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/images/icons/hake_logo.png" ) );
  }
  if ( !logoPixmap.isNull() )
  {
    mUi->lblHeroBrandIcon->setPixmap( logoPixmap.scaled( 28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblHeroBrandIcon->setText( QString() );
    mUi->lblHeaderLogo->setPixmap( logoPixmap.scaled( 26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblHeaderLogo->setText( QString() );
  }

  // Display workstation hardware id in UI header and activate form
  QString hwId = HakeLicenseManager::instance()->hardwareId();
  QString shortHw = hwId.left( 8 ) + QStringLiteral( "..." );
  mUi->lblWorkstationId->setText( tr( "Workstation: %1" ).arg( shortHw ) );
  mUi->lblActivateHwId->setText( hwId );

  // Setup interactive animated blue-and-white luminous shimmer effect on brand label "Hake Technologies"
  mUi->lblBrandHakeTech->installEventFilter( this );
  mUi->lblBrandHakeTech->setMouseTracking( true );
  mRainbowTimer = new QTimer( this );
  mRainbowTimer->setInterval( 40 ); // Smooth wave animation
  connect( mRainbowTimer, &QTimer::timeout, this, &HakeActivationDialog::onRainbowTimerTick );
  mUi->lblBrandHakeTech->setText( QStringLiteral( "<span style='font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif; font-size:17px; font-weight:800; letter-spacing:0.4px;'><span style='color:#0f2b66;'>Hake</span> <span style='color:#2563eb;'>Technologies</span></span>" ) );

  // Card selection event filters on Pricing page (recursive on child labels so whole card is clickable)
  auto makeCardClickable = [this]( QWidget *card ) {
    card->installEventFilter( this );
    const auto children = card->findChildren<QWidget *>();
    for ( QWidget *child : children )
    {
      if ( !qobject_cast<QPushButton *>( child ) )
      {
        child->setAttribute( Qt::WA_TransparentForMouseEvents, true );
      }
    }
  };

  makeCardClickable( mUi->cardTrial );
  makeCardClickable( mUi->cardAnnual );
  makeCardClickable( mUi->cardBusiness );

  setupConnections();

  // Clear inputs - do not prefill anything; user will give input fresh
  mUi->txtLoginEmail->clear();
  mUi->txtLoginPassword->clear();
  mUi->lblLoginSubtitle->setText( tr( "Enter your credentials to access your workstation." ) );

  // Initialize selected plan styling
  selectPlan( 1 );

  // Navigation logic:
  // If launching from app startup (requireStartupLogin=true), start on Welcome split screen
  if ( mRequireStartupLogin )
  {
    setPage( PageWelcome );
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
      mUi->lblBrandHakeTech->setText( QStringLiteral( "<span style='font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif; font-size:17px; font-weight:800; letter-spacing:0.4px;'><span style='color:#0f2b66;'>Hake</span> <span style='color:#2563eb;'>Technologies</span></span>" ) );
    }
  }
  else if ( event->type() == QEvent::MouseButtonPress )
  {
    if ( watched == mUi->cardTrial )
    {
      selectPlan( 0 );
      return true;
    }
    else if ( watched == mUi->cardAnnual )
    {
      selectPlan( 1 );
      return true;
    }
    else if ( watched == mUi->cardBusiness )
    {
      selectPlan( 2 );
      return true;
    }
  }
  return QDialog::eventFilter( watched, event );
}

void HakeActivationDialog::selectPlan( int planIndex )
{
  mSelectedPlanIndex = planIndex;

  const QString unselectedCardStyle = QStringLiteral(
    "QFrame#%1 {"
    "  background-color: #ffffff;"
    "  border: 1.5px solid #e2e8f0;"
    "  border-radius: 16px;"
    "  padding: 24px 20px;"
    "}"
    "QFrame#%1:hover {"
    "  border: 1.5px solid #94a3b8;"
    "  background-color: #f8fafc;"
    "}"
  );

  const QString selectedCardStyle = QStringLiteral(
    "QFrame#%1 {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #eff6ff);"
    "  border: 2.5px solid #2563eb;"
    "  border-radius: 16px;"
    "  padding: 23px 19px;"
    "}"
  );

  mUi->cardTrial->setStyleSheet( ( planIndex == 0 ? selectedCardStyle : unselectedCardStyle ).arg( QStringLiteral( "cardTrial" ) ) );
  mUi->cardAnnual->setStyleSheet( ( planIndex == 1 ? selectedCardStyle : unselectedCardStyle ).arg( QStringLiteral( "cardAnnual" ) ) );
  mUi->cardBusiness->setStyleSheet( ( planIndex == 2 ? selectedCardStyle : unselectedCardStyle ).arg( QStringLiteral( "cardBusiness" ) ) );

  const QString primaryBtnStyle = QStringLiteral(
    "QPushButton {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2563eb, stop:1 #1d4ed8);"
    "  color: #ffffff;"
    "  font-weight: 700;"
    "  font-size: 13.5px;"
    "  border-radius: 9px;"
    "  border: 1px solid #1e40af;"
    "  border-bottom: 3.5px solid #172554;"
    "  padding: 10px 16px;"
    "}"
    "QPushButton:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);"
    "  border-bottom: 3.5px solid #1e3a8a;"
    "}"
    "QPushButton:pressed {"
    "  background: #1d4ed8;"
    "  border-bottom: 1px solid #172554;"
    "  padding-top: 12px;"
    "  padding-bottom: 8px;"
    "}"
  );

  const QString secondaryBtnStyle = QStringLiteral(
    "QPushButton {"
    "  background-color: #0f172a;"
    "  color: #ffffff;"
    "  font-weight: 700;"
    "  font-size: 13.5px;"
    "  border-radius: 9px;"
    "  border: 1px solid #0f172a;"
    "  border-bottom: 2.5px solid #020617;"
    "  padding: 10px 16px;"
    "}"
    "QPushButton:hover {"
    "  background-color: #1e293b;"
    "  border-color: #1e293b;"
    "  border-bottom: 2.5px solid #0f172a;"
    "}"
    "QPushButton:pressed {"
    "  background-color: #020617;"
    "  border-bottom: 1px solid #020617;"
    "  padding-top: 11px;"
    "  padding-bottom: 8px;"
    "}"
  );

  mUi->btnSelectTrialPlan->setStyleSheet( planIndex == 0 ? primaryBtnStyle : secondaryBtnStyle );
  mUi->btnSelectAnnualPlan->setStyleSheet( planIndex == 1 ? primaryBtnStyle : secondaryBtnStyle );
  mUi->btnSelectBusinessPlan->setStyleSheet( planIndex == 2 ? primaryBtnStyle : secondaryBtnStyle );
}

void HakeActivationDialog::onRainbowTimerTick()
{
  mRainbowHue = ( mRainbowHue + 1 ) % 60;
  const QString brand = QStringLiteral( "Hake Technologies" );
  QString html = QStringLiteral( "<span style='font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif; font-size:17px; font-weight:800; letter-spacing:0.4px;'>" );

  // Curated blue-and-white luminous shimmer palette
  static const QVector<QColor> blueWhitePalette = {
    QColor( 15, 43, 102 ),   // Deep sapphire #0f2b66
    QColor( 30, 64, 175 ),   // Royal blue #1e40af
    QColor( 37, 99, 235 ),   // Vibrant blue #2563eb
    QColor( 2, 132, 199 ),   // Sky blue #0284c7
    QColor( 56, 189, 248 ),  // Light cyan #38bdf8
    QColor( 186, 230, 253 ), // Ice white-blue #bae6fd
    QColor( 255, 255, 255 ), // Pure white #ffffff
    QColor( 186, 230, 253 ), // Ice white-blue #bae6fd
    QColor( 56, 189, 248 ),  // Light cyan #38bdf8
    QColor( 37, 99, 235 ),   // Vibrant blue #2563eb
    QColor( 30, 64, 175 )    // Royal blue #1e40af
  };

  for ( int i = 0; i < brand.length(); ++i )
  {
    QChar c = brand.at( i );
    if ( c.isSpace() )
    {
      html += QStringLiteral( "&nbsp;" );
    }
    else
    {
      int colorIdx = ( ( mRainbowHue / 2 ) + i * 2 ) % blueWhitePalette.size();
      QColor col = blueWhitePalette.at( colorIdx );
      html += QStringLiteral( "<span style='color:%1;'>%2</span>" ).arg( col.name(), c );
    }
  }
  html += QStringLiteral( "</span>" );
  mUi->lblBrandHakeTech->setText( html );
}

void HakeActivationDialog::setPage( Page page )
{
  hideErrors();

  if ( page == PageLogin )
  {
    page = PageWelcome;
  }

  // On welcome split-screen and pricing page, hide the top global header bar for an edge-to-edge clean modern look
  if ( page == PageWelcome || page == PagePricing )
  {
    mUi->headerBrandBar->hide();
    if ( mGlobeWidget )
    {
      if ( page == PageWelcome )
        mGlobeWidget->startAnimation();
      else
        mGlobeWidget->stopAnimation();
    }
  }
  else
  {
    mUi->headerBrandBar->show();
    if ( mGlobeWidget )
    {
      mGlobeWidget->stopAnimation();
    }
  }

  mUi->stackedWidget->setCurrentIndex( static_cast<int>( page ) );
}

void HakeActivationDialog::setupConnections()
{
  // Navigation: Welcome Page (Split-screen)
  connect( mUi->btnLoginGoSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onGoSignUpClicked );
  connect( mUi->btnGoActivate, &QPushButton::clicked, this, &HakeActivationDialog::onGoActivateClicked );
  connect( mUi->btnGoPricing, &QPushButton::clicked, this, &HakeActivationDialog::onGoPricingClicked );
  connect( mUi->btnForgotPassword, &QPushButton::clicked, this, &HakeActivationDialog::onForgotPasswordClicked );
  connect( mUi->btnTerms, &QPushButton::clicked, this, &HakeActivationDialog::onTermsClicked );
  connect( mUi->btnPrivacy, &QPushButton::clicked, this, &HakeActivationDialog::onPrivacyClicked );
  connect( mUi->btnSupport, &QPushButton::clicked, this, &HakeActivationDialog::onSupportClicked );

  // Navigation: Back Buttons
  connect( mUi->btnBackFromTrial, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );
  connect( mUi->btnBackFromSignUp, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );
  connect( mUi->btnBackFromPricing, &QPushButton::clicked, this, &HakeActivationDialog::onBackFromPricingClicked );
  connect( mUi->btnBackFromActivate, &QPushButton::clicked, this, &HakeActivationDialog::onBackToWelcomeClicked );

  // Navigation: Inter-page shortcuts from Pricing
  connect( mUi->btnSelectTrialPlan, &QPushButton::clicked, this, [this]() {
    selectPlan( 0 );
    onStartTrialClicked();
  } );
  connect( mUi->btnSelectAnnualPlan, &QPushButton::clicked, this, [this]() {
    selectPlan( 1 );
    onGoActivateClicked();
  } );
  connect( mUi->btnSelectBusinessPlan, &QPushButton::clicked, this, [this]() {
    selectPlan( 2 );
    onGoActivateClicked();
  } );

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
  if ( !mCurrentAuthenticatedEmail.isEmpty() )
  {
    mUi->txtTrialEmail->setText( mCurrentAuthenticatedEmail );
    mUi->txtTrialName->setText( mCurrentAuthenticatedName );
    if ( !mCurrentAuthenticatedPassword.isEmpty() )
    {
      mUi->txtTrialPassword->setText( mCurrentAuthenticatedPassword );
    }
  }
  setPage( PageTrial );
}

void HakeActivationDialog::onGoActivateClicked()
{
  if ( !mCurrentAuthenticatedEmail.isEmpty() )
  {
    mUi->txtActivateEmail->setText( mCurrentAuthenticatedEmail );
  }
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
  selectPlan( mSelectedPlanIndex );
  setPage( PagePricing );
}

void HakeActivationDialog::onBackToWelcomeClicked()
{
  if ( !mCurrentAuthenticatedEmail.isEmpty() )
  {
    setPage( PagePricing );
    return;
  }
  setPage( PageWelcome );
}

void HakeActivationDialog::onBackFromPricingClicked()
{
  mCurrentAuthenticatedEmail.clear();
  mCurrentAuthenticatedName.clear();
  mCurrentAuthenticatedPassword.clear();
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
    case PageWelcome:
    case PageLogin:
      target = mUi->lblLoginError;
      mUi->lblLoginError->setStyleSheet( QStringLiteral( "background: #fef2f2; color: #b91c1c; border: 1px solid #fecaca; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500;" ) );
      break;
    case PageTrial:
      target = mUi->lblTrialError;
      break;
    case PageSignUp:
      target = mUi->lblSignUpError;
      break;
    case PageActivate:
      target = mUi->lblActivateError;
      mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #fef2f2; color: #b91c1c; border: 1px solid #fecaca; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500;" ) );
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

  if ( email.isEmpty() )
  {
    showError( PageWelcome, tr( "Please enter your Login ID." ) );
    return;
  }
  if ( password.isEmpty() )
  {
    showError( PageWelcome, tr( "Please enter your password." ) );
    return;
  }

  setButtonsBusy( true );
  hideErrors();

  HakeLicenseManager::instance()->loginAccount( email, password,
    [this, email, password]( bool success, const QString &message, const QJsonObject &data ) {
      setButtonsBusy( false );
      if ( success )
      {
        mCurrentAuthenticatedEmail = email;
        mCurrentAuthenticatedPassword = password;
        QString fullName = data.value( QStringLiteral( "full_name" ) ).toString();
        mCurrentAuthenticatedName = fullName.isEmpty() ? email : fullName;

        mUi->txtActivateEmail->setText( email );
        mUi->txtTrialEmail->setText( email );
        mUi->txtTrialPassword->setText( password );
        mUi->txtTrialName->setText( mCurrentAuthenticatedName );

        // Strict License Ownership Check:
        // Does the active workstation license token actually match THIS authenticated user?
        QString licenseEmail = HakeLicenseManager::instance()->currentLicense().customerEmail.trimmed().toLower();
        bool hasValidLicenseForUser = HakeLicenseManager::instance()->isLicenseValidOffline()
            && ( !licenseEmail.isEmpty() && licenseEmail == email.trimmed().toLower() );

        if ( hasValidLicenseForUser )
        {
          // Valid workstation license confirmed for this user: directly launch GeoDesk!
          accept();
        }
        else
        {
          // First-time or unlicensed user on this workstation: must activate with code first!
          setPage( PageActivate );
          mUi->lblActivateError->setText( tr( "First-time workstation setup: Please enter your 16-character activation code to unlock Hake GeoDesk." ) );
          mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #eff6ff; color: #1d4ed8; border: 1px solid #bfdbfe; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500;" ) );
          mUi->lblActivateError->show();
        }
      }
      else
      {
        showError( PageWelcome, message );
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
        mCurrentAuthenticatedEmail = email;
        mCurrentAuthenticatedName = name;
        mCurrentAuthenticatedPassword = pass;

        mUi->txtTrialName->setText( name );
        mUi->txtTrialEmail->setText( email );
        mUi->txtTrialPassword->setText( pass );
        mUi->txtActivateEmail->setText( email );
        mUi->txtLoginEmail->setText( email );
        mUi->txtLoginPassword->setText( pass );

        // Direct newly registered first-time user immediately to Activation Code screen
        setPage( PageActivate );
        mUi->lblActivateError->setText( tr( "Account created successfully! As a first-time user, please enter your 16-character activation code to unlock Hake GeoDesk." ) );
        mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #eff6ff; color: #1d4ed8; border: 1px solid #bfdbfe; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500;" ) );
        mUi->lblActivateError->show();
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

  QString email = mCurrentAuthenticatedEmail;
  if ( email.isEmpty() )
    email = mUi->txtActivateEmail->text().trimmed();
  if ( email.isEmpty() )
    email = mUi->txtLoginEmail->text().trimmed();
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

  QString email = mCurrentAuthenticatedEmail;
  if ( email.isEmpty() )
    email = mUi->txtActivateEmail->text().trimmed();
  if ( email.isEmpty() )
    email = mUi->txtLoginEmail->text().trimmed();
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

void HakeActivationDialog::onForgotPasswordClicked()
{
  QString serverUrl = Qgis::licenseServerUrl().trimmed();
  while ( serverUrl.endsWith( QLatin1Char( '/' ) ) )
    serverUrl.chop( 1 );

  if ( !serverUrl.isEmpty() )
  {
    QDesktopServices::openUrl( QUrl( QStringLiteral( "%1/admin/login/" ).arg( serverUrl ) ) );
  }
  else
  {
    QMessageBox::information( this, tr( "Forgot Password" ),
      tr( "Please contact your license administrator or support@haketech.com to reset your account password." ) );
  }
}

void HakeActivationDialog::onTermsClicked()
{
  QString serverUrl = Qgis::licenseServerUrl().trimmed();
  while ( serverUrl.endsWith( QLatin1Char( '/' ) ) )
    serverUrl.chop( 1 );
  QDesktopServices::openUrl( QUrl( QStringLiteral( "%1/terms" ).arg( serverUrl ) ) );
}

void HakeActivationDialog::onPrivacyClicked()
{
  QString serverUrl = Qgis::licenseServerUrl().trimmed();
  while ( serverUrl.endsWith( QLatin1Char( '/' ) ) )
    serverUrl.chop( 1 );
  QDesktopServices::openUrl( QUrl( QStringLiteral( "%1/privacy" ).arg( serverUrl ) ) );
}

void HakeActivationDialog::onSupportClicked()
{
  QDesktopServices::openUrl( QUrl( QStringLiteral( "mailto:support@haketech.com?subject=Hake%20GeoDesk%20Support" ) ) );
}
