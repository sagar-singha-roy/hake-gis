#ifndef HAKE_ACTIVATION_DIALOG_H
#define HAKE_ACTIVATION_DIALOG_H

#include <QDialog>
#include <memory>
#include "qgis_app.h"

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

  explicit HakeActivationDialog( QWidget *parent = nullptr );
  ~HakeActivationDialog() override;

  //! Set active page directly
  void setPage( Page page );

private slots:
  // Navigation
  void onStartTrialClicked();
  void onGoActivateClicked();
  void onGoLoginClicked();
  void onGoSignUpClicked();
  void onGoPricingClicked();
  void onBackToWelcomeClicked();

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

  // Formatting
  void onActivationCodeEdited( const QString &text );

private:
  void setupConnections();
  void refreshDashboardView();
  void showError( Page page, const QString &errorText );
  void hideErrors();
  void setButtonsBusy( bool busy );

  std::unique_ptr<Ui::HakeActivationDialogBase> mUi;
};

#endif // HAKE_ACTIVATION_DIALOG_H
