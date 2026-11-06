/***************************************************************************
 *  qgsmetasearchdialog.h
 *  -------------------
 *  MetaSearch Catalog Client Dialog for Hake GeoDesk
 ***************************************************************************/

#ifndef QGSMETASEARCHDIALOG_H
#define QGSMETASEARCHDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QList>

class QTabWidget;
class QComboBox;
class QLineEdit;
class QTextEdit;
class QTreeWidget;
class QSpinBox;
class QCheckBox;
class QPushButton;

class QgsMetaSearchDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit QgsMetaSearchDialog( QWidget *parent = nullptr );
    ~QgsMetaSearchDialog() override = default;

  private slots:
    void onAddDefaultServices();
    void onNewConnection();
    void onEditConnection();
    void onDeleteConnection();
    void onConnectionChanged( int index );
    void onSearch();
    void onSetGlobalExtent();
    void onHelp();

  private:
    struct ServiceEntry
    {
      QString name;
      QString type;
      QString url;
    };

    void setupUi();
    void updateServiceInfo();

    QTabWidget *mTabWidget = nullptr;

    // Search tab
    QLineEdit *mKeywordsEdit = nullptr;
    QComboBox *mSearchConnectionCombo = nullptr;
    QLineEdit *mXminEdit = nullptr;
    QLineEdit *mYminEdit = nullptr;
    QLineEdit *mXmaxEdit = nullptr;
    QLineEdit *mYmaxEdit = nullptr;
    QTreeWidget *mResultsTree = nullptr;

    // Services tab
    QComboBox *mServicesConnectionCombo = nullptr;
    QTextEdit *mServiceInfoText = nullptr;
    QPushButton *mBtnServiceInfo = nullptr;
    QPushButton *mBtnRawResponse = nullptr;
    QPushButton *mBtnAddDefault = nullptr;
    QPushButton *mBtnNew = nullptr;
    QPushButton *mBtnEdit = nullptr;
    QPushButton *mBtnDelete = nullptr;
    QPushButton *mBtnLoad = nullptr;
    QPushButton *mBtnSave = nullptr;

    // Settings tab
    QCheckBox *mSslCheck = nullptr;
    QSpinBox *mTimeoutSpin = nullptr;
    QSpinBox *mPagingSpin = nullptr;
    QCheckBox *mDebugCheck = nullptr;

    QList<ServiceEntry> mServices;
};

#endif // QGSMETASEARCHDIALOG_H

