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
#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QGraphicsDropShadowEffect>

HakeActivationDialog::HakeActivationDialog( QWidget *parent, bool requireStartupLogin )
  : QDialog( parent )
  , mUi( std::make_unique<Ui::HakeActivationDialogBase>() )
  , mRequireStartupLogin( requireStartupLogin )
{
  // Register Google Fonts: IBM Plex Serif (SemiBold 600) and Open Sans (Regular 400, SemiBold 600, Bold 700)
  const QStringList fontSearchPaths = {
    QDir::homePath() + QStringLiteral( "/Library/Fonts/IBMPlexSerif-SemiBold.ttf" ),
    QDir::homePath() + QStringLiteral( "/Library/Fonts/IBMPlexSerif-Regular.ttf" ),
    QDir::homePath() + QStringLiteral( "/Library/Fonts/IBMPlexSerif-Bold.ttf" ),
    QDir::homePath() + QStringLiteral( "/Library/Fonts/OpenSans-Regular.ttf" ),
    QDir::homePath() + QStringLiteral( "/Library/Fonts/OpenSans-SemiBold.ttf" ),
    QDir::homePath() + QStringLiteral( "/Library/Fonts/OpenSans-Bold.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/IBMPlexSerif-SemiBold.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/IBMPlexSerif-Regular.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/IBMPlexSerif-Bold.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/OpenSans-Regular.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/OpenSans-SemiBold.ttf" ),
    QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/src/app/licensing/fonts/OpenSans-Bold.ttf" )
  };
  for ( const QString &fontFile : fontSearchPaths )
  {
    if ( QFile::exists( fontFile ) )
    {
      QFontDatabase::addApplicationFont( fontFile );
    }
  }

  mUi->setupUi( this );
  setModal( true );

  // Embed the animated 3D Earth vector widget inside the welcome hero panel
  mGlobeWidget = new HakeGlobeWidget( mUi->globeContainer );
  mUi->globeContainerLayout->addWidget( mGlobeWidget );

  // Place "Welcome to Hake GeoDesk (Desktop)" in the middle of the globe image
  QVBoxLayout *globeCenterLayout = new QVBoxLayout( mGlobeWidget );
  globeCenterLayout->setContentsMargins( 12, 12, 12, 12 );
  globeCenterLayout->addWidget( mUi->lblHeroTitle, 0, Qt::AlignCenter );

  // Classy cinematic Gaussian drop shadow behind the title
  auto *titleShadow = new QGraphicsDropShadowEffect( this );
  titleShadow->setBlurRadius( 28 );
  titleShadow->setColor( QColor( 2, 6, 23, 235 ) );
  titleShadow->setOffset( 0, 3 );
  mUi->lblHeroTitle->setGraphicsEffect( titleShadow );

  // Load and display official Hake Technologies logo (with embedded high-res Base64 guarantee)
  static const char sHakeLogoBase64[] =
    "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";

  QPixmap logoPixmap;
  logoPixmap.loadFromData( QByteArray::fromBase64( QByteArray( sHakeLogoBase64 ) ), "PNG" );
  if ( logoPixmap.isNull() )
  {
    logoPixmap.load( QStringLiteral( ":/images/icons/hake_logo.png" ) );
  }
  if ( logoPixmap.isNull() )
  {
    logoPixmap.load( QStringLiteral( "/Users/piyakshi/Desktop/Hake Gis Licensing/hake-gis/images/icons/hake_logo.png" ) );
  }
  if ( !logoPixmap.isNull() )
  {
    mUi->lblHeroBrandIcon->setPixmap( logoPixmap.scaled( 34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblHeroBrandIcon->setText( QString() );
    mUi->lblHeaderLogo->setPixmap( logoPixmap.scaled( 32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblHeaderLogo->setText( QString() );
    mUi->lblActivateCardLogo->setPixmap( logoPixmap.scaled( 50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblActivateCardLogo->setText( QString() );
    mUi->lblSignUpCardLogo->setPixmap( logoPixmap.scaled( 48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    mUi->lblSignUpCardLogo->setText( QString() );
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
    "  font-family: 'Open Sans', -apple-system, sans-serif;"
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
    "  font-family: 'Open Sans', -apple-system, sans-serif;"
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
      mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #fef2f2; color: #b91c1c; border: 1px solid #fecaca; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500; font-family: 'Open Sans', -apple-system, sans-serif;" ) );
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
          mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #eff6ff; color: #1d4ed8; border: 1px solid #bfdbfe; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500; font-family: 'Open Sans', -apple-system, sans-serif;" ) );
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
        mUi->lblActivateError->setStyleSheet( QStringLiteral( "background: #eff6ff; color: #1d4ed8; border: 1px solid #bfdbfe; border-radius: 8px; padding: 8px 12px; font-size: 11.5px; font-weight: 500; font-family: 'Open Sans', -apple-system, sans-serif;" ) );
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
