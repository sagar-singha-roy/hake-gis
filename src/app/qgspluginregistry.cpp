/***************************************************************************
                    QgsPluginRegistry.cpp  -  Singleton class for
                    tracking registering plugins.
                             -------------------
    begin                : Fri Feb 7 2004
    copyright            : (C) 2004 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgspluginregistry.h"

#include "qgis.h"
#include "qgisapp.h"
#include "qgisinterface.h"
#include "qgisplugin.h"
#include "qgsapplication.h"
#include "qgslogger.h"
#include "qgsmessagebar.h"
#include "qgsmessagebaritem.h"
#include "qgsmessagelog.h"
#include "qgspluginmetadata.h"
#include "qgsruntimeprofiler.h"
#include "qgssettings.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLibrary>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QString>

using namespace Qt::StringLiterals;

#ifdef WITH_BINDINGS
#include "qgspythonutils.h"
#endif

/* typedefs for plugins */
typedef QgisPlugin *create_ui( QgisInterface *qI );
typedef const QString *name_t();
typedef const QString *description_t();
typedef const QString *category_t();
typedef int type_t();


QgsPluginRegistry *QgsPluginRegistry::sInstance = nullptr;
QgsPluginRegistry *QgsPluginRegistry::instance()
{
  if ( !sInstance )
  {
    sInstance = new QgsPluginRegistry();
  }
  return sInstance;
}

void QgsPluginRegistry::setQgisInterface( QgisInterface *iface )
{
  mQgisInterface = iface;
}

void QgsPluginRegistry::setPythonUtils( QgsPythonUtils *pythonUtils )
{
  mPythonUtils = pythonUtils;
}

bool QgsPluginRegistry::isLoaded( const QString &key ) const
{
  const QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.find( key );
  if ( it != mPlugins.end() ) // found a c++ plugin?
    return true;

  if ( mNativePythonPlugins.contains( key ) )
    return true;

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    return mPythonUtils->isPluginLoaded( key );
  }
#endif

  return false;
}

QString QgsPluginRegistry::library( const QString &key )
{
  const QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.constFind( key );
  if ( it != mPlugins.constEnd() )
    return it->library();

  if ( mNativePythonPlugins.contains( key ) )
    return key;

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    if ( mPythonUtils->isPluginLoaded( key ) )
      return key;
  }
#endif

  return QString();
}

QgisPlugin *QgsPluginRegistry::plugin( const QString &key )
{
  const QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.find( key );
  if ( it == mPlugins.end() )
    return nullptr;

  // note: not used by python plugins

  return it->plugin();
}

bool QgsPluginRegistry::isPythonPlugin( const QString &key ) const
{
  if ( mNativePythonPlugins.contains( key ) )
    return true;

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    if ( mPythonUtils->isPluginLoaded( key ) )
      return true;
  }
#else
  Q_UNUSED( key )
#endif
  return false;
}

void QgsPluginRegistry::addPlugin( const QString &key, const QgsPluginMetadata &metadata )
{
  mPlugins.insert( key, metadata );
}

void QgsPluginRegistry::dump()
{
  QgsDebugMsgLevel( u"PLUGINS IN REGISTRY: key -> (name, library)"_s, 1 );
  for ( QMap<QString, QgsPluginMetadata>::const_iterator it = mPlugins.constBegin(); it != mPlugins.constEnd(); ++it )
  {
    QgsDebugMsgLevel( u"PLUGIN: %1 -> (%2, %3)"_s.arg( it.key(), it->name(), it->library() ), 1 );
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    QgsDebugMsgLevel( u"PYTHON PLUGINS IN REGISTRY:"_s, 1 );
    const auto constListActivePlugins = mPythonUtils->listActivePlugins();
    for ( const QString &pluginName : constListActivePlugins )
    {
      Q_UNUSED( pluginName )
      QgsDebugMsgLevel( pluginName, 1 );
    }
  }
#endif
}


void QgsPluginRegistry::removePlugin( const QString &key )
{
  QgsDebugMsgLevel( "removing plugin: " + key, 2 );
  const QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.find( key );
  if ( it != mPlugins.end() )
  {
    mPlugins.erase( it );
  }

  // python plugins are removed when unloaded
}

void QgsPluginRegistry::unloadAll()
{
  for ( QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.begin(); it != mPlugins.end(); ++it )
  {
    if ( it->plugin() )
    {
      it->plugin()->unload();
    }
    else
    {
      QgsDebugError( "warning: plugin is NULL:" + it.key() );
    }
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    const auto constListActivePlugins = mPythonUtils->listActivePlugins();
    for ( const QString &pluginName : constListActivePlugins )
    {
      mPythonUtils->unloadPlugin( pluginName );
    }
  }
#endif
}


