/***************************************************************************
                          qgsmessagebaritem.h  -  description
                             -------------------
    begin                : August 2013
    copyright            : (C) 2013 by Denis Rouzaud
    email                : denis.rouzaud@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmessagebaritem.h"

#include "qgsapplication.h"
#include "qgsgui.h"
#include "qgsmessagebar.h"
#include "qgsnative.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QTextBrowser>

#include "moc_qgsmessagebaritem.cpp"

using namespace Qt::StringLiterals;

QgsMessageBarItem::QgsMessageBarItem( const QString &text, Qgis::MessageLevel level, int duration, QWidget *parent )
  : QWidget( parent )
  , mText( text )
  , mLevel( level )
  , mDuration( duration )
{
  writeContent();
}

QgsMessageBarItem::QgsMessageBarItem( const QString &title, const QString &text, Qgis::MessageLevel level, int duration, QWidget *parent )
  : QWidget( parent )
  , mTitle( title )
  , mText( text )
  , mLevel( level )
  , mDuration( duration )
{
  writeContent();
}

QgsMessageBarItem::QgsMessageBarItem( const QString &title, const QString &text, QWidget *widget, Qgis::MessageLevel level, int duration, QWidget *parent )
  : QWidget( parent )
  , mTitle( title )
  , mText( text )
  , mLevel( level )
  , mDuration( duration )
  , mWidget( widget )
  , mUserIcon( QIcon() )

{
  writeContent();
}

QgsMessageBarItem::QgsMessageBarItem( QWidget *widget, Qgis::MessageLevel level, int duration, QWidget *parent )
  : QWidget( parent )
  , mLevel( level )
  , mDuration( duration )
  , mWidget( widget )
  , mUserIcon( QIcon() )

{
  writeContent();
}

void QgsMessageBarItem::writeContent()
{
  if ( mDuration < 0 )
    mDuration = QgsMessageBar::defaultMessageTimeout( mLevel );

  if ( !mLayout )
  {
    mLayout = new QHBoxLayout( this );
    mLayout->setContentsMargins( 0, 0, 0, 0 );
    mTextBrowser = nullptr;
    mLblIcon = nullptr;
  }

  // ICON
  if ( !mLblIcon )
  {
    mLblIcon = new QLabel( this );
    mLayout->addWidget( mLblIcon );
  }
  QIcon icon;
  if ( !mUserIcon.isNull() )
  {
    icon = mUserIcon;
  }
  else
  {
    QString msgIcon( u"/mIconInfo.svg"_s );
    switch ( mLevel )
    {
      case Qgis::MessageLevel::Critical:
        msgIcon = u"/mIconCritical.svg"_s;
        break;
      case Qgis::MessageLevel::Warning:
        msgIcon = u"/mIconWarning.svg"_s;
        break;
      case Qgis::MessageLevel::Success:
        msgIcon = u"/mIconSuccess.svg"_s;
        break;
      default:
        break;
    }
    icon = QgsApplication::getThemeIcon( msgIcon );
  }
  const int iconSize = std::max( 24.0, fontMetrics().height() * 1.2 );
  mLblIcon->setPixmap( icon.pixmap( iconSize ) );


  // STYLESHEETS
  const bool isDark = qApp && ( qApp->palette().color( QPalette::Window ).lightness() < 128 );
  QString contentStyleSheet;
  if ( mLevel == Qgis::MessageLevel::Success )
  {
    if ( isDark )
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #143324; border: 1px solid #166534; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #BBF7D0; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#4ADE80; }</style>"_s;
    }
    else
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #F0FDF4; border: 1px solid #86EFAC; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #166534; font-weight: 500; } "
      );
      contentStyleSheet = u"<style> a, a:visited, a:hover { color:#15803D; } </style>"_s;
    }
  }
  else if ( mLevel == Qgis::MessageLevel::Critical )
  {
    if ( isDark )
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #3F1D1D; border: 1px solid #991B1B; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #FECACA; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#F87171; }</style>"_s;
    }
    else
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #FEF2F2; border: 1px solid #FCA5A5; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #991B1B; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#B91C1C; }</style>"_s;
    }
  }
  else if ( mLevel == Qgis::MessageLevel::Warning )
  {
    if ( isDark )
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #382A13; border: 1px solid #92400E; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #FDE68A; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#FBBF24; }</style>"_s;
    }
    else
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #FFFBEB; border: 1px solid #FCD34D; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #92400E; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#B45309; }</style>"_s;
    }
  }
  else if ( mLevel == Qgis::MessageLevel::Info )
  {
    if ( isDark )
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #172B4D; border: 1px solid #2684FF; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #B3D4FF; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#4C9AFF; }</style>"_s;
    }
    else
    {
      mStyleSheet = QStringLiteral(
        "QgsMessageBar { background-color: #EFF6FF; border: 1px solid #BFDBFE; border-radius: 4px; } "
        "QLabel,QTextEdit { color: #1E40AF; font-weight: 500; } "
      );
      contentStyleSheet = u"<style>a, a:visited, a:hover { color:#2563EB; }</style>"_s;
    }
  }
  mStyleSheet += "QLabel#mItemCount { font-style: italic; }"_L1;

  // TITLE AND TEXT
  if ( mTitle.isEmpty() && mText.isEmpty() )
  {
    if ( mTextBrowser )
    {
      delete mTextBrowser;
      mTextBrowser = nullptr;
    }
  }
  else
  {
    if ( !mTextBrowser )
    {
      mTextBrowser = new QTextBrowser( this );
      mTextBrowser->setObjectName( u"textEdit"_s );
      mTextBrowser->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );
      mTextBrowser->setReadOnly( true );
      mTextBrowser->setOpenLinks( false );
      connect( mTextBrowser, &QTextBrowser::anchorClicked, this, &QgsMessageBarItem::urlClicked );

      mTextBrowser->setFrameShape( QFrame::NoFrame );
      // stylesheet set here so Qt-style substituted scrollbar arrows can show within limited height
      // adjusts to height of font set in app options
      mTextBrowser->setStyleSheet(
        "QTextEdit { background-color: rgba(0,0,0,0); margin-top: 0.25em; max-height: 1.75em; min-height: 1.75em; } "
        "QScrollBar { background-color: rgba(0,0,0,0); } "
        "QScrollBar::add-page,QScrollBar::sub-page,QScrollBar::handle { background-color: rgba(0,0,0,0); color: rgba(0,0,0,0); } "
        "QScrollBar::up-arrow,QScrollBar::down-arrow { color: rgb(0,0,0); } "
      );
      mLayout->addWidget( mTextBrowser );
    }
    QString content = mText;
    if ( !mTitle.isEmpty() )
    {
      // add ':' to end of title
      QString t = mTitle.trimmed();
      if ( !content.isEmpty() && !t.endsWith( ':' ) && !t.endsWith( ": "_L1 ) )
        t += ": "_L1;
      content.prepend( u"<b>"_s + t + " </b>" );
    }
    content.prepend( contentStyleSheet );
    mTextBrowser->setText( content );
  }

  // WIDGET
  if ( mWidget )
  {
    QLayoutItem *item = mLayout->itemAt( 2 );
    if ( !item || item->widget() != mWidget )
    {
      mLayout->addWidget( mWidget );
    }
  }
}

QgsMessageBarItem *QgsMessageBarItem::setText( const QString &text )
{
  mText = text;
  writeContent();
  return this;
}

QString QgsMessageBarItem::text() const
{
  return mText;
}

QgsMessageBarItem *QgsMessageBarItem::setTitle( const QString &title )
{
  mTitle = title;
  writeContent();
  return this;
}

QString QgsMessageBarItem::title() const
{
  return mTitle;
}

QgsMessageBarItem *QgsMessageBarItem::setLevel( Qgis::MessageLevel level )
{
  if ( level != mLevel )
  {
    mLevel = level;
    writeContent();
    emit styleChanged( mStyleSheet );
  }

  return this;
}

Qgis::MessageLevel QgsMessageBarItem::level() const
{
  return mLevel;
}

QgsMessageBarItem *QgsMessageBarItem::setWidget( QWidget *widget )
{
  if ( mWidget )
  {
    QLayoutItem *item = nullptr;
    item = mLayout->itemAt( 2 );
    if ( item->widget() == mWidget )
    {
      delete item->widget();
    }
  }
  mWidget = widget;
  writeContent();
  return this;
}

QWidget *QgsMessageBarItem::widget() const
{
  return mWidget;
}

QgsMessageBarItem *QgsMessageBarItem::setIcon( const QIcon &icon )
{
  mUserIcon = icon;
  return this;
}

QIcon QgsMessageBarItem::icon() const
{
  return mUserIcon;
}


QgsMessageBarItem *QgsMessageBarItem::setDuration( int duration )
{
  mDuration = duration;
  return this;
}

void QgsMessageBarItem::dismiss()
{
  if ( !mMessageBar )
    return;

  mMessageBar->popWidget( this );
}

void QgsMessageBarItem::urlClicked( const QUrl &url )
{
  const QFileInfo file( url.toLocalFile() );
  if ( file.exists() && !file.isDir() )
    QgsGui::nativePlatformInterface()->openFileExplorerAndSelectFile( url.toLocalFile() );
  else
    QDesktopServices::openUrl( url );
  dismiss();
}
