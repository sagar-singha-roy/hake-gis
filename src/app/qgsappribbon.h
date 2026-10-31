/***************************************************************************
  qgsappribbon.h
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
#ifndef QGSAPPRIBBON_H
#define QGSAPPRIBBON_H

#include "qgis_app.h"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QTabWidget>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QMenuBar;
class QToolBar;
class QToolButton;
class QWidget;
class QgisApp;
class QgsAppRibbonGroup;
class QgsAppRibbonPage;
class QgsFloatingWidget;
class QgsLocatorWidget;

/**
 * Logical-pixel sizes for the ribbon, measured from the current font, style
 * and stylesheet rather than hard-coded, so they follow DPI and theme changes.
 */
struct QgsAppRibbonMetrics
{
    //! Icon size drawn in every command cell
    int cellIcon = 24;
    //! Side of the square cell every ribbon command occupies, on every tab
    int cellSize = 36;
    //! Width reserved beside the cell for the caret of a menu drop-down
    int caretWidth = 12;
    //! Height of a labelled large reference button; sets the command-area height
    int tallHeight = 52;
    int captionHeight = 14;
    int buttonAreaHeight = 52;
    //! Spacing scale shared by every ribbon page, group and separator
    int spaceXs = 2;
    int spaceSm = 4;
    int spaceMd = 6;
    int spaceLg = 8;
    //! Widest label text before eliding, for commands that cannot be shown icon-only
    int labelMax = 150;
    bool captions = true;

    bool operator==( const QgsAppRibbonMetrics &other ) const = default;
};

/**
 * Tabbed ribbon that reuses existing QgisApp QActions via QToolButton::setDefaultAction.
 * Does not take ownership of those actions.
 */
class APP_EXPORT QgsAppRibbon : public QTabWidget
{
    Q_OBJECT

  public:
    explicit QgsAppRibbon( QWidget *parent, QgisApp *app );

    //! Resolves dock toggle actions (Browser, Layers, Processing Toolbox) and plugin menus (Processing) created after the ribbon.
    void refreshOptionalActions();

    //! Rebuilds the groups so button styles and collapsed-group icons follow action icons changed by a theme switch.
    void refreshIcons();

    /**
     * Shows \a action as a compact icon button beside the brand at the top-right of the tab strip.
     * Does not take ownership of the action.
     */
    void setThemeToggleAction( QAction *action );

    /**
     * Presents the application \a locator in the tab strip, between the tabs and the theme toggle.
     * When the strip is too narrow it collapses to a search button that shows the same locator in
     * a floating overlay. The locator is reparented, never copied, and keeps its own search,
     * results and filters.
     */
    void setSearchWidget( QgsLocatorWidget *locator );

    //! Opens the search overlay when the search is collapsed to its button; does nothing otherwise.
    void revealSearch();

    //! Shows or hides the search entry point (field or button) in the tab strip.
    void setSearchVisible( bool visible );

    //! Returns TRUE unless the search entry point was hidden with setSearchVisible().
    bool isSearchVisible() const { return mSearchVisible; }

    //! Returns the tab-strip widget that hosts the search entry point, or NULLPTR without a search.
    QWidget *searchEntryWidget() const { return mSearchHost; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    bool event( QEvent *event ) override;
    void changeEvent( QEvent *event ) override;
    bool eventFilter( QObject *watched, QEvent *event ) override;
    void resizeEvent( QResizeEvent *event ) override;
    void showEvent( QShowEvent *event ) override;

  private:
    void syncChromeTabBarGeometry();
    //! Picks field or button for the search from the room left beside the tabs at \a stripWidth.
    void syncSearchGeometry( int stripWidth, int rowHeight );
    void setSearchCollapsed( bool collapsed );
    void hideSearchOverlay( bool restoreFocus );
    void updateMetrics();
    QgsAppRibbonPage *addPage( const QString &title );
    QgsAppRibbonGroup *addGroup( QgsAppRibbonPage *page, const QString &title );
    void addNamedAction( QgsAppRibbonGroup *group, const QString &objectName, bool primary = false );
    //! \a label is shown on the button only; the dock's toggle action text is left untouched.
    void addDockToggle( QgsAppRibbonGroup *group, const QString &dockObjectName, const QString &label, bool primary = false );
    //! Adds an existing menu as a drop-down button; hidden while the menu is empty.
    void addMenu( QgsAppRibbonGroup *group, QMenu *menu );
    //! Adds a menu created later (e.g. by a plugin), resolved by object name in refreshOptionalActions().
    void addDeferredMenu( QgsAppRibbonGroup *group, const QString &menuObjectName );
    void mirrorToolbar( QgsAppRibbonGroup *group, QToolBar *toolbar );
    void syncMirroredGroup( QToolBar *toolbar );

    /**
     * Watches the hidden classic menu bar: keeps top-level menu actions registered on the main
     * window (so their shortcuts work) and presents non-standard menus, such as those added by
     * installed extensions, in the Extensions tab.
     */
    void watchMenuBar( QMenuBar *menuBar );
    void syncMenuBar();
    bool isStandardMenu( const QMenu *menu ) const;

    QgisApp *mApp = nullptr;
    QLabel *mBrand = nullptr;
    QToolButton *mThemeToggle = nullptr;
    QWidget *mSearchHost = nullptr;
    QToolButton *mSearchButton = nullptr;
    QgsFloatingWidget *mSearchOverlay = nullptr;
    QPointer<QgsLocatorWidget> mSearchWidget;
    QPointer<QLineEdit> mSearchField;
    QPointer<QWidget> mFocusBeforeSearch;
    int mSearchMin = 0;
    int mSearchMax = 0;
    bool mSearchCollapsed = false;
    bool mSearchVisible = true;
    bool mSearchEscArmed = false;
    QList<QgsAppRibbonPage *> mPages;
    QHash<QToolBar *, QgsAppRibbonGroup *> mMirroredToolbars;
    QList<QPointer<QToolBar>> mPendingMirrorSyncs;
    QPointer<QMenuBar> mMenuBar;
    QgsAppRibbonGroup *mExtensionMenus = nullptr;
    QList<QPointer<QAction>> mAdoptedMenuBarActions;
    bool mMenuBarSyncPending = false;
    QgsAppRibbonMetrics mMetrics;
    int mCommandHeight = 0;
    bool mUpdatingMetrics = false;
};

#endif // QGSAPPRIBBON_H
