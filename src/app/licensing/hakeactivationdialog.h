#ifndef HAKE_ACTIVATION_DIALOG_H
#define HAKE_ACTIVATION_DIALOG_H

#include <QDialog>
#include <memory>
#include "qgis_app.h"

class QTimer;
class HakeGlobeWidget;

namespace Ui
{
  class HakeActivationDialogBase;
}

/**
 * \brief Multi-page enterprise licensing and account management dialog for Hake GeoDesk.
 *
 * Implements:
 * - Welcome / Get Started portal
 * - 7-Day Server-Managed Free Trial
 * - Account Sign In & Registration
 * - Subscription & Pricing information
 * - 16-character Crockford Base32 license activation with live auto-formatting
 * - Interactive License & Account Dashboard
 * - Interactive Rainbow Color Wave Animation on "Hake Technologies"
 */
class APP_EXPORT HakeActivationDialog : public QDialog
{
  Q_OBJECT

public:
  enum Page
  {
    PageWelcome = 0,
    PageTrial = 1,
    PageLogin = 2,
    PageSignUp = 3,
    PagePricing = 4,
    PageActivate = 5,
    PageDashboard = 6
  };

  explicit HakeActivationDialog( QWidget *parent = nullptr, bool requireStartupLogin = false );
  ~HakeActivationDialog() override;

  //! Set active page directly
  void setPage( Page page );

protected:
  bool eventFilter( QObject *watched, QEvent *event ) override;

private slots:
  // Navigation
  void onStartTrialClicked();
  void onGoActivateClicked();
  void onGoLoginClicked();
  void onGoSignUpClicked();
  void onGoPricingClicked();
  void onBackToWelcomeClicked();
  void onBackFromPricingClicked();

  // Actions
  void onSubmitTrial();
  void onSubmitActivate();
  void onSubmitLogin();
  void onSubmitSignUp();
  void onLaunchApp();
  void onSignOut();
  void onRefreshLicenseClicked();
  void onDeactivateWorkstationClicked();
  void onBuyAnnualClicked();
  void onBuyBusinessClicked();
  void onForgotPasswordClicked();
  void onTermsClicked();
  void onPrivacyClicked();
  void onSupportClicked();

  // Formatting & Animation
  void onActivationCodeEdited( const QString &text );
  void onRainbowTimerTick();

private:
  void setupConnections();
  void refreshDashboardView();
  void showError( Page page, const QString &errorText );
  void hideErrors();
  void setButtonsBusy( bool busy );
  void selectPlan( int planIndex );

  std::unique_ptr<Ui::HakeActivationDialogBase> mUi;
  bool mRequireStartupLogin = false;
  QTimer *mRainbowTimer = nullptr;
  int mRainbowHue = 0;
  HakeGlobeWidget *mGlobeWidget = nullptr;
  int mSelectedPlanIndex = 1;
  QString mCurrentAuthenticatedEmail;
  QString mCurrentAuthenticatedName;
  QString mCurrentAuthenticatedPassword;
};

#endif // HAKE_ACTIVATION_DIALOG_H