bool QgsPluginRegistry::checkQgisVersion( const QString &minVersion, const QString &maxVersion ) const
{
  // Parse qgisMinVersion. Must be in form x.y.z or just x.y
  const QStringList minVersionParts = minVersion.split( '.' );
  if ( minVersionParts.count() != 2 && minVersionParts.count() != 3 )
    return false;

  int minVerMajor, minVerMinor, minVerBugfix = 0;
  bool ok;
  minVerMajor = minVersionParts.at( 0 ).toInt( &ok );
  if ( !ok )
    return false;
  minVerMinor = minVersionParts.at( 1 ).toInt( &ok );
  if ( !ok )
    return false;
  if ( minVersionParts.count() == 3 )
  {
    minVerBugfix = minVersionParts.at( 2 ).toInt( &ok );
    if ( !ok )
      return false;
  }

  // Parse qgisMaxVersion. Must be in form x.y.z or just x.y
  int maxVerMajor, maxVerMinor, maxVerBugfix = 99;
  if ( maxVersion.isEmpty() || maxVersion == "__error__"_L1 )
  {
    maxVerMajor = minVerMajor;
    maxVerMinor = 99;
  }
  else
  {
    const QStringList maxVersionParts = maxVersion.split( '.' );
    if ( maxVersionParts.count() != 2 && maxVersionParts.count() != 3 )
      return false;

    bool ok;
    maxVerMajor = maxVersionParts.at( 0 ).toInt( &ok );
    if ( !ok )
      return false;
    maxVerMinor = maxVersionParts.at( 1 ).toInt( &ok );
    if ( !ok )
      return false;
    if ( maxVersionParts.count() == 3 )
    {
      maxVerBugfix = maxVersionParts.at( 2 ).toInt( &ok );
      if ( !ok )
        return false;
    }
  }

  // our qgis version - cut release name after version number
  const QString qgisVersion = Qgis::version().section( '-', 0, 0 );

  const QStringList qgisVersionParts = qgisVersion.split( '.' );

  int qgisMajor = qgisVersionParts.at( 0 ).toInt();
  int qgisMinor = qgisVersionParts.at( 1 ).toInt();
  int qgisBugfix = qgisVersionParts.at( 2 ).toInt();

  if ( qgisMinor == 99 )
  {
    // we want the API version, so for x.99 bump it up to the next major release: e.g. 2.99 to 3.0.0
    qgisMajor++;
    qgisMinor = 0;
    qgisBugfix = 0;
  };

  // build XxYyZz strings with trailing zeroes if needed
  const QString minVer = u"%1%2%3"_s.arg( minVerMajor, 2, 10, QChar( '0' ) ).arg( minVerMinor, 2, 10, QChar( '0' ) ).arg( minVerBugfix, 2, 10, QChar( '0' ) );
  const QString maxVer = u"%1%2%3"_s.arg( maxVerMajor, 2, 10, QChar( '0' ) ).arg( maxVerMinor, 2, 10, QChar( '0' ) ).arg( maxVerBugfix, 2, 10, QChar( '0' ) );
  const QString curVer = u"%1%2%3"_s.arg( qgisMajor, 2, 10, QChar( '0' ) ).arg( qgisMinor, 2, 10, QChar( '0' ) ).arg( qgisBugfix, 2, 10, QChar( '0' ) );

  // For Hake GeoDesk (QGIS 4.x runtime), allow modern QGIS 3.x ecosystem plugins (minVerMajor >= 3)
  // as the community repository metadata capped maxVersion at 3.99.
  if ( qgisMajor >= 4 && minVerMajor >= 3 )
  {
    return minVer <= curVer;
  }

  // compare
  return ( minVer <= curVer && maxVer >= curVer );
}


void QgsPluginRegistry::loadPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    QgsSettings settings;

    // is loaded already?
    if ( !isLoaded( packageName ) )
    {
      // if plugin is not compatible, disable it
      if ( !isPythonPluginCompatible( packageName ) )
      {
        QgsMessageLog::logMessage( QObject::tr( "Plugin \"%1\" is not compatible with this version of Hake Geospatial.\nIt will be disabled." ).arg( packageName ), QObject::tr( "Plugins" ) );
        settings.setValue( "/PythonPlugins/" + packageName, false );
        return;
      }

      const QgsScopedRuntimeProfile profile( packageName );
      mPythonUtils->loadPlugin( packageName );
      mPythonUtils->startPlugin( packageName );

      // TODO: test success

      const QString pluginName = mPythonUtils->getPluginMetadata( packageName, u"name"_s );

      // add to settings
      settings.setValue( "/PythonPlugins/" + packageName, true );
      QgsMessageLog::logMessage( QObject::tr( "Loaded %1 (package: %2)" ).arg( pluginName, packageName ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );

      settings.remove( "/PythonPlugins/watchDogTimestamp/" + packageName );
    }
    return;
  }
