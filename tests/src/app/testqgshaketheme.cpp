/***************************************************************************
     testqgshaketheme.cpp
     --------------------------------------
    Date                 : October 2026
    Copyright            : (C) 2026 by Hake Technologies
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplication.h"
#include "qgsapplicationthemeregistry.h"
#include "qgshakeicons.h"
#include "qgshaketheme.h"
#include "qgssettings.h"
#include "qgstest.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPalette>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

using namespace Qt::StringLiterals;

/**
 * \ingroup UnitTests
 * Tests for the Hake Light / Hake Night theme handling.
 */
class TestQgsHakeTheme : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void themeInventory();
    void hakeDarkUnchanged();
    void liveSwitchPolicy();
    void validateResources();
    void switchLightNightLight();
    void stressSwitching();
    void corruptThemeKeepsCurrent();
    void iconVariants();
    void appearanceModes();
    void persistence();

  private:
    static double meanLightness( const QIcon &icon );
    static QString variableValue( const QString &theme, const QString &variable );
};

void TestQgsHakeTheme::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsHakeTheme::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

double TestQgsHakeTheme::meanLightness( const QIcon &icon )
{
  const QImage image = icon.pixmap( QSize( 24, 24 ) ).toImage().convertToFormat( QImage::Format_ARGB32 );
  double total = 0;
  int count = 0;
  for ( int y = 0; y < image.height(); ++y )
  {
    for ( int x = 0; x < image.width(); ++x )
    {
      const QColor color = image.pixelColor( x, y );
      if ( color.alpha() < 128 )
        continue;
      total += color.lightnessF();
      ++count;
    }
  }
  return count ? total / count : -1;
}

QString TestQgsHakeTheme::variableValue( const QString &theme, const QString &variable )
{
  QFile file( QgsApplication::applicationThemeRegistry()->themeFolder( theme ) + u"/variables.qss"_s );
  if ( !file.open( QIODevice::ReadOnly ) )
    return QString();
  QTextStream in( &file );
  while ( !in.atEnd() )
  {
    const QString line = in.readLine();
    if ( line.startsWith( variable + ':' ) )
      return line.mid( variable.size() + 1 ).trimmed();
  }
  return QString();
}

void TestQgsHakeTheme::themeInventory()
{
  const QHash<QString, QString> themes = QgsApplication::uiThemes();
  QVERIFY( themes.contains( u"Hake Light"_s ) );
  QVERIFY( themes.contains( u"Hake Night"_s ) );
  QVERIFY( themes.contains( u"Hake Dark"_s ) );
  QCOMPARE( QgsHakeTheme::lightTheme(), u"Hake Light"_s );
  QCOMPARE( QgsHakeTheme::nightTheme(), u"Hake Night"_s );

  const QString nightFolder = QgsApplication::applicationThemeRegistry()->themeFolder( u"Hake Night"_s );
  for ( const QString &file : { u"style.qss"_s, u"variables.qss"_s, u"palette.txt"_s, u"qscintilla.ini"_s } )
    QVERIFY2( QFile::exists( nightFolder + '/' + file ), qPrintable( file ) );
}

void TestQgsHakeTheme::hakeDarkUnchanged()
{
  // Hake Dark is the brand-blue theme and must not be turned into the true dark theme.
  QCOMPARE( variableValue( u"Hake Dark"_s, u"@background"_s ).toLower(), u"#0775e3"_s );
  QCOMPARE( QgsHakeTheme::variantForTheme( u"Hake Dark"_s ), QgsHakeTheme::Variant::None );
  QVERIFY( !QgsHakeIcons::isHakeTheme( u"Hake Dark"_s ) );
}

void TestQgsHakeTheme::liveSwitchPolicy()
{
  QVERIFY( QgsHakeTheme::isLiveSwitchable( u"Hake Light"_s, u"Hake Night"_s ) );
  QVERIFY( QgsHakeTheme::isLiveSwitchable( u"Hake Night"_s, u"Hake Light"_s ) );
  QVERIFY( !QgsHakeTheme::isLiveSwitchable( u"Hake Light"_s, u"Hake Dark"_s ) );
  QVERIFY( !QgsHakeTheme::isLiveSwitchable( u"Hake Dark"_s, u"Hake Night"_s ) );
  QVERIFY( !QgsHakeTheme::isLiveSwitchable( u"default"_s, u"Hake Night"_s ) );
  QVERIFY( !QgsHakeTheme::isLiveSwitchable( u"Hake Night"_s, u"Night Mapping"_s ) );
}

