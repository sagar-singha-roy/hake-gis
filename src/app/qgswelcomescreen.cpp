/***************************************************************************
                             qgswelcomescreen.cpp
                             -------------------
    begin                : December 2025
    copyright            : (C) 2025 by Mathieu Pellerin
    email                : mathieu at opengis dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgswelcomescreen.h"

#include "qgis.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgshakeicons.h"
#include "qgshaketheme.h"
#include "qgshelp.h"
#include "qgsmessagelog.h"
#include "qgspluginmanager.h"
#include "qgssettings.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingstree.h"

#include <QAbstractButton>
#include <QColor>
#include <QMessageBox>
#include <QQmlContext>
#include <QQmlError>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickImageProvider>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_qgswelcomescreen.cpp"

using namespace Qt::StringLiterals;

#define FEED_URL "https://haketech.com/feed/"

namespace
{
  /**
   * Serves Hake icons to the Welcome Screen QML as image://hakeicon/<key>/<revision>.
   * The key is a QgsHakeIcons::iconFor() key; the revision only defeats image caching across theme switches.
   */
  class QgsHakeIconImageProvider : public QQuickImageProvider
  {
    public:
      QgsHakeIconImageProvider()
        : QQuickImageProvider( QQuickImageProvider::Pixmap )
      {}

      QPixmap requestPixmap( const QString &id, QSize *size, const QSize &requestedSize ) override
      {
        const QSize pixmapSize = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize( 24, 24 );
        if ( size )
          *size = pixmapSize;
        return QgsHakeIcons::iconFor( id.section( '/', 0, 0 ), QString() ).pixmap( pixmapSize );
      }
  };
} // namespace


QgsWelcomeScreenController::QgsWelcomeScreenController( QgsWelcomeScreen *welcomeScreen )
  : QObject( welcomeScreen )
  , mWelcomeScreen( welcomeScreen )
{}

void QgsWelcomeScreenController::openProject( const QString &path )
{
  // QTimer needed to prevent crashes when the Item bound to the calling function is deleted by the ListView
  QTimer::singleShot( 1, this, [path]() { QgisApp::instance()->openProject( path ); } );
}

void QgsWelcomeScreenController::createBlankProject()
{
  // QTimer needed to prevent crashes when the Item bound to the calling function is deleted by the ListView
  QTimer::singleShot( 1, this, []() { QgisApp::instance()->newProject(); } );
}

void QgsWelcomeScreenController::createProjectFromBasemap()
{
  // QTimer needed to prevent crashes when the Item bound to the calling function is deleted by the ListView
  QTimer::singleShot( 1, this, []() { QgisApp::instance()->fileNewWithBasemap(); } );
}

void QgsWelcomeScreenController::createProjectFromTemplate( const QString &path )
{
  // QTimer needed to prevent crashes when the Item bound to the calling function is deleted by the ListView
  QTimer::singleShot( 1, this, [path]() { QgisApp::instance()->fileNewFromTemplate( path ); } );
}

void QgsWelcomeScreenController::clearRecentProjects()
{
  if ( mWelcomeScreen )
  {
    mWelcomeScreen->clearRecentProjects();
  }
}

void QgsWelcomeScreenController::removeTemplateProject( int row )
{
  if ( mWelcomeScreen )
  {
    mWelcomeScreen->removeTemplateProject( row );
  }
}

void QgsWelcomeScreenController::showPluginManager()
{
  QgisApp::instance()->showPluginManager( static_cast<int>( QgsPluginManager::Tabs::UpgradeablePlugins ) );
}

void QgsWelcomeScreenController::hideScene()
{
  if ( mWelcomeScreen )
  {
    mWelcomeScreen->hideScene();
  }
}

void QgsWelcomeScreenController::openGettingStarted()
{
  QgsHelp::openHelp( u"introduction/getting_started.html"_s );
}

void QgsWelcomeScreenController::openProjectDialog()
{
  QTimer::singleShot( 1, this, []() { QgisApp::instance()->fileOpen(); } );
}