#endif

  // Native / Fallback loading for macOS, Linux, and environments without in-process Python bindings
  if ( mNativePythonPlugins.contains( packageName ) )
  {
    return;
  }

  QgsSettings settings;
  settings.setValue( u"/PythonPlugins/"_s + packageName, true );

  // 1. Locate plugin directory
  QString pluginDir = QgsApplication::qgisSettingsDirPath() + u"python/plugins/"_s + packageName;
  if ( !QDir( pluginDir ).exists() )
  {
    pluginDir = QgsApplication::pkgDataPath() + u"/python/plugins/"_s + packageName;
  }
  if ( !QDir( pluginDir ).exists() )
  {
    const QStringList searchPaths = settings.value( u"plugins/searchPathsForPlugins"_s ).toStringList();
    for ( const QString &path : searchPaths )
    {
      const QString candidate = path + u"/python/plugins/"_s + packageName;
      if ( QDir( candidate ).exists() )
      {
        pluginDir = candidate;
        break;
      }
    }
  }

  // Handle nested folder if present (e.g. plugin/plugin/metadata.txt)
  if ( !QFile::exists( pluginDir + u"/metadata.txt"_s ) && QFile::exists( pluginDir + u"/"_s + packageName + u"/metadata.txt"_s ) )
  {
    pluginDir = pluginDir + u"/"_s + packageName;
  }

  QString name = packageName;
  QString description;
  QString category;
  QString iconPath;
  QString version;

  QString metadataPath = pluginDir + u"/metadata.txt"_s;
  if ( QFile::exists( metadataPath ) )
  {
    QSettings meta( metadataPath, QSettings::IniFormat );
    name = meta.value( u"general/name"_s ).toString();
    if ( name.isEmpty() )
      name = meta.value( u"name"_s ).toString();
    description = meta.value( u"general/description"_s ).toString();
    if ( description.isEmpty() )
      description = meta.value( u"description"_s ).toString();
    category = meta.value( u"general/category"_s ).toString();
    if ( category.isEmpty() )
      category = meta.value( u"category"_s ).toString();
    version = meta.value( u"general/version"_s ).toString();
    const QString metaIcon = meta.value( u"general/icon"_s ).toString();
    if ( !metaIcon.isEmpty() )
    {
      if ( QFile::exists( pluginDir + u"/"_s + metaIcon ) )
        iconPath = pluginDir + u"/"_s + metaIcon;
      else if ( QFile::exists( QgsApplication::pkgDataPath() + u"/resources/"_s + metaIcon ) )
        iconPath = QgsApplication::pkgDataPath() + u"/resources/"_s + metaIcon;
    }
  }

  if ( name.isEmpty() )
    name = packageName;

  // Resolve icon
  if ( iconPath.isEmpty() || !QFile::exists( iconPath ) )
  {
    const QStringList candidates = {
      pluginDir + u"/icon.png"_s,
      pluginDir + u"/icon.svg"_s,
      pluginDir + u"/icons/icon.png"_s,
      pluginDir + u"/icons/"_s + packageName + u".png"_s,
      pluginDir + u"/icons/logo.png"_s
    };
    for ( const QString &candidate : candidates )
    {
      if ( QFile::exists( candidate ) )
      {
        iconPath = candidate;
        break;
      }
    }
  }

  QIcon icon;
  if ( !iconPath.isEmpty() && QFile::exists( iconPath ) )
  {
    icon = QIcon( iconPath );
  }
  if ( icon.isNull() )
  {
    icon = QgsApplication::getThemeIcon( u"/mActionShowPluginManager.svg"_s );
  }

  QString actionText = name;
  if ( !actionText.startsWith( u"Open "_s, Qt::CaseInsensitive ) )
  {
    actionText = QObject::tr( "Open %1" ).arg( name );
  }

  QWidget *parentWindow = mQgisInterface ? mQgisInterface->mainWindow() : nullptr;
  QAction *action = new QAction( icon, actionText, parentWindow );
  action->setObjectName( QStringLiteral( "mActionPlugin_%1" ).arg( packageName ) );
  action->setToolTip( description.isEmpty() ? name : description );
  action->setStatusTip( description );

  QObject::connect( action, &QAction::triggered, [this, packageName, name, pluginDir]() {
    executePythonPlugin( packageName, name, pluginDir );
  } );

  // Register on plugin toolbar -> mirrors to Extensions tab "Installed" group
  if ( mQgisInterface )
  {
    mQgisInterface->addToolBarIcon( action );
  }

  // Register menu on menubar -> mirrors to Extensions tab "Installed Menus" group
  QMenu *pluginMenu = nullptr;
  if ( parentWindow )
  {
    QMenuBar *menuBar = parentWindow->findChild<QMenuBar *>();
    if ( menuBar )
    {
      pluginMenu = new QMenu( name, menuBar );
      pluginMenu->setObjectName( QStringLiteral( "mPluginMenu_%1" ).arg( packageName ) );
      pluginMenu->setIcon( icon );
      pluginMenu->addAction( action );
      menuBar->addMenu( pluginMenu );
    }
  }

  QString menuCategory = category.trimmed();
  if ( menuCategory.isEmpty() )
    menuCategory = name;
  if ( mQgisInterface )
  {
    mQgisInterface->addPluginToMenu( menuCategory, action );
  }

  NativePluginItem item;
  item.packageName = packageName;
  item.name = name;
  item.description = description;
  item.category = menuCategory;
  item.iconPath = iconPath;
  item.pluginDir = pluginDir;
  item.toolBarAction = action;
  item.menu = pluginMenu;
  mNativePythonPlugins.insert( packageName, item );

  QgsMessageLog::logMessage( QObject::tr( "Loaded %1 (package: %2)" ).arg( name, packageName ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );
}