void TestQgsHakeTheme::validateResources()
{
  QString error;
  QVERIFY( QgsHakeTheme::validateThemeResources( u"default"_s, &error ) );
  QVERIFY2( QgsHakeTheme::validateThemeResources( u"Hake Light"_s, &error ), qPrintable( error ) );
  QVERIFY2( QgsHakeTheme::validateThemeResources( u"Hake Night"_s, &error ), qPrintable( error ) );
  QVERIFY2( QgsHakeTheme::validateThemeResources( u"Hake Dark"_s, &error ), qPrintable( error ) );
  error.clear();
  QVERIFY( !QgsHakeTheme::validateThemeResources( u"No Such Theme"_s, &error ) );
  QVERIFY( !error.isEmpty() );
}

void TestQgsHakeTheme::switchLightNightLight()
{
  const thread_local QRegularExpression placeholder( u"@[A-Za-z_]"_s );
  QSignalSpy spy( QgsApplication::instance(), &QgsApplication::themeChanged );

  QgsApplication::setUITheme( u"Hake Light"_s );
  QCOMPARE( QgsApplication::themeName(), u"Hake Light"_s );
  const QPalette light = qApp->palette();
  QVERIFY( light.color( QPalette::Window ).lightness() > 180 );
  QVERIFY( !qApp->styleSheet().contains( placeholder ) );

  QgsApplication::setUITheme( u"Hake Night"_s );
  QCOMPARE( QgsApplication::themeName(), u"Hake Night"_s );
  const QPalette night = qApp->palette();
  QVERIFY( night.color( QPalette::Window ).lightness() < 70 );
  QVERIFY( night.color( QPalette::Base ).lightness() < 70 );
  QVERIFY( night.color( QPalette::WindowText ).lightness() > 160 );
  QVERIFY( night.color( QPalette::Text ).lightness() > 160 );
  QVERIFY( night.color( QPalette::Disabled, QPalette::Text ) != night.color( QPalette::Active, QPalette::Text ) );
  QVERIFY( night.color( QPalette::Disabled, QPalette::WindowText ) != night.color( QPalette::Active, QPalette::WindowText ) );
  QVERIFY( !qApp->styleSheet().isEmpty() );
  QVERIFY2( !qApp->styleSheet().contains( placeholder ), qPrintable( placeholder.match( qApp->styleSheet() ).captured( 0 ) ) );

  QgsApplication::setUITheme( u"Hake Light"_s );
  QCOMPARE( QgsApplication::themeName(), u"Hake Light"_s );
  // Colors from Hake Night must not leak back into Hake Light.
  QCOMPARE( qApp->palette().color( QPalette::Window ), light.color( QPalette::Window ) );
  QCOMPARE( qApp->palette().color( QPalette::Text ), light.color( QPalette::Text ) );
  QCOMPARE( qApp->palette().color( QPalette::Disabled, QPalette::Text ), light.color( QPalette::Disabled, QPalette::Text ) );

  QCOMPARE( spy.count(), 3 );
}

void TestQgsHakeTheme::stressSwitching()
{
  QgsApplication::setUITheme( u"Hake Light"_s );
  const QString lightStyle = qApp->styleSheet();
  for ( int i = 0; i < 25; ++i )
  {
    QgsApplication::setUITheme( u"Hake Night"_s );
    QgsApplication::setUITheme( u"Hake Light"_s );
  }
  QCOMPARE( QgsApplication::themeName(), u"Hake Light"_s );
  QCOMPARE( qApp->styleSheet(), lightStyle );
}

void TestQgsHakeTheme::corruptThemeKeepsCurrent()
{
  QTemporaryDir dir;
  QVERIFY( dir.isValid() );
  const QString stylePath = dir.path() + u"/style.qss"_s;
  {
    QFile style( stylePath );
    QVERIFY( style.open( QIODevice::WriteOnly ) );
    style.write( "QWidget { color: red; }" );
  }
  QVERIFY( QgsApplication::applicationThemeRegistry()->addTheme( u"Hake Test Broken"_s, dir.path() ) );
  QVERIFY( QFile::remove( stylePath ) );

  QgsApplication::setUITheme( u"Hake Night"_s );
  const QString nightStyle = qApp->styleSheet();
  const QColor nightWindow = qApp->palette().color( QPalette::Window );

  QVERIFY( !QgsHakeTheme::validateThemeResources( u"Hake Test Broken"_s ) );
  QgsApplication::setUITheme( u"Hake Test Broken"_s );
  QCOMPARE( QgsApplication::themeName(), u"Hake Night"_s );
  QCOMPARE( qApp->styleSheet(), nightStyle );
  QCOMPARE( qApp->palette().color( QPalette::Window ), nightWindow );

  QgsApplication::applicationThemeRegistry()->removeTheme( u"Hake Test Broken"_s );
  QgsApplication::setUITheme( u"Hake Light"_s );
}