void QgsWelcomeScreenController::forwardDrop( const QString &text, const QStringList &urls, const QVariantMap &formatsData )
{
  QMimeData mimeData;
  const QStringList formats = formatsData.keys();
  for ( const QString &format : formats )
  {
    mimeData.setData( format, formatsData[format].toByteArray() );
  }

  QList<QUrl> mimeDataUrls;
  for ( const QString &url : urls )
  {
    mimeDataUrls << QUrl( url );
  }
  mimeData.setUrls( mimeDataUrls );
  mimeData.setText( text );

  QDropEvent dropEvent( QPointF( 0, 0 ), Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier );
  QgisApp::instance()->dropEvent( &dropEvent );
}


const QgsSettingsEntryBool *QgsWelcomeScreen::settingsCheckVersion
  = new QgsSettingsEntryBool( u"check-version"_s, QgsSettingsTree::sTreeApp, true, u"Whether the welcome screen should check for a newer Hake Geospatial version online"_s );

QgsWelcomeScreen::QgsWelcomeScreen( bool skipVersionCheck, QWidget *parent )
  : QQuickWidget( parent )
{
  setAttribute( Qt::WA_AlwaysStackOnTop );
  setAttribute( Qt::WA_TranslucentBackground );
  setClearColor( Qt::transparent );

  mRecentProjectsModel = new QgsRecentProjectItemsModel( this );
  connect( mRecentProjectsModel, &QgsRecentProjectItemsModel::projectPinned, this, &QgsWelcomeScreen::projectPinned );
  connect( mRecentProjectsModel, &QgsRecentProjectItemsModel::projectUnpinned, this, &QgsWelcomeScreen::projectUnpinned );
  connect( mRecentProjectsModel, &QgsRecentProjectItemsModel::projectRemoved, this, &QgsWelcomeScreen::projectRemoved );
  connect( mRecentProjectsModel, &QgsRecentProjectItemsModel::projectsCleared, this, &QgsWelcomeScreen::projectsCleared );

  mTemplateProjectsModel = new QgsTemplateProjectsModel( this );

  mNewsFeedParser = new QgsNewsFeedParser( QUrl( QStringLiteral( FEED_URL ) ), QString(), this );
  mNewsFeedModel = new QgsNewsFeedProxyModel( mNewsFeedParser, this );

  mWelcomeScreenController = new QgsWelcomeScreenController( this );

  rootContext()->setContextProperty( u"recentProjectsModel"_s, mRecentProjectsModel );
  rootContext()->setContextProperty( u"templateProjectsModel"_s, mTemplateProjectsModel );
  rootContext()->setContextProperty( u"newsFeedParser"_s, mNewsFeedParser );
  rootContext()->setContextProperty( u"newsFeedModel"_s, mNewsFeedModel );
  rootContext()->setContextProperty( u"welcomeScreenController"_s, mWelcomeScreenController );
  rootContext()->setContextProperty( u"productDisplayName"_s, Qgis::productDisplayName() );
  rootContext()->setContextProperty( u"appVersion"_s, Qgis::productVersionLabel() );

  engine()->addImageProvider( u"hakeicon"_s, new QgsHakeIconImageProvider() );

  // Registered before the (lazy) QML load; later theme changes update the values in place.
  mThemeColors = new QQmlPropertyMap( this );
  updateThemeColors();
  rootContext()->setContextProperty( u"welcomeTheme"_s, mThemeColors );
  connect( QgsApplication::instance(), &QgsApplication::themeChanged, this, &QgsWelcomeScreen::updateThemeColors );

  setResizeMode( QQuickWidget::ResizeMode::SizeRootObjectToView );

  if ( parent )
  {
    parent->installEventFilter( this );
  }

  QgsSettings settings;
  mVersionInfo = new QgsVersionInfo();
  if ( !QgsApplication::isRunningFromBuildDir() && settings.value( u"/qgis/allowVersionCheck"_s, true ).toBool() && settingsCheckVersion->value() && !skipVersionCheck )
  {
    connect( mVersionInfo, &QgsVersionInfo::versionInfoAvailable, this, &QgsWelcomeScreen::versionInfoReceived );
    mVersionInfo->checkVersion();
  }
}

