/***************************************************************************
                        qgsactionlocatorfilters.cpp
                        ----------------------------
   begin                : May 2017
   copyright            : (C) 2017 by Nyall Dawson
   email                : nyall dot dawson at gmail dot com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsactionlocatorfilter.h"
#include "qgsappribbon.h"
#include "qgshakeicons.h"
#include "qgsstringutils.h"

#include <QAction>
#include <QMenu>
#include <QRegularExpression>
#include <QSet>
#include <QString>

#include "moc_qgsactionlocatorfilter.cpp"

using namespace Qt::StringLiterals;

QgsActionLocatorFilter::QgsActionLocatorFilter( QgsAppRibbon *ribbon, const QList<QWidget *> &parentObjectsForActions, QObject *parent )
  : QgsLocatorFilter( parent )
  , mRibbon( ribbon )
  , mActionParents( parentObjectsForActions )
{
  setUseWithoutPrefix( true );
}

QgsActionLocatorFilter::QgsActionLocatorFilter( const QList<QWidget *> &parentObjectsForActions, QObject *parent )
  : QgsLocatorFilter( parent )
  , mActionParents( parentObjectsForActions )
{
  setUseWithoutPrefix( true );
}

QgsActionLocatorFilter *QgsActionLocatorFilter::clone() const
{
  return new QgsActionLocatorFilter( mRibbon, mActionParents );
}

void QgsActionLocatorFilter::fetchResults( const QString &string, const QgsLocatorContext &, QgsFeedback *feedback )
{
  if ( string.trimmed().isEmpty() )
    return;

  QSet<QAction *> found;
  const QString trimmedSearch = string.trimmed();
  const QString unaccentedSearch = QgsStringUtils::unaccent( trimmedSearch );

  // 1. Search Ribbon commands across all tabs (Home, Data, Analysis, Map, Vector, Raster, Extensions, Mesh, Help)
  if ( mRibbon )
  {
    const QList<QgsAppRibbon::RibbonCommandInfo> cmds = mRibbon->ribbonCommands();
    for ( const QgsAppRibbon::RibbonCommandInfo &cmd : cmds )
    {
      if ( feedback && feedback->isCanceled() )
        return;

      QAction *action = cmd.action.data();
      if ( !action )
        continue;
      if ( found.contains( action ) )
        continue;

      QString primaryLabel = !cmd.ribbonLabel.isEmpty() ? cmd.ribbonLabel : action->text();
      primaryLabel.remove( '&' );
      primaryLabel.remove( u"…"_s );
      primaryLabel.remove( "..."_L1 );
      primaryLabel = primaryLabel.trimmed();

      QString actText = action->text();
      actText.remove( '&' );
      actText.remove( u"…"_s );
      actText.remove( "..."_L1 );
      actText = actText.trimmed();

      QString tooltip = action->toolTip();
      tooltip.remove( '&' );
      tooltip.remove( u"…"_s );
      tooltip.remove( "..."_L1 );
      tooltip = tooltip.trimmed();

      int score = 0;
      const QString unaccentedPrimary = QgsStringUtils::unaccent( primaryLabel );
      const QString unaccentedText = QgsStringUtils::unaccent( actText );
      const QString unaccentedTooltip = QgsStringUtils::unaccent( tooltip );

      if ( unaccentedPrimary.contains( unaccentedSearch, Qt::CaseInsensitive ) )
      {
        score = 100 + ( unaccentedPrimary.startsWith( unaccentedSearch, Qt::CaseInsensitive ) ? 50 : 0 );
      }
      else if ( unaccentedText.contains( unaccentedSearch, Qt::CaseInsensitive ) )
      {
        score = 80 + ( unaccentedText.startsWith( unaccentedSearch, Qt::CaseInsensitive ) ? 30 : 0 );
      }
      else if ( unaccentedTooltip.contains( unaccentedSearch, Qt::CaseInsensitive ) )
      {
        score = 60;
      }
      else
      {
        int f1 = fuzzyScore( unaccentedPrimary, unaccentedSearch );
        int f2 = fuzzyScore( unaccentedText, unaccentedSearch );
        score = std::max( f1, f2 );
      }

      // Check common synonyms e.g. "full extent" for "Zoom Full" or "Zoom to Full Extent"
      if ( score <= 0 )
      {
        if ( unaccentedSearch.contains( "full extent"_L1, Qt::CaseInsensitive ) && ( action->objectName() == "mActionZoomFullExtent"_L1 || actText.contains( "Full", Qt::CaseInsensitive ) ) )
          score = 120;
        else if ( unaccentedSearch.contains( "pan"_L1, Qt::CaseInsensitive ) && ( action->objectName() == "mActionPan"_L1 || actText.contains( "Pan", Qt::CaseInsensitive ) ) )
          score = 120;
      }

      if ( score > 0 )
      {
        found.insert( action );

        QgsLocatorResult result;
        QString display = primaryLabel;
        if ( !cmd.fullHierarchy.isEmpty() )
          display += u"  (%1)"_s.arg( cmd.fullHierarchy );
        if ( !action->isEnabled() )
          display += u" [%1]"_s.arg( tr( "Inactive" ) );

        result.displayString = display;
        result.setUserData( QVariant::fromValue( action ) );
        result.icon = !action->icon().isNull() ? action->icon() : QgsHakeIcons::icon( u"help/hake-help-tool-search.svg"_s );
        result.score = score;
        result.group = tr( "Ribbon Commands" );

        emit resultFetched( result );
      }
    }
  }

  // 2. Also search other parent objects (toolbars, menuBar) to catch any action not on the ribbon
  for ( QWidget *object : std::as_const( mActionParents ) )
  {
    if ( feedback && feedback->isCanceled() )
      return;
    searchActions( string, object, found );
  }
}

void QgsActionLocatorFilter::triggerResult( const QgsLocatorResult &result )
{
  QAction *action = qobject_cast<QAction *>( qvariant_cast<QObject *>( result.userData() ) );
  if ( action )
  {
    if ( mRibbon )
    {
      mRibbon->activateRibbonCommand( action );
    }
    else
    {
      action->trigger();
    }
  }
}

void QgsActionLocatorFilter::searchActions( const QString &string, QWidget *parent, QSet<QAction *> &found )
{
  if ( !parent )
    return;

  const QList<QWidget *> children = parent->findChildren<QWidget *>();
  for ( QWidget *widget : children )
  {
    searchActions( string, widget, found );
  }

  const thread_local QRegularExpression extractFromTooltip( u"<b>(.*)</b>"_s );
  const thread_local QRegularExpression newLineToSpace( u"[\\s\\n\\r]+"_s );

  const auto constActions = parent->actions();
  for ( QAction *action : constActions )
  {
    if ( action->menu() )
    {
      searchActions( string, action->menu(), found );
      continue;
    }

    if ( action->text().isEmpty() && action->toolTip().isEmpty() )
      continue;
    if ( found.contains( action ) )
      continue;

    QString searchText = action->text();
    searchText.replace( '&', QString() );

    QString tooltip = action->toolTip();
    tooltip.replace( newLineToSpace, u" "_s );
    QRegularExpressionMatch match = extractFromTooltip.match( tooltip );
    if ( match.hasMatch() )
    {
      tooltip = match.captured( 1 );
    }
    tooltip.replace( "..."_L1, QString() );
    tooltip.replace( QString( QChar( 0x2026 ) ), QString() );
    searchText.replace( "..."_L1, QString() );
    searchText.replace( QString( QChar( 0x2026 ) ), QString() );

    if ( searchText.isEmpty() )
      searchText = tooltip;

    bool uniqueTooltip = searchText.trimmed().compare( tooltip.trimmed(), Qt::CaseInsensitive ) != 0;
    if ( action->isChecked() )
    {
      searchText += u" [%1]"_s.arg( tr( "Active" ) );
    }
    if ( !action->isEnabled() )
    {
      searchText += u" [%1]"_s.arg( tr( "Inactive" ) );
    }
    if ( uniqueTooltip )
    {
      searchText += u" (%1)"_s.arg( tooltip.trimmed() );
    }

    QgsLocatorResult result;
    result.displayString = searchText;
    result.setUserData( QVariant::fromValue( action ) );
    result.icon = !action->icon().isNull() ? action->icon() : QgsHakeIcons::icon( u"help/hake-help-tool-search.svg"_s );

    const QString unaccentedDisplay = QgsStringUtils::unaccent( result.displayString );
    const QString unaccentedSearch = QgsStringUtils::unaccent( string.trimmed() );

    int score = 0;
    if ( unaccentedDisplay.contains( unaccentedSearch, Qt::CaseInsensitive ) )
      score = 70 + ( unaccentedDisplay.startsWith( unaccentedSearch, Qt::CaseInsensitive ) ? 20 : 0 );
    else
      score = fuzzyScore( unaccentedDisplay, unaccentedSearch );

    if ( score > 0 )
    {
      result.score = score;
      result.group = tr( "Actions" );
      found.insert( action );
      emit resultFetched( result );
    }
  }
}
