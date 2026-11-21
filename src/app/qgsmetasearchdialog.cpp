/***************************************************************************
 *  qgsmetasearchdialog.cpp
 *  -------------------
 *  MetaSearch Catalog Client Dialog for Hake GeoDesk
 ***************************************************************************/

#include "qgsmetasearchdialog.h"
#include "qgshakeicons.h"
#include "qgsapplication.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_qgsmetasearchdialog.cpp"

using namespace Qt::StringLiterals;

QgsMetaSearchDialog::QgsMetaSearchDialog( QWidget *parent )
  : QDialog( parent )
{
  setWindowTitle( tr( "MetaSearch" ) );
  setWindowIcon( QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s ) );
  resize( 658, 550 );

  setupUi();
}

void QgsMetaSearchDialog::setupUi()
{
  QVBoxLayout *mainLayout = new QVBoxLayout( this );

  mTabWidget = new QTabWidget( this );

  // -------------------------------------------------------------
  // TAB 1: Search
  // -------------------------------------------------------------
  QWidget *searchTab = new QWidget( this );
  QVBoxLayout *searchLayout = new QVBoxLayout( searchTab );

  QGroupBox *findGroup = new QGroupBox( tr( "Find" ), searchTab );
  QGridLayout *findLayout = new QGridLayout( findGroup );

  findLayout->addWidget( new QLabel( tr( "Keywords" ), findGroup ), 0, 0 );
  mKeywordsEdit = new QLineEdit( findGroup );
  mKeywordsEdit->setPlaceholderText( tr( "Search terms…" ) );
  findLayout->addWidget( mKeywordsEdit, 0, 1, 1, 3 );

  findLayout->addWidget( new QLabel( tr( "From" ), findGroup ), 1, 0 );
  mSearchConnectionCombo = new QComboBox( findGroup );
  mSearchConnectionCombo->addItem( tr( "All connections" ) );
  findLayout->addWidget( mSearchConnectionCombo, 1, 1, 1, 3 );

  QGroupBox *extentGroup = new QGroupBox( tr( "Map extent" ), findGroup );
  QGridLayout *extentLayout = new QGridLayout( extentGroup );

  mXminEdit = new QLineEdit( u"-180"_s, extentGroup );
  mYminEdit = new QLineEdit( u"-90"_s, extentGroup );
  mXmaxEdit = new QLineEdit( u"180"_s, extentGroup );
  mYmaxEdit = new QLineEdit( u"90"_s, extentGroup );

  extentLayout->addWidget( new QLabel( tr( "Xmin" ), extentGroup ), 0, 0 );
  extentLayout->addWidget( mXminEdit, 0, 1 );
  extentLayout->addWidget( new QLabel( tr( "Ymin" ), extentGroup ), 0, 2 );
  extentLayout->addWidget( mYminEdit, 0, 3 );
  extentLayout->addWidget( new QLabel( tr( "Xmax" ), extentGroup ), 1, 0 );
  extentLayout->addWidget( mXmaxEdit, 1, 1 );
  extentLayout->addWidget( new QLabel( tr( "Ymax" ), extentGroup ), 1, 2 );
  extentLayout->addWidget( mYmaxEdit, 1, 3 );

  QPushButton *btnGlobal = new QPushButton( tr( "Set global" ), extentGroup );
  connect( btnGlobal, &QPushButton::clicked, this, &QgsMetaSearchDialog::onSetGlobalExtent );
  extentLayout->addWidget( btnGlobal, 2, 0, 1, 4 );

  findLayout->addWidget( extentGroup, 2, 0, 1, 4 );

  QPushButton *btnSearch = new QPushButton( tr( "Search" ), findGroup );
  btnSearch->setDefault( true );
  connect( btnSearch, &QPushButton::clicked, this, &QgsMetaSearchDialog::onSearch );
  findLayout->addWidget( btnSearch, 3, 3 );

  searchLayout->addWidget( findGroup );

  QGroupBox *resultsGroup = new QGroupBox( tr( "Results" ), searchTab );
  QVBoxLayout *resultsLayout = new QVBoxLayout( resultsGroup );

  mResultsTree = new QTreeWidget( resultsGroup );
  mResultsTree->setHeaderLabels( { tr( "Type" ), tr( "Title" ), tr( "Identifier" ) } );
  mResultsTree->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
  resultsLayout->addWidget( mResultsTree );

  searchLayout->addWidget( resultsGroup );
  mTabWidget->addTab( searchTab, tr( "Search" ) );

  // -------------------------------------------------------------
  // TAB 2: Services (Default active tab matching screenshot)
  // -------------------------------------------------------------
  QWidget *servicesTab = new QWidget( this );
  QVBoxLayout *servicesLayout = new QVBoxLayout( servicesTab );

  mServicesConnectionCombo = new QComboBox( servicesTab );
  connect( mServicesConnectionCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &QgsMetaSearchDialog::onConnectionChanged );
  servicesLayout->addWidget( mServicesConnectionCombo );

  // Row 1 of buttons
  QHBoxLayout *btnRow1 = new QHBoxLayout();
  mBtnServiceInfo = new QPushButton( tr( "Service Info" ), servicesTab );
  mBtnRawResponse = new QPushButton( tr( "Raw API Response" ), servicesTab );
  mBtnAddDefault = new QPushButton( tr( "Add Default Services" ), servicesTab );
  connect( mBtnAddDefault, &QPushButton::clicked, this, &QgsMetaSearchDialog::onAddDefaultServices );

  btnRow1->addWidget( mBtnServiceInfo );
  btnRow1->addWidget( mBtnRawResponse );
  btnRow1->addWidget( mBtnAddDefault );
  btnRow1->addStretch();
  servicesLayout->addLayout( btnRow1 );

  // Row 2 of buttons
  QHBoxLayout *btnRow2 = new QHBoxLayout();
  mBtnNew = new QPushButton( tr( "New…" ), servicesTab );
  mBtnEdit = new QPushButton( tr( "Edit…" ), servicesTab );
  mBtnDelete = new QPushButton( tr( "Delete…" ), servicesTab );
  mBtnLoad = new QPushButton( tr( "Load…" ), servicesTab );
  mBtnSave = new QPushButton( tr( "Save…" ), servicesTab );

  connect( mBtnNew, &QPushButton::clicked, this, &QgsMetaSearchDialog::onNewConnection );
  connect( mBtnEdit, &QPushButton::clicked, this, &QgsMetaSearchDialog::onEditConnection );
  connect( mBtnDelete, &QPushButton::clicked, this, &QgsMetaSearchDialog::onDeleteConnection );

  btnRow2->addWidget( mBtnNew );
  btnRow2->addWidget( mBtnEdit );
  btnRow2->addWidget( mBtnDelete );
  btnRow2->addWidget( mBtnLoad );
  btnRow2->addWidget( mBtnSave );
  btnRow2->addStretch();
  servicesLayout->addLayout( btnRow2 );

  // Service Info Text area
  mServiceInfoText = new QTextEdit( servicesTab );
  mServiceInfoText->setReadOnly( true );
  mServiceInfoText->setPlainText(
    tr( "No services/connections defined. To get started with MetaSearch, create a new connection by clicking 'New' or click 'Add default services'." )
  );
  servicesLayout->addWidget( mServiceInfoText );

  mTabWidget->addTab( servicesTab, tr( "Services" ) );

  // -------------------------------------------------------------
  // TAB 3: Settings
  // -------------------------------------------------------------
  QWidget *settingsTab = new QWidget( this );
  QVBoxLayout *settingsLayout = new QVBoxLayout( settingsTab );

  QGroupBox *serverGroup = new QGroupBox( tr( "Server" ), settingsTab );
  QVBoxLayout *serverLayout = new QVBoxLayout( serverGroup );

  mSslCheck = new QCheckBox( tr( "Disable SSL verification" ), serverGroup );
  serverLayout->addWidget( mSslCheck );

  QHBoxLayout *timeoutLayout = new QHBoxLayout();
  timeoutLayout->addWidget( new QLabel( tr( "Timeout" ), serverGroup ) );
  mTimeoutSpin = new QSpinBox( serverGroup );
  mTimeoutSpin->setRange( 1, 300 );
  mTimeoutSpin->setValue( 10 );
  timeoutLayout->addWidget( mTimeoutSpin );
  timeoutLayout->addWidget( new QLabel( tr( "seconds" ), serverGroup ) );
  timeoutLayout->addStretch();
  serverLayout->addLayout( timeoutLayout );

  settingsLayout->addWidget( serverGroup );

  QGroupBox *pagingGroup = new QGroupBox( tr( "Results Paging" ), settingsTab );
  QHBoxLayout *pagingLayout = new QHBoxLayout( pagingGroup );
  pagingLayout->addWidget( new QLabel( tr( "Show" ), pagingGroup ) );
  mPagingSpin = new QSpinBox( pagingGroup );
  mPagingSpin->setRange( 1, 100 );
  mPagingSpin->setValue( 10 );
  pagingLayout->addWidget( mPagingSpin );
  pagingLayout->addWidget( new QLabel( tr( "results at a time" ), pagingGroup ) );
  pagingLayout->addStretch();
  settingsLayout->addWidget( pagingGroup );

  QGroupBox *debugGroup = new QGroupBox( tr( "Debugging" ), settingsTab );
  QVBoxLayout *debugLayout = new QVBoxLayout( debugGroup );
  mDebugCheck = new QCheckBox( tr( "Log debugging messages" ), debugGroup );
  debugLayout->addWidget( mDebugCheck );
  settingsLayout->addWidget( debugGroup );

  settingsLayout->addStretch();
  mTabWidget->addTab( settingsTab, tr( "Settings" ) );

  // Select Services tab by default matching reference
  mTabWidget->setCurrentIndex( 1 );
  mainLayout->addWidget( mTabWidget );

  // Bottom action buttons
  QHBoxLayout *bottomLayout = new QHBoxLayout();
  QPushButton *btnHelp = new QPushButton( tr( "Help" ), this );
  connect( btnHelp, &QPushButton::clicked, this, &QgsMetaSearchDialog::onHelp );
  bottomLayout->addWidget( btnHelp );

  bottomLayout->addStretch();

  QPushButton *btnClose = new QPushButton( tr( "Close" ), this );
  connect( btnClose, &QPushButton::clicked, this, &QDialog::accept );
  bottomLayout->addWidget( btnClose );

  mainLayout->addLayout( bottomLayout );
}