bool QgsWelcomeScreen::eventFilter( QObject *object, QEvent *event )
{
  bool result = QWidget::eventFilter( object, event );

  if ( event->type() == QEvent::Resize )
  {
    if ( isVisible() && object == parent() )
    {
      refreshGeometry();
    }
  }

  return result;
}

void QgsWelcomeScreen::updateThemeColors()
{
  if ( !mThemeColors )
    return;

  struct ThemeColor
  {
      const char *name;
      const char *light;
      const char *night;
  };

  // Light values are the Welcome Screen's original colors and apply to every theme except Hake Night.
  // clang-format off
  static const ThemeColor colors[] = {
    { "workspaceColor", "#F7F9FB", "#0D141C" },
    { "pageColor", "#F1F6FA", "#121A24" },
    { "elevationColor", "#DCE6EE", "#0A0F15" },
    { "insetColor", "#F7FAFC", "#172231" },
    { "panelColor", "#FFFFFF", "#1A2533" },
    { "surfaceColor", "#EAF2F7", "#1F2C3C" },
    { "pressedSurfaceColor", "#D6E4F4", "#24496F" },
    { "primaryColor", "#164A73", "#2F72AE" },
    { "hoverColor", "#22658F", "#3A80BF" },
    { "activeColor", "#0E3858", "#245C8F" },
    { "accentColor", "#164A73", "#8EC5F5" },
    { "accentSoftColor", "#EAF2F7", "#22344A" },
    { "newsAccentColor", "#164A73", "#8EC5F5" },
    { "textColor", "#243B53", "#E6EDF5" },
    { "mutedTextColor", "#607D94", "#9FB3C8" },
    { "borderColor", "#C7D8E5", "#2A3B50" },
    { "surfaceBorderColor", "#AFC7D8", "#3F5A76" },
    { "statusColor", "#25875F", "#5CC79A" },
    { "onPrimaryTextColor", "#FFFFFF", "#FFFFFF" },
    { "projectCardColor", "#FFFFFF", "#1A2533" },
    { "projectTitleColor", "#2D3748", "#E6EDF5" },
    { "projectTextColor", "#4A5568", "#9FB3C8" },
    { "scrollBarColor", "#A7A7A7", "#4A5D73" },
  };
  // clang-format on

  const bool night = QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night;
  for ( const ThemeColor &color : colors )
  {
    const QColor value( QString::fromLatin1( night ? color.night : color.light ) );
    const QString key = QLatin1String( color.name );
    if ( mThemeColors->value( key ).value<QColor>() != value )
      mThemeColors->insert( key, value );
  }

  // hakeIcons switches the QML icons to image://hakeicon/; iconRevision forces them to reload per theme.
  mThemeColors->insert( u"hakeIcons"_s, QgsHakeIcons::isHakeTheme( QgsApplication::themeName() ) );
  mThemeColors->insert( u"iconRevision"_s, mThemeColors->value( u"iconRevision"_s ).toInt() + 1 );
}

void QgsWelcomeScreen::refreshGeometry()
{
  if ( QWidget *parentWidget = qobject_cast<QWidget *>( parent() ) )
  {
    // Fill the central widget so QML can paint the workspace tint ring
    // and a raised home surface; no inset against the map canvas.
    setGeometry( 0, 0, parentWidget->width(), parentWidget->height() );
  }
}

void QgsWelcomeScreen::showScene()
{
  if ( source().isEmpty() )
  {
    setSource( QUrl( "qrc:/qt/qml/org/hake/app/qml/WelcomeScreen.qml" ) );

    if ( status() == QQuickWidget::Error )
    {
      const QList<QQmlError> qmlErrors = errors();
      for ( const QQmlError &error : qmlErrors )
      {
        QgsMessageLog::logMessage( error.toString(), tr( "Welcome Screen" ), Qgis::MessageLevel::Critical );
      }
    }
  }
  refreshGeometry();
  show();
  raise();
}

void QgsWelcomeScreen::hideScene()
{
  if ( isVisible() )
  {
    hide();
  }
}

QString QgsWelcomeScreen::newsFeedUrl()
{
  return QStringLiteral( FEED_URL );
}

