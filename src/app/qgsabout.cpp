/***************************************************************************
                          qgsabout.cpp  -  description
                             -------------------
    begin                : Sat Aug 10 2002
    copyright            : (C) 2002 by Gary E.Sherman
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

#include "qgsabout.h"

#include "qgis.h"
#include "qgsapplication.h"
#include "qgshakeicons.h"
#include "qgslogger.h"
#include "qgsnetworkaccessmanager.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

#include "moc_qgsabout.cpp"

using namespace Qt::StringLiterals;

namespace
{
  // Avoid re-downloading What's New on every About open within a day.
  constexpr qint64 WHATSNEW_CACHE_MAX_AGE_SECS = 24 * 60 * 60;
  constexpr int WHATSNEW_TRANSFER_TIMEOUT_MS = 8000;
}

#ifdef Q_OS_MACOS
// Modeless dialog with close button only
constexpr Qt::WindowFlags kAboutWindowFlags = Qt::WindowSystemMenuHint;
#else
// Normal dialog in non Mac-OS
constexpr Qt::WindowFlags kAboutWindowFlags = Qt::WindowFlags();
#endif

QgsAbout::QgsAbout( QWidget *parent )
  : QgsOptionsDialogBase( u"about"_s, parent, kAboutWindowFlags )
{
  setupUi( this );
  if ( QgsHakeIcons::isHakeTheme( QgsApplication::themeName() ) )
  {
    // Sidebar rows follow the page order in qgsabout.ui.
    const QStringList pageIcons { u"about:about"_s, u"about:whats-new"_s, u"about:license"_s };
    for ( int row = 0; row < std::min( static_cast<int>( pageIcons.size() ), mOptionsListWidget->count() ); ++row )
      mOptionsListWidget->item( row )->setIcon( QgsHakeIcons::iconFor( pageIcons.at( row ), QString() ) );
  }
  updateLogo();
  setWindowTitle( tr( "About %1" ).arg( Qgis::productDisplayName() ) );
  connect( btnQgisUser, &QPushButton::clicked, this, &QgsAbout::btnQgisUser_clicked );
  connect( btnQgisHome, &QPushButton::clicked, this, &QgsAbout::btnQgisHome_clicked );
  connect( btnCopyToClipboard, &QPushButton::clicked, this, &QgsAbout::btnCopyToClipboard_clicked );
  if constexpr ( QSysInfo::WordSize != 64 )
  {
    // 64 bit is the current standard. Only specify word size if it is not 64.
    initOptionsBase( true, tr( "%1 - %2 Bit" ).arg( windowTitle() ).arg( QSysInfo::WordSize ) );
  }
  else
  {
    initOptionsBase( true );
  }
  init();
}

void QgsAbout::init()
{
  setWhatsNew();
  setLicence();
}

void QgsAbout::updateLogo()
{
  const qreal dpr = qgisIcon->devicePixelRatioF();
  QPixmap logo( u":/images/icons/hake-gis-full.png"_s );
  logo = logo.scaled( qgisIcon->maximumSize() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation );
  logo.setDevicePixelRatio( dpr );
  qgisIcon->setPixmap( logo );
}

bool QgsAbout::event( QEvent *e )
{
#if QT_VERSION >= QT_VERSION_CHECK( 6, 6, 0 )
  if ( e->type() == QEvent::DevicePixelRatioChange )
    updateLogo();
#endif
  return QgsOptionsDialogBase::event( e );
}

void QgsAbout::updateWindowTitle()
{
  // QgsOptionsDialogBase appends the current sidebar page (e.g. " — About").
  // The dialog title already starts with "About …", so skip that page to avoid
  // "About … — About". Still append What's New / License.
  const QString itemText = mOptListWidget && mOptListWidget->currentItem()
                             ? mOptListWidget->currentItem()->text()
                             : QString();
  if ( !itemText.isEmpty() && itemText.compare( tr( "About" ), Qt::CaseInsensitive ) != 0 )
  {
    setWindowTitle( u"%1 %2 %3"_s.arg( mDialogTitle, QChar( 0x2014 ), itemText ) );
  }
  else
  {
    setWindowTitle( mDialogTitle );
  }
}

void QgsAbout::setLicence()
{
  QString licencePath = QgsApplication::licenceFilePath();
  QFile licenceFile( licencePath );
  if ( !licenceFile.exists() )
  {
    const QStringList fallbacks = {
      QgsApplication::buildSourcePath() + u"/doc/LICENSE"_s,
      QgsApplication::buildOutputPath() + u"/data/doc/LICENSE"_s,
      QDir( QgsApplication::pkgDataPath() ).filePath( u"doc/LICENSE"_s ),
      QCoreApplication::applicationDirPath() + u"/../Resources/hake-gis/doc/LICENSE"_s,
      QCoreApplication::applicationDirPath() + u"/../../Resources/hake-gis/doc/LICENSE"_s,
      QCoreApplication::applicationDirPath() + u"/../../../data/doc/LICENSE"_s,
      QDir::current().filePath( u"doc/LICENSE"_s )
    };
    for ( const QString &fb : fallbacks )
    {
      if ( QFile::exists( fb ) )
      {
        licenceFile.setFileName( fb );
        break;
      }
    }
  }

  QgsDebugMsgLevel( u"Reading licence file %1"_s.arg( licenceFile.fileName() ), 2 );
  if ( licenceFile.open( QIODevice::ReadOnly ) )
  {
    const bool isDark = QgsApplication::themeName().contains( u"Night"_s, Qt::CaseInsensitive ) ||
                        QgsApplication::themeName().contains( u"Dark"_s, Qt::CaseInsensitive ) ||
                        ( qApp && qApp->palette().color( QPalette::Window ).lightness() < 128 );
    const QString textColor = isDark ? QStringLiteral( "#E6EDF5" ) : QStringLiteral( "#0F172A" );
    txtLicense->document()->setDefaultStyleSheet( QStringLiteral(
      "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; font-size: 13pt; color: %1; line-height: 1.6; padding: 12px; }"
      "pre { font-family: 'SF Mono', Menlo, Monaco, Consolas, 'Courier New', monospace; font-size: 12pt; line-height: 1.55; color: %1; white-space: pre-wrap; word-wrap: break-word; }"
    ).arg( textColor ) );
    txtLicense->setText( QString::fromUtf8( licenceFile.readAll() ) );
  }
}

void QgsAbout::setVersion( const QString &v )
{
  txtVersion->setBackgroundRole( QPalette::NoRole );
  txtVersion->setAutoFillBackground( true );

  const bool isDark = QgsApplication::themeName().contains( u"Night"_s, Qt::CaseInsensitive ) ||
                      QgsApplication::themeName().contains( u"Dark"_s, Qt::CaseInsensitive ) ||
                      ( qApp && qApp->palette().color( QPalette::Window ).lightness() < 128 );
  const QString textColor = isDark ? QStringLiteral( "#D7E1EC" ) : QStringLiteral( "#1E293B" );
  const QString headingColor = isDark ? QStringLiteral( "#38BDF8" ) : QStringLiteral( "#0284C7" );
  const QString linkColor = isDark ? QStringLiteral( "#38BDF8" ) : QStringLiteral( "#0284C7" );
  const QString tableBorder = isDark ? QStringLiteral( "rgba(255,255,255,0.08)" ) : QStringLiteral( "#E2E8F0" );

  const QString style = QStringLiteral(
    "body, table, td, tr, p {"
    "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
    "  font-size: 12.5pt;"
    "  color: %1;"
    "  line-height: 1.6;"
    "}"
    "td {"
    "  padding: 6px 14px;"
    "  border-bottom: 1px solid %4;"
    "}"
    "b {"
    "  font-size: 13.5pt;"
    "  font-weight: 700;"
    "  color: %2;"
    "}"
    "a {"
    "  color: %3;"
    "  text-decoration: none;"
    "  font-weight: 600;"
    "}"
  ).arg( textColor, headingColor, linkColor, tableBorder );

  txtVersion->document()->setDefaultStyleSheet( style );
  txtVersion->setHtml( v );
  mVersionString = v;
}

QString QgsAbout::defaultWhatsNewHtml()
{
  const QString ver = Qgis::productVersionLabel();
  return u"<p style=\"margin-bottom: 16px;\"><img src=\"qrc:/images/icons/hake-gis-full.png\" width=\"160\" height=\"160\"></p>"
         "<h2>Hake GeoDesk – Desktop GIS</h2>"
         "<p style=\"font-size: 13pt; color: #94A3B8; margin-top: 2px;\">Professional GIS for Mapping, Analysis & Spatial Intelligence</p>"
         "<p style=\"margin: 16px 0 20px 0;\"><span style=\"background-color: #1E3A5F; color: #38BDF8; border: 1px solid #38BDF8; padding: 6px 14px; border-radius: 6px; font-size: 13.5pt; font-weight: bold;\">&nbsp;Version %1&nbsp;</span></p>"
         "<h4>Product & branding</h4>"
         "<ul>"
         "<li>Desktop product identity: Hake GeoDesk – Desktop GIS with product version label %1.</li>"
         "<li>Modern Qt6 desktop foundation for mapping, analysis, and spatial intelligence workflows.</li>"
         "</ul>"
         "<h4>User interface & map tools</h4>"
         "<ul>"
         "<li>Redesigned welcome page for a clearer first-run experience.</li>"
         "<li>Create user-defined toolbars and menus; unlock shipping themes through plugins.</li>"
         "<li>Map tool to edit blank segments; copy features in an array along a line.</li>"
         "<li>Attribute table: double-click to zoom to a feature.</li>"
         "</ul>"_s.arg( ver );
}

QString QgsAbout::whatsNewCachePath()
{
  return QgsApplication::qgisSettingsDirPath() + u"whatsnew/latest.html"_s;
}

bool QgsAbout::isWhatsNewCacheFresh( const QString &path )
{
  const QFileInfo info( path );
  if ( !info.exists() || info.size() == 0 )
    return false;
  return info.lastModified().secsTo( QDateTime::currentDateTime() ) < WHATSNEW_CACHE_MAX_AGE_SECS;
}

void QgsAbout::writeWhatsNewCache( const QString &path, const QByteArray &data )
{
  const QFileInfo info( path );
  if ( !QDir().mkpath( info.absolutePath() ) )
  {
    QgsDebugError( u"Could not create What's New cache directory %1"_s.arg( info.absolutePath() ) );
    return;
  }
  QFile file( path );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
  {
    QgsDebugError( u"Could not write What's New cache %1"_s.arg( path ) );
    return;
  }
  QString content = QString::fromUtf8( data );
  content.replace( u"2026.0.0"_s, Qgis::productVersionLabel() );
  static const QRegularExpression verRe( u"2026\\.0\\.\\d+"_s );
  content.replace( verRe, Qgis::productVersionLabel() );
  file.write( content.toUtf8() );

  const QString logoPath = info.absolutePath() + u"/logo.png"_s;
  if ( !QFile::exists( logoPath ) )
  {
    const QPixmap logo( u":/images/icons/hake-gis-full.png"_s );
    logo.save( logoPath, "PNG" );
  }
}

void QgsAbout::showWhatsNewHtml( const QString &html )
{
  txtWhatsNew->clear();

  const bool isDark = QgsApplication::themeName().contains( u"Night"_s, Qt::CaseInsensitive ) ||
                      QgsApplication::themeName().contains( u"Dark"_s, Qt::CaseInsensitive ) ||
                      ( qApp && qApp->palette().color( QPalette::Window ).lightness() < 128 );

  const QString bodyColor = isDark ? QStringLiteral( "#D7E1EC" ) : QStringLiteral( "#1E293B" );
  const QString h2Color = isDark ? QStringLiteral( "#FFFFFF" ) : QStringLiteral( "#0F172A" );
  const QString h3Color = isDark ? QStringLiteral( "#38BDF8" ) : QStringLiteral( "#0284C7" );
  const QString h4Color = isDark ? QStringLiteral( "#2DD4BF" ) : QStringLiteral( "#0D9488" );
  const QString listColor = isDark ? QStringLiteral( "#CBD5E1" ) : QStringLiteral( "#334155" );
  const QString borderSep = isDark ? QStringLiteral( "rgba(255,255,255,0.12)" ) : QStringLiteral( "#CBD5E1" );
  const QString linkColor = isDark ? QStringLiteral( "#38BDF8" ) : QStringLiteral( "#0284C7" );

  QString style = QStringLiteral(
    " body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; font-size: 12.5pt; line-height: 1.6; color: %1; padding: 10px; }"
    " h2 { font-size: 16pt; font-weight: 700; color: %2; margin-top: 14px; margin-bottom: 6px; }"
    " h3 { font-size: 14.5pt; font-weight: 700; color: %3; margin-top: 14px; margin-bottom: 6px; }"
    " h4 { font-size: 13.5pt; font-weight: 600; color: %4; margin-top: 18px; margin-bottom: 8px; padding-bottom: 4px; border-bottom: 1px solid %6; }"
    " li { font-size: 12.5pt; line-height: 1.6; margin-bottom: 6px; color: %5; }"
    " p { font-size: 12.5pt; line-height: 1.6; margin-bottom: 8px; color: %1; }"
    " a { color: %7; text-decoration: none; font-weight: 600; }"
  ).arg( bodyColor, h2Color, h3Color, h4Color, listColor, borderSep, linkColor );

  txtWhatsNew->document()->setDefaultStyleSheet( style );

  const QPixmap logo( u":/images/icons/hake-gis-full.png"_s );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"http://plugins.haketech.com/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"https://plugins.haketech.com/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"qrc:/images/icons/hake-gis-full.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u":/images/icons/hake-gis-full.png"_s ), logo );

  QString processedHtml = html;

  // Enlarge Hake Geo logo to a prominent 160x160
  static const QRegularExpression imgLogoRe( u"<img[^>]*logo\\.png[^>]*>"_s, QRegularExpression::CaseInsensitiveOption );
  if ( processedHtml.contains( imgLogoRe ) )
  {
    processedHtml.replace( imgLogoRe, u"<p style=\"margin-bottom: 16px;\"><img src=\"qrc:/images/icons/hake-gis-full.png\" width=\"160\" height=\"160\"></p>"_s );
  }
  else
  {
    static const QRegularExpression genericImgRe( u"<img[^>]*src=[\"'][^\"']*[\"'][^>]*>"_s, QRegularExpression::CaseInsensitiveOption );
    if ( processedHtml.contains( genericImgRe ) )
      processedHtml.replace( genericImgRe, u"<p style=\"margin-bottom: 16px;\"><img src=\"qrc:/images/icons/hake-gis-full.png\" width=\"160\" height=\"160\"></p>"_s );
    else if ( !processedHtml.contains( u"hake-gis-full.png"_s ) )
      processedHtml.prepend( u"<p style=\"margin-bottom: 16px;\"><img src=\"qrc:/images/icons/hake-gis-full.png\" width=\"160\" height=\"160\"></p>"_s );
  }

  // Modern version badge styling
  static const QRegularExpression verHeadingRe( u"<h3[^>]*>\\s*(Version\\s+[^<]+)\\s*</h3>"_s, QRegularExpression::CaseInsensitiveOption );
  const QString verBadge = isDark
    ? QStringLiteral( "<p style=\"margin: 14px 0 18px 0;\"><span style=\"background-color: #1E3A5F; color: #38BDF8; border: 1px solid #38BDF8; padding: 6px 14px; border-radius: 6px; font-size: 13.5pt; font-weight: bold;\">&nbsp;\\1&nbsp;</span></p>" )
    : QStringLiteral( "<p style=\"margin: 14px 0 18px 0;\"><span style=\"background-color: #E0F2FE; color: #0369A1; border: 1px solid #0284C7; padding: 6px 14px; border-radius: 6px; font-size: 13.5pt; font-weight: bold;\">&nbsp;\\1&nbsp;</span></p>" );
  processedHtml.replace( verHeadingRe, verBadge );

  const QString curVersion = Qgis::productVersionLabel();
  processedHtml.replace( u"2026.0.0"_s, curVersion );
  static const QRegularExpression verRe( u"2026\\.0\\.\\d+"_s );
  processedHtml.replace( verRe, curVersion );

  txtWhatsNew->setHtml( processedHtml );
}

void QgsAbout::showWhatsNewFile( const QString &path )
{
  if ( !QFile::exists( path ) )
    return;
  QFile file( path );
  if ( !file.open( QIODevice::ReadOnly ) )
    return;
  showWhatsNewHtml( QString::fromUtf8( file.readAll() ) );
}

void QgsAbout::setWhatsNew()
{
  // Hake GeoDesk product What's New — not upstream QGIS NEWS.html.
  // Prefer remote latest.html; fall back to local disk cache only.
  if ( mWhatsNewReply )
  {
    mWhatsNewReply->abort();
    mWhatsNewReply->deleteLater();
    mWhatsNewReply = nullptr;
  }

  const QString cachePath = whatsNewCachePath();

  if ( isWhatsNewCacheFresh( cachePath ) )
  {
    showWhatsNewFile( cachePath );
    return;
  }

  // Show stale cache immediately (if any), then refresh from network.
  if ( QFile::exists( cachePath ) )
    showWhatsNewFile( cachePath );
  else
    showWhatsNewHtml( defaultWhatsNewHtml() );

  QNetworkRequest request{ QUrl{ Qgis::whatsNewUrl() } };
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  request.setTransferTimeout( WHATSNEW_TRANSFER_TIMEOUT_MS );
  request.setRawHeader( "Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.8" );

  mWhatsNewReply = QgsNetworkAccessManager::instance()->get( request );
  connect( mWhatsNewReply, &QNetworkReply::finished, this, &QgsAbout::whatsNewReplyFinished );
}

void QgsAbout::whatsNewReplyFinished()
{
  QNetworkReply *reply = mWhatsNewReply;
  mWhatsNewReply = nullptr;
  if ( !reply )
    return;

  reply->deleteLater();

  const QString cachePath = whatsNewCachePath();

  if ( reply->error() != QNetworkReply::NoError )
  {
    QgsDebugMsgLevel( u"What's New fetch failed: %1"_s.arg( reply->errorString() ), 2 );
    if ( txtWhatsNew->toPlainText().trimmed().isEmpty() )
    {
      if ( QFile::exists( cachePath ) )
        showWhatsNewFile( cachePath );
      else
        showWhatsNewHtml( defaultWhatsNewHtml() );
    }
    return;
  }

  const QByteArray data = reply->readAll();
  if ( data.isEmpty() )
  {
    if ( txtWhatsNew->toPlainText().trimmed().isEmpty() )
    {
      if ( QFile::exists( cachePath ) )
        showWhatsNewFile( cachePath );
      else
        showWhatsNewHtml( defaultWhatsNewHtml() );
    }
    return;
  }

  writeWhatsNewCache( cachePath, data );
  showWhatsNewHtml( QString::fromUtf8( data ) );
}

void QgsAbout::btnCopyToClipboard_clicked()
{
  QGuiApplication::clipboard()->setText( mVersionString );
}

void QgsAbout::btnQgisUser_clicked()
{
  openUrl( u"https://haketech.com"_s );
}

void QgsAbout::btnQgisHome_clicked()
{
  openUrl( u"https://haketech.com"_s );
}

void QgsAbout::openUrl( const QUrl &url )
{
  //use the users default browser
  QDesktopServices::openUrl( url );
}

/*
 * The function below makes a name safe for using in most file system
 * Step 1: Code QString as UTF-8
 * Step 2: Replace all bytes of the UTF-8 above 0x7f with the hexcode in lower case.
 * Step 2: Replace all non [a-z][a-Z][0-9] with underscore (backward compatibility)
 */
QString QgsAbout::fileSystemSafe( const QString &fileName )
{
  QString result;
  QByteArray utf8 = fileName.toUtf8();

  for ( int i = 0; i < utf8.size(); i++ )
  {
    const uchar c = utf8[i];

    if ( c > 0x7f )
    {
      result = result + u"%1"_s.arg( c, 2, 16, QChar( '0' ) );
    }
    else
    {
      result = result + QChar( c );
    }
  }

  const thread_local QRegularExpression sNonAlphaNumericRx( u"[^a-zA-Z0-9]"_s );
  result.replace( sNonAlphaNumericRx, u"_"_s );
  QgsDebugMsgLevel( result, 3 );

  return result;
}