void QgsMetaSearchDialog::onAddDefaultServices()
{
  mServices.clear();
  mServices.append( { u"Danmark: National CSW (geodata-info)"_s, u"OGC CSW 2.0.2"_s, u"https://geodata-info.dk/srv/dan/csw"_s } );
  mServices.append( { u"Finland: National CSW (Paikkatietohakemisto)"_s, u"OGC CSW 2.0.2"_s, u"http://www.paikkatietohakemisto.fi/geonetwork/srv/fi/csw"_s } );
  mServices.append( { u"Iceland: National CSW (Iceland Service)"_s, u"OGC CSW 2.0.2"_s, u"https://gatt.lmi.is/geonetwork/srv/eng/csw"_s } );
  mServices.append( { u"Italy: National CSW (Geoportale Nazionale)"_s, u"OGC CSW 2.0.2"_s, u"http://www.pcn.minambiente.it/geoportal/csw"_s } );
  mServices.append( { u"New Zealand: LINZ Data Service"_s, u"OGC CSW 2.0.2"_s, u"https://data.linz.govt.nz/services/csw/"_s } );
  mServices.append( { u"Norway: National CSW (Geonorge)"_s, u"OGC CSW 2.0.2"_s, u"http://www.geonorge.no/geonetwork/srv/no/csw"_s } );
  mServices.append( { u"Sweden: National CSW"_s, u"OGC CSW 2.0.2"_s, u"https://www.geodata.se/geodataportalen/srv/eng/csw-inspire"_s } );
  mServices.append( { u"UK Location Catalogue Publishing Service"_s, u"OGC CSW 2.0.2"_s, u"https://data.gov.uk/csw"_s } );
  mServices.append( { u"UNEP GRID-Geneva Metadata Catalog"_s, u"OGC CSW 2.0.2"_s, u"https://datacore-gn.unepgrid.ch/geonetwork/srv/eng/csw"_s } );
  mServices.append( { u"Canada: Federal Geospatial Platform"_s, u"OGC CSW 2.0.2"_s, u"https://maps.canada.ca/geonetwork/srv/csw"_s } );
  mServices.append( { u"WIS2 Global Discovery Catalogue"_s, u"OGC API - Records"_s, u"https://wis2-gdc.weather.gc.ca/collections/wis2-discovery-metadata"_s } );

  mServicesConnectionCombo->clear();
  mSearchConnectionCombo->clear();
  mSearchConnectionCombo->addItem( tr( "All connections" ) );

  for ( const ServiceEntry &svc : std::as_const( mServices ) )
  {
    mServicesConnectionCombo->addItem( svc.name );
    mSearchConnectionCombo->addItem( svc.name );
  }

  updateServiceInfo();
}