void QgsPluginRegistry::loadCppPlugin( const QString &fullPathName )
{
  QgsSettings settings;

  QString baseName = QFileInfo( fullPathName ).baseName();

  // first check to see if it's already loaded
  if ( isLoaded( baseName ) )
  {
    // plugin is loaded
    // QMessageBox::warning(this, "Loading Plugins", description + " is already loaded");
    return;
  }

  const QgsScopedRuntimeProfile profile( baseName );

  QLibrary myLib( fullPathName );

  QString myError; //we will only show detailed diagnostics if something went wrong
  myError += QObject::tr( "Library name is %1\n" ).arg( myLib.fileName() );

  const bool loaded = myLib.load();
  if ( !loaded )
  {
    QgsMessageLog::logMessage( QObject::tr( "Failed to load %1 (Reason: %2)" ).arg( myLib.fileName(), myLib.errorString() ), QObject::tr( "Plugins" ) );
    return;
  }

  myError += QObject::tr( "Attempting to resolve the classFactory function\n" );

  type_t *pType = ( type_t * ) cast_to_fptr( myLib.resolve( "type" ) );
  name_t *pName = ( name_t * ) cast_to_fptr( myLib.resolve( "name" ) );

  switch ( pType() )
  {
    case QgisPlugin::Renderer:
    case QgisPlugin::UI:
    {
      // UI only -- doesn't use mapcanvas
      create_ui *cf = ( create_ui * ) cast_to_fptr( myLib.resolve( "classFactory" ) );
      if ( cf )
      {
        QgisPlugin *pl = cf( mQgisInterface );
        if ( pl )
        {
          pl->initGui();
          // add it to the plugin registry
          addPlugin( baseName, QgsPluginMetadata( myLib.fileName(), *pName(), pl ) );
          //add it to the qsettings file [ts]
          settings.setValue( "/Plugins/" + baseName, true );
          QgsMessageLog::logMessage( QObject::tr( "Loaded %1 (Path: %2)" ).arg( *pName(), myLib.fileName() ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );

          QObject *o = dynamic_cast<QObject *>( pl );
          if ( o )
          {
            QgsDebugMsgLevel( u"plugin object name: %1"_s.arg( o->objectName() ), 2 );
            if ( o->objectName().isEmpty() )
            {
#ifndef Q_OS_WIN
              baseName = baseName.mid( 3 );
#endif
              QgsDebugMsgLevel( u"object name to %1"_s.arg( baseName ), 2 );
              o->setObjectName( u"qgis_plugin_%1"_s.arg( baseName ) );
              QgsDebugMsgLevel( u"plugin object name now: %1"_s.arg( o->objectName() ), 2 );
            }

            if ( !o->parent() )
            {
              QgsDebugMsgLevel( u"setting plugin parent"_s, 2 );
              o->setParent( QgisApp::instance() );
            }
            else
            {
              QgsDebugMsgLevel( u"plugin parent already set"_s, 2 );
            }
          }

          settings.remove( "/Plugins/watchDogTimestamp/" + baseName );
        }
        else
        {
          // something went wrong
          QMessageBox::warning(
            mQgisInterface->mainWindow(),
            QObject::tr( "Loading Plugins" ),
            QObject::tr(
              "There was an error loading a plugin. "
              "The following diagnostic information may help the Hake Geospatial developers resolve the issue:\n%1."
            )
              .arg( myError )
          );
          //disable it to the qsettings file [ts]
          settings.setValue( "/Plugins/" + baseName, false );
        }
      }
      else
      {
        QgsMessageLog::logMessage( QObject::tr( "Unable to find the class factory for %1." ).arg( fullPathName ), QObject::tr( "Plugins" ) );
      }
    }
    break;
    default:
      // type is unknown
      QgsMessageLog::logMessage( QObject::tr( "Plugin %1 did not return a valid type and cannot be loaded" ).arg( fullPathName ), QObject::tr( "Plugins" ) );
      break;
  }
}


void QgsPluginRegistry::unloadPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    if ( isLoaded( packageName ) )
    {
      mPythonUtils->unloadPlugin( packageName );
      QgsDebugMsgLevel( "Python plugin successfully unloaded: " + packageName, 2 );
    }
    QgsSettings settings;
    settings.setValue( "/PythonPlugins/" + packageName, false );
    return;
  }
