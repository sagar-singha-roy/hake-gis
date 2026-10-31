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
  QFile licenceFile( QgsApplication::licenceFilePath() );
  QgsDebugMsgLevel( u"Reading licence file %1"_s.arg( licenceFile.fileName() ), 2 );
  if ( licenceFile.open( QIODevice::ReadOnly ) )
  {
    txtLicense->setText( licenceFile.readAll() );
  }
}

void QgsAbout::setVersion( const QString &v )
{
  txtVersion->setBackgroundRole( QPalette::NoRole );
  txtVersion->setAutoFillBackground( true );
  txtVersion->setHtml( v );
  mVersionString = v;
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
  file.write( data );

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
  txtWhatsNew->document()->setDefaultStyleSheet( QgsApplication::reportStyleSheet() );

  const QPixmap logo( u":/images/icons/hake-gis-full.png"_s );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"http://plugins.haketech.com/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"https://plugins.haketech.com/logo.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u"qrc:/images/icons/hake-gis-full.png"_s ), logo );
  txtWhatsNew->document()->addResource( QTextDocument::ImageResource, QUrl( u":/images/icons/hake-gis-full.png"_s ), logo );

  QString processedHtml = html;
  static const QRegularExpression logoRe( u"src=[\"'](?:https?://[^/\"'>]+)?/?(?:[^/\"'>]+/)*logo\\.png[\"']"_s, QRegularExpression::CaseInsensitiveOption );
  processedHtml.replace( logoRe, u"src=\"qrc:/images/icons/hake-gis-full.png\""_s );

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
  {
    txtWhatsNew->clear();
    txtWhatsNew->document()->setDefaultStyleSheet( QgsApplication::reportStyleSheet() );
  }

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
    if ( txtWhatsNew->toPlainText().trimmed().isEmpty() && QFile::exists( cachePath ) )
      showWhatsNewFile( cachePath );
    return;
  }

  const QByteArray data = reply->readAll();
  if ( data.isEmpty() )
  {
    if ( txtWhatsNew->toPlainText().trimmed().isEmpty() && QFile::exists( cachePath ) )
      showWhatsNewFile( cachePath );
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