void TestQgsHakeTheme::iconVariants()
{
  for ( const QString &resource : { u"app/hake-theme-moon.svg"_s, u"app/hake-theme-sun.svg"_s } )
  {
    const QIcon light = QgsHakeIcons::icon( resource, QgsHakeTheme::Variant::Light );
    const QIcon night = QgsHakeIcons::icon( resource, QgsHakeTheme::Variant::Night );
    QVERIFY2( !light.isNull(), qPrintable( resource ) );
    QVERIFY2( !night.isNull(), qPrintable( resource ) );

    const double lightLightness = meanLightness( light );
    const double nightLightness = meanLightness( night );
    QVERIFY2( lightLightness >= 0 && nightLightness >= 0, qPrintable( resource ) );
    QVERIFY2( nightLightness > lightLightness, qPrintable( u"%1: night %2 <= light %3"_s.arg( resource ).arg( nightLightness ).arg( lightLightness ) ) );

    // Repeated lookups come from the bounded per-variant cache.
    QCOMPARE( QgsHakeIcons::icon( resource, QgsHakeTheme::Variant::Night ).cacheKey(), night.cacheKey() );
  }
}

void TestQgsHakeTheme::appearanceModes()
{
  using Mode = QgsHakeTheme::AppearanceMode;
  for ( const Mode mode : { Mode::System, Mode::Light, Mode::Dark, Mode::Custom } )
    QCOMPARE( QgsHakeTheme::appearanceModeFromString( QgsHakeTheme::appearanceModeToString( mode ) ), mode );
  QCOMPARE( QgsHakeTheme::appearanceModeFromString( u"unexpected"_s ), Mode::Custom );

  QCOMPARE( QgsHakeTheme::resolveTheme( Mode::Light, u"Hake Dark"_s ), u"Hake Light"_s );
  QCOMPARE( QgsHakeTheme::resolveTheme( Mode::Dark, u"Hake Dark"_s ), u"Hake Night"_s );
  QCOMPARE( QgsHakeTheme::resolveTheme( Mode::Custom, u"Hake Dark"_s ), u"Hake Dark"_s );

  const QString system = QgsHakeTheme::resolveTheme( Mode::System, u"Hake Dark"_s );
  switch ( QgsHakeTheme::systemVariant() )
  {
    case QgsHakeTheme::Variant::Light:
      QCOMPARE( system, u"Hake Light"_s );
      break;
    case QgsHakeTheme::Variant::Night:
      QCOMPARE( system, u"Hake Night"_s );
      break;
    case QgsHakeTheme::Variant::None:
      QCOMPARE( system, u"Hake Dark"_s );
      break;
  }
}

void TestQgsHakeTheme::persistence()
{
  QgsSettings settings;
  settings.remove( u"UI/appearanceMode"_s );

  // Without a stored mode, the mode is derived from UI/UITheme so existing users keep their theme.
  settings.setValue( u"UI/UITheme"_s, u"Hake Dark"_s );
  QCOMPARE( QgsHakeTheme::appearanceMode(), QgsHakeTheme::AppearanceMode::Custom );
  settings.setValue( u"UI/UITheme"_s, u"Hake Light"_s );
  QCOMPARE( QgsHakeTheme::appearanceMode(), QgsHakeTheme::AppearanceMode::Light );

  QVERIFY( QgsHakeTheme::persist( u"Hake Night"_s, QgsHakeTheme::AppearanceMode::Dark ) );
  QCOMPARE( QgsSettings().value( u"UI/UITheme"_s ).toString(), u"Hake Night"_s );
  QCOMPARE( QgsSettings().value( u"UI/appearanceMode"_s ).toString(), u"dark"_s );
  QCOMPARE( QgsHakeTheme::appearanceMode(), QgsHakeTheme::AppearanceMode::Dark );

  // System mode still stores a concrete theme name in UI/UITheme.
  QVERIFY( QgsHakeTheme::persist( u"Hake Light"_s, QgsHakeTheme::AppearanceMode::System ) );
  QCOMPARE( QgsSettings().value( u"UI/UITheme"_s ).toString(), u"Hake Light"_s );
  QCOMPARE( QgsHakeTheme::appearanceMode(), QgsHakeTheme::AppearanceMode::System );

  settings.remove( u"UI/appearanceMode"_s );
  settings.remove( u"UI/UITheme"_s );
}

QGSTEST_MAIN( TestQgsHakeTheme )
#include "testqgshaketheme.moc"
