/***************************************************************************
  qgsappsidebar.cpp
  -------------------
  begin                : August 2026
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

#include "qgsappsidebar.h"

#include "qgsapplication.h"
#include "qgsguiutils.h"

#include <QAction>
#include <QSizePolicy>

#include "moc_qgsappsidebar.cpp"

using namespace Qt::StringLiterals;

QgsAppSidebar::QgsAppSidebar( QWidget *parent )
  : QToolBar( parent )
{
  setObjectName( u"HakeAppSidebar"_s );
  setWindowTitle( tr( "Navigation" ) );
  setMovable( false );
  setFloatable( false );
  setOrientation( Qt::Vertical );
  setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  setIconSize( QSize( QgsGuiUtils::scaleIconSize( 24 ), QgsGuiUtils::scaleIconSize( 24 ) ) );
  setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
  setMinimumWidth( QgsGuiUtils::scaleIconSize( 64 ) );
  setMaximumWidth( QgsGuiUtils::scaleIconSize( 72 ) );

  addNavAction( u"browser"_s, tr( "Geo Explorer" ), QgsApplication::getThemeIcon( u"/mIconFolder.svg"_s ), true );
  addNavAction( u"layers"_s, tr( "Layers" ), QgsApplication::getThemeIcon( u"/mActionLayers.svg"_s ), true );
  addNavAction( u"styles"_s, tr( "Styles" ), QgsApplication::getThemeIcon( u"/mActionStyleManager.svg"_s ), true );
  addNavAction( u"processing"_s, tr( "Processing" ), QgsApplication::getThemeIcon( u"/processingAlgorithm.svg"_s ), true );
  addNavAction( u"plugins"_s, tr( "Plugins" ), QgsApplication::getThemeIcon( u"/mActionShowPluginManager.svg"_s ), false );
  addNavAction( u"favorites"_s, tr( "Favorites" ), QgsApplication::getThemeIcon( u"/mIconFavorites.svg"_s ), false );

  // Keep icons at the top of the rail
  QWidget *spacer = new QWidget( this );
  spacer->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
  addWidget( spacer );
}

QAction *QgsAppSidebar::addNavAction( const QString &id, const QString &text, const QIcon &icon, bool checkable )
{
  QAction *action = addAction( icon, text );
  action->setObjectName( u"HakeSidebar_%1"_s.arg( id ) );
  action->setCheckable( checkable );
  action->setData( id );
  connect( action, &QAction::triggered, this, [this, id]( bool ) {
    emit itemActivated( id );
  } );
  mActions.insert( id, action );
  return action;
}

void QgsAppSidebar::setItemChecked( const QString &id, bool checked )
{
  if ( QAction *action = mActions.value( id ) )
  {
    action->setChecked( checked );
  }
}

bool QgsAppSidebar::isItemChecked( const QString &id ) const
{
  if ( const QAction *action = mActions.value( id ) )
  {
    return action->isChecked();
  }
  return false;
}