void QgsWelcomeScreen::registerTypes()
{
  qmlRegisterType<QgsTemplateProjectsModel>( "org.hake.app", 1, 0, "TemplateProjectsModel" );
  qmlRegisterType<QgsRecentProjectItemsModel>( "org.hake.app", 1, 0, "RecentProjectItemsModel" );
  qmlRegisterType<QgsNewsFeedModel>( "org.hake.app", 1, 0, "NewsFeedModel" );
}

void QgsWelcomeScreen::setRecentProjects( const QList<QgsRecentProjectItemsModel::RecentProjectData> &recentProjects )
{
  mRecentProjectsModel->setRecentProjects( recentProjects );
}

QgsRecentProjectItemsModel *QgsWelcomeScreen::recentProjectsModel()
{
  return mRecentProjectsModel;
}

QgsTemplateProjectsModel *QgsWelcomeScreen::templateProjectsModel()
{
  return mTemplateProjectsModel;
}

void QgsWelcomeScreen::clearRecentProjects()
{
  QMessageBox
    messageBox( QMessageBox::Question, tr( "Recent Projects" ), tr( "Are you sure you want to clear the list of recent projects?" ), QMessageBox::No | QMessageBox::Yes | QMessageBox::YesToAll, this );
  messageBox.button( QMessageBox::YesToAll )->setText( tr( "Yes, including pinned projects" ) );
  int answer = messageBox.exec();
  if ( answer != QMessageBox::No )
  {
    const bool clearPinned = ( answer == QMessageBox::YesToAll );
    mRecentProjectsModel->clear( clearPinned );
    emit projectsCleared( clearPinned );
  }
}

void QgsWelcomeScreen::removeTemplateProject( int row )
{
  if ( row < 0 || row >= mTemplateProjectsModel->rowCount() )
  {
    return;
  }

  QStandardItem *templateItem = mTemplateProjectsModel->item( row );
  const QFileInfo fileInfo( templateItem->data( static_cast<int>( QgsTemplateProjectsModel::CustomRole::NativePathRole ) ).toString() );
  if ( fileInfo.isWritable() )
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle( tr( "Delete Template" ) );
    msgBox.setText(
      tr( "Do you want to delete the template %1? This action can not be undone." ).arg( templateItem->data( static_cast<int>( QgsTemplateProjectsModel::CustomRole::TitleRole ) ).toString() )
    );
    auto deleteButton = msgBox.addButton( tr( "Delete" ), QMessageBox::YesRole );
    msgBox.addButton( QMessageBox::Cancel );
    msgBox.setIcon( QMessageBox::Question );
    msgBox.exec();
    if ( msgBox.clickedButton() == deleteButton )
    {
      mTemplateProjectsModel->removeRow( row );
      QFile file( fileInfo.filePath() );
      file.remove();
    }
  }
}

void QgsWelcomeScreen::versionInfoReceived()
{
  if ( !mWelcomeScreenController )
  {
    return;
  }

  QgsVersionInfo *versionInfo = qobject_cast<QgsVersionInfo *>( sender() );
  Q_ASSERT( versionInfo );

  if ( versionInfo->newVersionAvailable() )
  {
    QString latestVersion;
    const QString latestVersionCode = QString::number( versionInfo->latestVersionCode() );
    if ( latestVersionCode.size() >= 5 )
    {
      int major = latestVersionCode.mid( 0, latestVersionCode.size() - 4 ).toInt();
      int minor = latestVersionCode.mid( latestVersionCode.size() - 4, 2 ).toInt();
      int patch = latestVersionCode.mid( latestVersionCode.size() - 2, 2 ).toInt();

      latestVersion = u"%1.%2.%3"_s.arg( major ).arg( minor ).arg( patch );
    }
    emit mWelcomeScreenController->newVersionAvailable( latestVersion );
  }
}

void QgsWelcomeScreen::pluginUpdatesAvailableReceived( const QStringList &plugins )
{
  if ( !mWelcomeScreenController )
  {
    return;
  }

  emit mWelcomeScreenController->pluginUpdatesAvailable( plugins );
}