void QgsMetaSearchDialog::onConnectionChanged( int index )
{
  Q_UNUSED( index );
  updateServiceInfo();
}

void QgsMetaSearchDialog::updateServiceInfo()
{
  const int idx = mServicesConnectionCombo->currentIndex();
  if ( idx >= 0 && idx < mServices.size() )
  {
    const ServiceEntry &svc = mServices.at( idx );
    mServiceInfoText->setHtml(
      u"<h3>%1</h3>"
      "<p><b>Type:</b> %2</p>"
      "<p><b>URL:</b> <a href=\"%3\">%3</a></p>"
      "<p><b>Status:</b> Ready</p>"_s
      .arg( svc.name, svc.type, svc.url )
    );
  }
  else
  {
    mServiceInfoText->setPlainText(
      tr( "No services/connections defined. To get started with MetaSearch, create a new connection by clicking 'New' or click 'Add default services'." )
    );
  }
}

void QgsMetaSearchDialog::onNewConnection()
{
  bool ok = false;
  const QString name = QInputDialog::getText( this, tr( "New Service Connection" ), tr( "Service Name:" ), QLineEdit::Normal, QString(), &ok );
  if ( !ok || name.trimmed().isEmpty() )
    return;

  const QString url = QInputDialog::getText( this, tr( "New Service Connection" ), tr( "Service URL:" ), QLineEdit::Normal, QString(), &ok );
  if ( !ok || url.trimmed().isEmpty() )
    return;

  mServices.append( { name.trimmed(), u"OGC CSW 2.0.2"_s, url.trimmed() } );
  mServicesConnectionCombo->addItem( name.trimmed() );
  mSearchConnectionCombo->addItem( name.trimmed() );
  mServicesConnectionCombo->setCurrentIndex( mServices.size() - 1 );
}

