/***************************************************************************
  qgshaketheme.cpp
  -------------------
  begin                : October 2026
  copyright            : (C) 2026 by Hake Technologies
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgshaketheme.h"

#include "qgsapplication.h"
#include "qgsapplicationthemeregistry.h"
#include "qgssettings.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStringList>
#include <QStyleHints>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace
{
  const char *const THEME_SETTING = "UI/UITheme";
  const char *const APPEARANCE_SETTING = "UI/appearanceMode";

  bool readable( const QString &path )
  {
    QFile file( path );
    return file.open( QIODevice::ReadOnly );
  }
} // namespace

QString QgsHakeTheme::lightTheme()
{
  return u"Hake Light"_s;
}

QString QgsHakeTheme::nightTheme()
{
  return u"Hake Night"_s;
}

QgsHakeTheme::Variant QgsHakeTheme::variantForTheme( const QString &themeName )
{
  if ( themeName == lightTheme() )
    return Variant::Light;
  if ( themeName == nightTheme() )
    return Variant::Night;
  return Variant::None;
}

bool QgsHakeTheme::isLiveSwitchable( const QString &fromTheme, const QString &toTheme )
{
  return variantForTheme( fromTheme ) != Variant::None && variantForTheme( toTheme ) != Variant::None;
}

bool QgsHakeTheme::validateThemeResources( const QString &themeName, QString *error )
{
  const auto fail = [error]( const QString &message ) {
    if ( error )
      *error = message;
    return false;
  };

  if ( themeName == "default"_L1 )
    return true;

  const QString folder = QgsApplication::applicationThemeRegistry()->themeFolder( themeName );
  if ( folder.isEmpty() )
    return fail( QObject::tr( "UI theme \"%1\" is not installed." ).arg( themeName ) );

  if ( !readable( folder + u"/style.qss"_s ) )
    return fail( QObject::tr( "style.qss of UI theme \"%1\" cannot be read." ).arg( themeName ) );

  const QString variablesPath = folder + u"/variables.qss"_s;
  if ( QFileInfo::exists( variablesPath ) && !readable( variablesPath ) )
    return fail( QObject::tr( "variables.qss of UI theme \"%1\" cannot be read." ).arg( themeName ) );

  const QString palettePath = folder + u"/palette.txt"_s;
  if ( QFileInfo::exists( palettePath ) )
  {
    QFile palette( palettePath );
    if ( !palette.open( QIODevice::ReadOnly ) )
      return fail( QObject::tr( "palette.txt of UI theme \"%1\" cannot be read." ).arg( themeName ) );

    QTextStream in( &palette );
    int lineNumber = 0;
    while ( !in.atEnd() )
    {
      const QString line = in.readLine().trimmed();
      ++lineNumber;
      if ( line.isEmpty() )
        continue;
      QStringList parts = line.split( ':' );
      if ( parts.count() == 3 && parts.at( 0 ).trimmed() == "disabled"_L1 )
        parts.removeFirst();
      bool ok = false;
      const int role = parts.count() == 2 ? parts.at( 0 ).trimmed().toInt( &ok ) : -1;
      if ( !ok || role < 0 || role >= static_cast<int>( QPalette::NColorRoles ) )
        return fail( QObject::tr( "palette.txt of UI theme \"%1\" is invalid at line %2." ).arg( themeName ).arg( lineNumber ) );
    }
  }

  return true;
}

QgsHakeTheme::AppearanceMode QgsHakeTheme::appearanceMode()
{
  const QgsSettings settings;
  const QString stored = settings.value( QLatin1String( APPEARANCE_SETTING ) ).toString();
  if ( !stored.isEmpty() )
    return appearanceModeFromString( stored );

  switch ( variantForTheme( settings.value( QLatin1String( THEME_SETTING ), "default"_L1 ).toString() ) )
  {
    case Variant::Light:
      return AppearanceMode::Light;
    case Variant::Night:
      return AppearanceMode::Dark;
    case Variant::None:
      break;
  }
  return AppearanceMode::Custom;
}

QString QgsHakeTheme::appearanceModeToString( AppearanceMode mode )
{
  switch ( mode )
  {
    case AppearanceMode::System:
      return u"system"_s;
    case AppearanceMode::Light:
      return u"light"_s;
    case AppearanceMode::Dark:
      return u"dark"_s;
    case AppearanceMode::Custom:
      break;
  }
  return u"custom"_s;
}

QgsHakeTheme::AppearanceMode QgsHakeTheme::appearanceModeFromString( const QString &value )
{
  if ( value == "system"_L1 )
    return AppearanceMode::System;
  if ( value == "light"_L1 )
    return AppearanceMode::Light;
  if ( value == "dark"_L1 )
    return AppearanceMode::Dark;
  return AppearanceMode::Custom;
}

QgsHakeTheme::Variant QgsHakeTheme::systemVariant()
{
#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
  if ( const QStyleHints *hints = QGuiApplication::styleHints() )
  {
    switch ( hints->colorScheme() )
    {
      case Qt::ColorScheme::Light:
        return Variant::Light;
      case Qt::ColorScheme::Dark:
        return Variant::Night;
      case Qt::ColorScheme::Unknown:
        break;
    }
  }
#endif
  return Variant::None;
}

QString QgsHakeTheme::resolveTheme( AppearanceMode mode, const QString &fallbackTheme )
{
  switch ( mode )
  {
    case AppearanceMode::Light:
      return u"default"_s;
    case AppearanceMode::Dark:
      return nightTheme();
    case AppearanceMode::System:
      switch ( systemVariant() )
      {
        case Variant::Light:
          return u"default"_s;
        case Variant::Night:
          return nightTheme();
        case Variant::None:
          break;
      }
      break;
    case AppearanceMode::Custom:
      break;
  }
  return fallbackTheme;
}

bool QgsHakeTheme::persist( const QString &themeName, AppearanceMode mode )
{
  {
    QgsSettings settings;
    settings.setValue( QLatin1String( THEME_SETTING ), themeName );
    settings.setValue( QLatin1String( APPEARANCE_SETTING ), appearanceModeToString( mode ) );
    settings.sync();
  }

  // QgsSettings does not expose the write status; check the same user store directly.
  QSettings store;
  store.sync();
  return store.isWritable() && store.status() == QSettings::NoError && store.value( QLatin1String( THEME_SETTING ) ).toString() == themeName;
}