#endif

  QgsSettings settings;
  settings.setValue( u"/PythonPlugins/"_s + packageName, false );

  if ( mNativePythonPlugins.contains( packageName ) )
  {
    NativePluginItem item = mNativePythonPlugins.take( packageName );
    if ( item.toolBarAction && mQgisInterface )
    {
      mQgisInterface->removeToolBarIcon( item.toolBarAction );
      mQgisInterface->removePluginMenu( item.category, item.toolBarAction );
    }
    if ( item.menu && mQgisInterface && mQgisInterface->mainWindow() )
    {
      QMenuBar *menuBar = mQgisInterface->mainWindow()->findChild<QMenuBar *>();
      if ( menuBar )
      {
        menuBar->removeAction( item.menu->menuAction() );
      }
      delete item.menu;
    }
    if ( item.toolBarAction )
    {
      delete item.toolBarAction;
    }
    QgsMessageLog::logMessage( QObject::tr( "Unloaded %1" ).arg( item.name ), QObject::tr( "Plugins" ), Qgis::MessageLevel::Info );
  }
}


void QgsPluginRegistry::unloadCppPlugin( const QString &fullPathName )
{
  QgsSettings settings;
  const QString baseName = QFileInfo( fullPathName ).baseName();
  settings.setValue( "/Plugins/" + baseName, false );
  if ( isLoaded( baseName ) )
  {
    QgisPlugin *pluginInstance = plugin( baseName );
    if ( pluginInstance )
    {
      pluginInstance->unload();
    }
    // remove the plugin from the registry
    removePlugin( baseName );
    QgsDebugMsgLevel( "Cpp plugin successfully unloaded: " + baseName, 2 );
  }
}


//overloaded version of the next method that will load from multiple directories not just one
void QgsPluginRegistry::restoreSessionPlugins( const QStringList &pluginDirList )
{
  QStringListIterator myIterator( pluginDirList );
  while ( myIterator.hasNext() )
  {
    restoreSessionPlugins( myIterator.next() );
  }
}

