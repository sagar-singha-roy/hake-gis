/***************************************************************************
  qgshakeicons.h
  -------------------
  begin                : September 2026
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
#ifndef QGSHAKEICONS_H
#define QGSHAKEICONS_H

#include "qgis.h"
#include "qgis_app.h"
#include "qgshaketheme.h"

#include <QIcon>
#include <QList>
#include <QString>

class QObject;
class QWidget;
class QgsBrowserModel;

/**
 * Hake GeoDesk icon family for the application's own (Hake-owned) actions.
 *
 * Icons are SVGs compiled into the :/hake/icons resource. They are only used
 * under the "Hake Light" and "Hake Night" UI themes (light and dark color
 * substitution of the same SVGs) and only for the fixed set of targets listed
 * in qgshakeicons.cpp: core actions (including the snapping, shape digitizing and
 * annotation tools), ribbon panel dock toggles, Data Source Manager pages, Layer
 * Properties, Project Properties and Options dialog sidebar pages, Browser root items of
 * core providers, the Browser and Layers panel toolbars, app widgets addressed by a
 * semantic key (status bar, About, Welcome screen, layer tree context menu), and the
 * bundled plugin commands named in the Hake icon catalog.
 * Any other plugin or extension action keeps its own icon. Unmapped stock icons are
 * recolored to the same palette by QgsApplication::getThemeIcon().
 */
class APP_EXPORT QgsHakeIcons
{
  public:
    //! Returns TRUE if \a themeName is a UI theme the Hake icon family is designed for.
    static bool isHakeTheme( const QString &themeName );

    /**
     * Returns the Hake icon for \a resource (relative to :/hake/icons) in the colors of the
     * active UI theme (Hake Light colors for non-Hake themes), or a null icon if it is missing.
     */
    static QIcon icon( const QString &resource );

    //! Returns the Hake icon for \a resource in the colors of \a variant (Light for None).
    static QIcon icon( const QString &resource, QgsHakeTheme::Variant variant );

    /**
     * Applies the Hake icons to the mapped actions and dock toggles found under \a root when
     * \a themeName is a Hake theme, otherwise restores the icons those actions had before.
     */
    static void applyToActions( QObject *root, const QString &themeName );

    /**
     * Applies the Hake icons to the source pages of the Data Source Manager \a dialog when
     * \a themeName is a Hake theme, otherwise restores the provider icons.
     */
    static void applyToDataSourceManager( QWidget *dialog, const QString &themeName );

    /**
     * Applies the Hake icons to the shared sidebar pages of a layer properties \a dialog when
     * \a themeName is a Hake theme, otherwise restores the icons those pages had before.
     * Call after all pages, including factory-provided ones, have been added.
     */
    static void applyToLayerProperties( QWidget *dialog, const QString &themeName );

    /**
     * Applies the Hake icons to the application-owned sidebar pages of the Project Properties
     * \a dialog when \a themeName is a Hake theme. Pages registered by plugins keep their own icons.
     */
    static void applyToProjectProperties( QWidget *dialog, const QString &themeName );

    /**
     * Applies the Hake icons to the application-owned sidebar pages of the Options \a dialog when
     * \a themeName is a Hake theme, otherwise restores the icons those pages had before.
     * Pages registered by third-party plugins keep their own icons.
     * Call after all pages, including factory-provided ones, have been added.
     */
    static void applyToOptions( QWidget *dialog, const QString &themeName );

    /**
     * Re-applies the Hake icons to actions under \a root whenever an action is added to one of
     * \a menus (or their submenus), so plugins loaded or reloaded later are covered.
     */
    static void watchMenus( QObject *root, const QList<QWidget *> &menus );

    /**
     * Marks a panel toolbar \a action or button (anything with an "icon" property) to show the
     * Hake icon \a resource under the Hake themes. Takes effect on the next applyToPanel() call.
     */
    static void setPanelIcon( QObject *target, const QString &resource );

    /**
     * Applies the Hake icons to the toolbar actions and buttons of \a panel (and the actions of
     * its toolbars) marked with setPanelIcon() when \a themeName is a Hake theme, otherwise
     * restores the icons they had before.
     */
    static void applyToPanel( QWidget *panel, const QString &themeName );

    /**
     * Applies the Hake icons to the top-level Browser items of \a model whose provider the Hake
     * icon family covers when \a themeName is a Hake theme, otherwise restores the provider icons.
     * Items of other providers, including plugin ones, keep their own icons.
     */
    static void applyToBrowserModel( QgsBrowserModel *model, const QString &themeName );

    /**
     * Returns the Hake icon mapped to \a key (a semantic widget key such as "statusbar:crs", or a
     * core action objectName) under the Hake themes, otherwise the stock theme icon \a stockThemeIcon
     * (a null icon if empty). For icons set outside actions, or swapped at runtime.
     */
    static QIcon iconFor( const QString &key, const QString &stockThemeIcon );

    /**
     * Applies the geometry-specific Hake icons of the digitizing actions under \a root for an
     * active layer of \a geometryType. Call after the stock icons for that geometry were set.
     */
    static void applyGeometryIcons( QObject *root, Qgis::GeometryType geometryType );
};

#endif // QGSHAKEICONS_H