void QgsMetaSearchDialog::onEditConnection()
{
  const int idx = mServicesConnectionCombo->currentIndex();
  if ( idx < 0 || idx >= mServices.size() )
    return;

  bool ok = false;
  ServiceEntry svc = mServices.at( idx );
  const QString name = QInputDialog::getText( this, tr( "Edit Connection" ), tr( "Name:" ), QLineEdit::Normal, svc.name, &ok );
  if ( !ok || name.trimmed().isEmpty() )
    return;

  const QString url = QInputDialog::getText( this, tr( "Edit Connection" ), tr( "URL:" ), QLineEdit::Normal, svc.url, &ok );
  if ( !ok || url.trimmed().isEmpty() )
    return;

  svc.name = name.trimmed();
  svc.url = url.trimmed();
  mServices[idx] = svc;
  mServicesConnectionCombo->setItemText( idx, svc.name );
  updateServiceInfo();
}

void QgsMetaSearchDialog::onDeleteConnection()
{
  const int idx = mServicesConnectionCombo->currentIndex();
  if ( idx < 0 || idx >= mServices.size() )
    return;

  mServices.removeAt( idx );
  mServicesConnectionCombo->removeItem( idx );
  mSearchConnectionCombo->removeItem( idx + 1 );
  updateServiceInfo();
}

void QgsMetaSearchDialog::onSetGlobalExtent()
{
  mXminEdit->setText( u"-180"_s );
  mYminEdit->setText( u"-90"_s );
  mXmaxEdit->setText( u"180"_s );
  mYmaxEdit->setText( u"90"_s );
}

void QgsMetaSearchDialog::onSearch()
{
  mResultsTree->clear();
  const QString kw = mKeywordsEdit->text().trimmed();

  if ( mServices.isEmpty() )
  {
    QMessageBox::information( this, tr( "MetaSearch" ), tr( "No catalog services are configured. Please go to the Services tab and click 'Add Default Services' or add a service connection first." ) );
    mTabWidget->setCurrentIndex( 1 );
    return;
  }

  // Populate sample search results
  QTreeWidgetItem *item1 = new QTreeWidgetItem( mResultsTree, { u"dataset"_s, tr( "Global Land Cover & Elevation Map %1" ).arg( kw.isEmpty() ? QString() : u"(%1)"_s.arg( kw ) ), u"urn:ogc:def:crs:EPSG::4326"_s } );
  QTreeWidgetItem *item2 = new QTreeWidgetItem( mResultsTree, { u"service"_s, tr( "Global Vector Features & Boundaries %1" ).arg( kw.isEmpty() ? QString() : u"(%1)"_s.arg( kw ) ), u"urn:x-ogc:specification:csw:2.0.2"_s } );
  mResultsTree->addTopLevelItem( item1 );
  mResultsTree->addTopLevelItem( item2 );
}

void QgsMetaSearchDialog::onHelp()
{
  QDesktopServices::openUrl( QUrl( u"https://docs.qgis.org/latest/en/docs/user_manual/plugins/core_plugins/plugins_metasearch.html"_s ) );
}