void QgsPluginRegistry::restoreSessionPlugins( const QString &pluginDirString )
{
  QgsSettings mySettings;

  const QgsScopedRuntimeProfile profile( QObject::tr( "Load plugins" ) );

#if defined( Q_OS_WIN ) || defined( __CYGWIN__ )
  QString pluginExt = "*.dll";
#elif ANDROID
  QString pluginExt = "*plugin.so";
#else
  const QString pluginExt = u"*.so"_s;
#endif

  // check all libs in the current plugin directory and get name and descriptions
  const QDir myPluginDir( pluginDirString, pluginExt, QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::NoSymLinks );

  for ( uint i = 0; i < myPluginDir.count(); i++ )
  {
    const QString myFullPath = pluginDirString + '/' + myPluginDir[i];
    if ( checkCppPlugin( myFullPath ) )
    {
      // check if there is a watchdog timestamp left after last session

      bool pluginCrashedPreviously = false;
      const QString baseName = QFileInfo( myFullPath ).baseName();
      const QVariant lastRun = mySettings.value( u"Plugins/watchDogTimestamp/%1"_s.arg( baseName ) );
      if ( lastRun.isValid() )
      {
        if ( QDateTime::currentDateTime().toSecsSinceEpoch() - lastRun.toLongLong() > 5 )
        {
          // The timestamp is left unremoved and is older than 5 seconds, so it's not coming
          // from a parallelly running instance.
          pluginCrashedPreviously = true;
        }
      }

      if ( pluginCrashedPreviously )
      {
        QToolButton *btnEnablePlugin = new QToolButton();
        btnEnablePlugin->setText( QObject::tr( "Enable Plugin" ) );
        btnEnablePlugin->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QToolButton *btnIgnore = new QToolButton();
        btnIgnore->setText( QObject::tr( "Ignore" ) );
        btnIgnore->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QgsMessageBarItem *watchdogMsg = new QgsMessageBarItem(
          QObject::tr( "Plugin %1" ).arg( baseName ),
          QObject::tr( "This plugin is disabled because it previously crashed Hake Geospatial." ),
          btnEnablePlugin,
          Qgis::MessageLevel::Warning,
          0,
          mQgisInterface->messageBar()
        );
        watchdogMsg->layout()->addWidget( btnIgnore );

        QObject::connect( btnEnablePlugin, &QToolButton::clicked, mQgisInterface->messageBar(), [this, baseName, myFullPath, watchdogMsg]() {
          QgsSettings settings;
          settings.setValue( "/Plugins/" + baseName, true );
          loadCppPlugin( myFullPath );
          settings.remove( u"/Plugins/watchDogTimestamp/%1"_s.arg( baseName ) );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );
        QObject::connect( btnIgnore, &QToolButton::clicked, mQgisInterface->messageBar(), [this, baseName, watchdogMsg]() {
          QgsSettings settings;
          settings.setValue( "/Plugins/" + baseName, false );
          settings.remove( "/Plugins/watchDogTimestamp/" + baseName );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        mQgisInterface->messageBar()->pushItem( watchdogMsg );
        mySettings.setValue( "/Plugins/" + baseName, false );
      }
      if ( mySettings.value( "/Plugins/" + baseName ).toBool() )
      {
        mySettings.setValue( u"Plugins/watchDogTimestamp/%1"_s.arg( baseName ), QDateTime::currentDateTime().toSecsSinceEpoch() );
        loadCppPlugin( myFullPath );
        mySettings.remove( u"/Plugins/watchDogTimestamp/%1"_s.arg( baseName ) );
      }
    }
  }

#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    // check for python plugins system-wide
    const QStringList pluginList = mPythonUtils->pluginList();
    QgsDebugMsgLevel( u"Loading python plugins"_s, 2 );
    QgsDebugMsgLevel( u"Python plugins will be loaded in the following order: "_s + pluginList.join( "," ), 2 );

    QStringList corePlugins = QStringList();
    corePlugins << u"db_manager_community"_s;
    corePlugins << u"processing"_s;
    corePlugins << u"MetaSearch"_s;
    corePlugins << u"grassprovider"_s;

    // make the required core plugins enabled by default:
    const auto constCorePlugins = corePlugins;
    for ( const QString &corePlugin : constCorePlugins )
    {
      if ( !mySettings.contains( "/PythonPlugins/" + corePlugin ) )
      {
        mySettings.setValue( "/PythonPlugins/" + corePlugin, true );
      }
    }

    // The deprecated core stub "db_manager" was replaced by db_manager_community.
    // Force-disable any leftover enable bit from older Hake / QGIS profiles so
    // upgrades never try to load a missing stub or show its deprecation banner.
    mySettings.setValue( u"/PythonPlugins/db_manager"_s, false );

    const auto constPluginList = pluginList;
    for ( const QString &packageName : constPluginList )
    {
      // TODO: apply better solution for #5879
      // start - temporary fix for issue #5879
      if ( QgsApplication::isRunningFromBuildDir() )
      {
        if ( corePlugins.contains( packageName ) )
        {
          QgsApplication::setPkgDataPath( QString() );
        }
        else
        {
          QgsApplication::setPkgDataPath( QgsApplication::buildSourcePath() );
        }
      }
      // end - temporary fix for issue #5879, more below

      bool pluginCrashedPreviously = false;
      const QVariant lastRun = mySettings.value( u"/PythonPlugins/watchDogTimestamp/%1"_s.arg( packageName ) );
      if ( lastRun.isValid() )
      {
        if ( QDateTime::currentDateTime().toSecsSinceEpoch() - lastRun.toLongLong() > 5 )
        {
          // The timestamp is left unremoved and is older than 5 seconds, so it's not coming
          // from a parallelly running instance.
          pluginCrashedPreviously = true;
        }
      }

      if ( pluginCrashedPreviously )
      {
        QToolButton *btnEnablePlugin = new QToolButton();
        btnEnablePlugin->setText( QObject::tr( "Enable Plugin" ) );
        btnEnablePlugin->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QToolButton *btnIgnore = new QToolButton();
        btnIgnore->setText( QObject::tr( "Ignore" ) );
        btnIgnore->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        QgsMessageBarItem *watchdogMsg = new QgsMessageBarItem(
          QObject::tr( "Plugin %1" ).arg( packageName ),
          QObject::tr( "This plugin is disabled because it previously crashed Hake Geospatial." ),
          btnEnablePlugin,
          Qgis::MessageLevel::Warning,
          0,
          mQgisInterface->messageBar()
        );
        watchdogMsg->layout()->addWidget( btnIgnore );

        QObject::connect( btnEnablePlugin, &QToolButton::clicked, mQgisInterface->messageBar(), [this, packageName, watchdogMsg]() {
          QgsSettings settings;
          settings.setValue( "/PythonPlugins/" + packageName, true );
          if ( checkPythonPlugin( packageName ) )
          {
            loadPythonPlugin( packageName );
          }
          settings.remove( "/PythonPlugins/watchDogTimestamp/" + packageName );

          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        QObject::connect( btnIgnore, &QToolButton::clicked, mQgisInterface->messageBar(), [this, packageName, watchdogMsg]() {
          QgsSettings settings;
          settings.setValue( "/PythonPlugins/" + packageName, false );
          settings.remove( "/PythonPlugins/watchDogTimestamp/" + packageName );
          mQgisInterface->messageBar()->popWidget( watchdogMsg );
        } );

        mQgisInterface->messageBar()->pushItem( watchdogMsg );

        mySettings.setValue( "/PythonPlugins/" + packageName, false );
      }
      // check if the plugin was active on last session
      if ( mySettings.value( "/PythonPlugins/" + packageName ).toBool() )
      {
        mySettings.setValue( "/PythonPlugins/watchDogTimestamp/" + packageName, QDateTime::currentDateTime().toSecsSinceEpoch() );
        if ( checkPythonPlugin( packageName ) )
        {
          loadPythonPlugin( packageName );
        }
        mySettings.remove( "/PythonPlugins/watchDogTimestamp/" + packageName );
      }
    }
    // start - temporary fix for issue #5879, more above
    if ( QgsApplication::isRunningFromBuildDir() )
    {
      QgsApplication::setPkgDataPath( QgsApplication::buildOutputPath() + u"/data"_s );
    }
    // end - temporary fix for issue #5879
  }
#endif

  // Ensure enabled Python plugins are restored across sessions on all platforms
  restorePythonPlugins();

  QgsDebugMsgLevel( u"Plugin loading completed"_s, 2 );
}

void QgsPluginRegistry::restorePythonPlugins()
{
  if ( mPythonPluginsRestored )
    return;
  mPythonPluginsRestored = true;

  QgsSettings settings;
  QSet<QString> checkedPackages;

  // 1. Discover all packages explicitly enabled in QSettings
  const QStringList allKeys = settings.allKeys();
  for ( const QString &key : allKeys )
  {
    if ( key.startsWith( u"/PythonPlugins/"_s ) )
    {
      const QString pkg = key.mid( 15 );
      if ( pkg.isEmpty() || pkg == u"watchDogTimestamp"_s || pkg == u"db_manager"_s || pkg.startsWith( u"watchDogTimestamp/"_s ) )
        continue;
      if ( settings.value( key, false ).toBool() )
      {
        checkedPackages.insert( pkg );
      }
    }
  }

  // 2. Scan installed plugin directories for enabled plugins
  QStringList pluginDirs;
  pluginDirs << QgsApplication::qgisSettingsDirPath() + u"python/plugins"_s;
  pluginDirs << QgsApplication::pkgDataPath() + u"/python/plugins"_s;
  const QStringList searchPaths = settings.value( u"plugins/searchPathsForPlugins"_s ).toStringList();
  for ( const QString &path : searchPaths )
  {
    pluginDirs << path + u"/python/plugins"_s;
  }

  for ( const QString &dirPath : std::as_const( pluginDirs ) )
  {
    const QDir dir( dirPath );
    if ( !dir.exists() )
      continue;
    const QFileInfoList subdirs = dir.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );
    for ( const QFileInfo &sub : subdirs )
    {
      const QString pkg = sub.fileName();
      if ( settings.value( u"/PythonPlugins/"_s + pkg, false ).toBool() )
      {
        checkedPackages.insert( pkg );
      }
    }
  }

  for ( const QString &pkg : std::as_const( checkedPackages ) )
  {
    if ( !isLoaded( pkg ) )
    {
      loadPythonPlugin( pkg );
    }
  }
}

void QgsPluginRegistry::executePythonPlugin( const QString &packageName, const QString &name, const QString &pluginDir )
{
  if ( mQgisInterface && mQgisInterface->messageBar() )
  {
    mQgisInterface->messageBar()->pushMessage(
      QObject::tr( "Plugin: %1" ).arg( name ),
      QObject::tr( "Starting %1…" ).arg( name ),
      Qgis::MessageLevel::Info,
      3
    );
  }

  // Look for python 3 executable
  QString pythonExe;
  const QStringList candidates = {
    QStringLiteral( "/opt/homebrew/bin/python3.15" ),
    QStringLiteral( "/opt/homebrew/bin/python3" ),
    QStringLiteral( "/usr/local/bin/python3" ),
    QStringLiteral( "/usr/bin/python3" ),
    QStandardPaths::findExecutable( QStringLiteral( "python3" ) )
  };
  for ( const QString &candidate : candidates )
  {
    if ( !candidate.isEmpty() && QFile::exists( candidate ) )
    {
      pythonExe = candidate;
      break;
    }
  }

  if ( pythonExe.isEmpty() )
  {
    if ( mQgisInterface && mQgisInterface->messageBar() )
    {
      mQgisInterface->messageBar()->pushMessage(
        QObject::tr( "Plugin: %1" ).arg( name ),
        QObject::tr( "Python 3 is required to launch %1." ).arg( name ),
        Qgis::MessageLevel::Warning,
        6
      );
    }
    return;
  }

  // Check launcher script location
  QString launcherScript = QgsApplication::pkgDataPath() + QStringLiteral( "/python/hake_plugin_launcher.py" );
  if ( !QFile::exists( launcherScript ) )
  {
    launcherScript = QCoreApplication::applicationDirPath() + QStringLiteral( "/../Resources/python/hake_plugin_launcher.py" );
  }
  if ( !QFile::exists( launcherScript ) )
  {
    launcherScript = QgsApplication::buildSourcePath() + QStringLiteral( "/src/app/hake_plugin_launcher.py" );
  }
  if ( !QFile::exists( launcherScript ) )
  {
    launcherScript = QgsApplication::qgisSettingsDirPath() + QStringLiteral( "python/hake_plugin_launcher.py" );
  }

  if ( QFile::exists( launcherScript ) )
  {
    QStringList args;
    args << launcherScript << pluginDir << packageName;
    const bool started = QProcess::startDetached( pythonExe, args );
    if ( started && mQgisInterface && mQgisInterface->messageBar() )
    {
      mQgisInterface->messageBar()->pushMessage(
        QObject::tr( "Plugin: %1" ).arg( name ),
        QObject::tr( "%1 is active." ).arg( name ),
        Qgis::MessageLevel::Success,
        4
      );
    }
  }
  else
  {
    if ( mQgisInterface && mQgisInterface->messageBar() )
    {
      mQgisInterface->messageBar()->pushMessage(
        QObject::tr( "Plugin: %1" ).arg( name ),
        QObject::tr( "%1 is installed and enabled." ).arg( name ),
        Qgis::MessageLevel::Success,
        4
      );
    }
  }
}


bool QgsPluginRegistry::checkCppPlugin( const QString &pluginFullPath )
{
  QLibrary myLib( pluginFullPath );
  const bool loaded = myLib.load();
  if ( !loaded )
  {
    QgsMessageLog::logMessage( QObject::tr( "Failed to load %1 (Reason: %2)" ).arg( myLib.fileName(), myLib.errorString() ), QObject::tr( "Plugins" ) );
    return false;
  }

  name_t *myName = ( name_t * ) cast_to_fptr( myLib.resolve( "name" ) );
  description_t *myDescription = ( description_t * ) cast_to_fptr( myLib.resolve( "description" ) );
  category_t *myCategory = ( category_t * ) cast_to_fptr( myLib.resolve( "category" ) );
  version_t *myVersion = ( version_t * ) cast_to_fptr( myLib.resolve( "version" ) );

  if ( myName && myDescription && myVersion && myCategory )
    return true;

  QgsDebugMsgLevel( "Failed to get name, description, category or type for " + myLib.fileName(), 2 );
  return false;
}


bool QgsPluginRegistry::checkPythonPlugin( const QString &packageName )
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    QString pluginName = mPythonUtils->getPluginMetadata( packageName, u"name"_s );
    QString description = mPythonUtils->getPluginMetadata( packageName, u"description"_s );
    QString version = mPythonUtils->getPluginMetadata( packageName, u"version"_s );

    if ( pluginName == "__error__"_L1 || description == "__error__"_L1 || version == "__error__"_L1 )
    {
      QgsMessageLog::logMessage( QObject::tr( "Error when reading metadata of plugin %1" ).arg( packageName ), QObject::tr( "Plugins" ) );
      return false;
    }

    return true;
  }
#endif

  QString pluginDir = QgsApplication::qgisSettingsDirPath() + u"python/plugins/"_s + packageName;
  if ( !QDir( pluginDir ).exists() )
  {
    pluginDir = QgsApplication::pkgDataPath() + u"/python/plugins/"_s + packageName;
  }
  return QDir( pluginDir ).exists();
}

bool QgsPluginRegistry::isPythonPluginCompatible( const QString &packageName ) const
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    const QString minVersion = mPythonUtils->getPluginMetadata( packageName, u"qgisMinimumVersion"_s );
    // try to read qgisMaximumVersion. Note checkQgisVersion can cope with "__error__" value.
    const QString maxVersion = mPythonUtils->getPluginMetadata( packageName, u"qgisMaximumVersion"_s );
    return minVersion != "__error__"_L1 && checkQgisVersion( minVersion, maxVersion );
  }
#endif

  QString pluginDir = QgsApplication::qgisSettingsDirPath() + u"python/plugins/"_s + packageName;
  if ( !QDir( pluginDir ).exists() )
  {
    pluginDir = QgsApplication::pkgDataPath() + u"/python/plugins/"_s + packageName;
  }
  QString metaPath = pluginDir + u"/metadata.txt"_s;
  if ( !QFile::exists( metaPath ) && QFile::exists( pluginDir + u"/"_s + packageName + u"/metadata.txt"_s ) )
  {
    metaPath = pluginDir + u"/"_s + packageName + u"/metadata.txt"_s;
  }
  if ( QFile::exists( metaPath ) )
  {
    QSettings meta( metaPath, QSettings::IniFormat );
    const QString minVer = meta.value( u"general/qgisMinimumVersion"_s ).toString();
    const QString maxVer = meta.value( u"general/qgisMaximumVersion"_s ).toString();
    if ( minVer.isEmpty() )
      return true;
    return checkQgisVersion( minVer, maxVer );
  }
  return true;
}

QList<QgsPluginMetadata *> QgsPluginRegistry::pluginData()
{
  QList<QgsPluginMetadata *> resultList;
  QMap<QString, QgsPluginMetadata>::iterator it = mPlugins.begin();
  for ( ; it != mPlugins.end(); ++it )
  {
    resultList.push_back( &( it.value() ) );
  }
  return resultList;
}
