/***************************************************************************
  qgsappribbon.cpp
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

#include "qgsappribbon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsfloatingwidget.h"
#include "qgslocatorwidget.h"
#include "qgspythonrunner.h"
#include "qgshakeicons.h"
#include "qgshaketheme.h"
#include "qgssettings.h"
#include "qgsprocessinghistorywidget.h"
#include "processing/models/qgsmodeldesignerdialog.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QString>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>

#include "moc_qgsappribbon.cpp"

using namespace Qt::StringLiterals;

namespace
{
  QFont ribbonCaptionFont( const QFont &base )
  {
    QFont f = base;
    if ( f.pointSizeF() > 0 )
      f.setPointSizeF( f.pointSizeF() * 0.85 );
    else
      f.setPixelSize( std::max( 8, static_cast<int>( std::round( f.pixelSize() * 0.85 ) ) ) );
    f.setWeight( QFont::Medium );
    return f;
  }

  bool isPresentableAction( const QAction *action )
  {
    return action && !action->isSeparator() && !qobject_cast<const QWidgetAction *>( action );
  }

  //! The menu behind a QMenu::menuAction() drop-down, as opposed to a regular command that merely carries a menu.
  QMenu *dropDownMenu( const QAction *action )
  {
    QMenu *menu = action ? action->menu() : nullptr;
    return menu && menu->menuAction() == action ? menu : nullptr;
  }

  //! Visible, and for a menu drop-down, the menu has at least one visible command.
  bool isShowableCommand( const QAction *action )
  {
    if ( !action || !action->isVisible() )
      return false;
    if ( const QMenu *menu = dropDownMenu( action ) )
    {
      if ( menu->objectName() == "mWebMenu"_L1 || menu->objectName() == "mDatabaseMenu"_L1
           || menu->objectName() == "mVectorMenu"_L1 || menu->objectName() == "mRasterMenu"_L1 )
        return true;
      const QList<QAction *> actions = menu->actions();
      return std::any_of( actions.cbegin(), actions.cend(), []( const QAction *a ) { return !a->isSeparator() && a->isVisible(); } );
    }
    return true;
  }

  /**
   * Ribbon commands show only their icon; the action's tooltip (which QAction derives from
   * its text when unset) is the description. Commands without an icon or any description keep
   * their label so they never become an unidentifiable blank or unexplained button.
   */
  bool presentsIconOnly( const QAction *action )
  {
    return action && !action->icon().isNull() && !action->toolTip().isEmpty();
  }

  //! Toolbar widgets such as drop-down tool buttons are presented through their default action.
  QAction *presentableToolbarAction( QAction *action )
  {
    if ( auto *widgetAction = qobject_cast<QWidgetAction *>( action ) )
    {
      if ( auto *button = qobject_cast<QToolButton *>( widgetAction->defaultWidget() ) )
      {
        if ( button->defaultAction() )
          return button->defaultAction();
        if ( !button->actions().isEmpty() )
          return button->actions().first();
        if ( button->menu() )
          return button->menu()->menuAction();
      }
      return nullptr;
    }
    return action;
  }
  QString cleanActionLabel( const QAction *action, const QString &overrideLabel )
  {
    if ( !overrideLabel.isEmpty() )
      return overrideLabel;
    if ( !action )
      return QString();

    const QString obj = action->objectName();
    if ( obj == "mActionNewProject"_L1 ) return QObject::tr( "New" );
    if ( obj == "mActionOpenProject"_L1 ) return QObject::tr( "Open" );
    if ( obj == "mActionSaveProject"_L1 ) return QObject::tr( "Save" );
    if ( obj == "mActionSaveProjectAs"_L1 ) return QObject::tr( "Save As" );
    if ( obj == "mActionProjectProperties"_L1 ) return QObject::tr( "Properties" );
    if ( obj == "mActionExit"_L1 ) return QObject::tr( "Exit" );
    if ( obj == "mActionUndo"_L1 ) return QObject::tr( "Undo" );
    if ( obj == "mActionRedo"_L1 ) return QObject::tr( "Redo" );
    if ( obj == "mActionPan"_L1 ) return QObject::tr( "Pan Map" );
    if ( obj == "mActionZoomIn"_L1 ) return QObject::tr( "Zoom In" );
    if ( obj == "mActionZoomOut"_L1 ) return QObject::tr( "Zoom Out" );
    if ( obj == "mActionZoomFullExtent"_L1 ) return QObject::tr( "Full Extent" );
    if ( obj == "mActionDraw"_L1 ) return QObject::tr( "Refresh" );
    if ( obj == "mActionPanToSelected"_L1 ) return QObject::tr( "Pan Select" );
    if ( obj == "mActionZoomToSelected"_L1 ) return QObject::tr( "Zoom Select" );
    if ( obj == "mActionZoomToLayers"_L1 ) return QObject::tr( "Zoom Layer" );
    if ( obj == "mActionZoomActualSize"_L1 ) return QObject::tr( "100%" );
    if ( obj == "mActionZoomLast"_L1 ) return QObject::tr( "Previous" );
    if ( obj == "mActionZoomNext"_L1 ) return QObject::tr( "Next" );
    if ( obj == "mActionIdentify"_L1 ) return QObject::tr( "Identify" );
    if ( obj == "mActionOpenTable"_L1 ) return QObject::tr( "Attributes" );
    if ( obj == "mActionOptions"_L1 ) return QObject::tr( "Options" );
    if ( obj == "mActionStyleManager"_L1 ) return QObject::tr( "Styles" );
    if ( obj == "mActionCustomProjection"_L1 ) return QObject::tr( "Projections" );
    if ( obj == "mActionDataSourceManager"_L1 ) return QObject::tr( "Data Source" );
    if ( obj == "mActionAddOgrLayer"_L1 ) return QObject::tr( "Vector" );
    if ( obj == "mActionAddRasterLayer"_L1 ) return QObject::tr( "Raster" );
    if ( obj == "mActionAddMeshLayer"_L1 ) return QObject::tr( "Mesh" );
    if ( obj == "mActionAddDelimitedText"_L1 ) return QObject::tr( "Delimited" );
    if ( obj == "mActionAddSpatiaLiteLayer"_L1 ) return QObject::tr( "SpatiaLite" );
    if ( obj == "mActionAddVirtualLayer"_L1 ) return QObject::tr( "Virtual" );
    if ( obj == "mActionAddWmsLayer"_L1 ) return QObject::tr( "WMS / WMTS" );
    if ( obj == "mActionAddWfsLayer"_L1 ) return QObject::tr( "WFS" );
    if ( obj == "mActionLayerProperties"_L1 ) return QObject::tr( "Properties" );
    if ( obj == "mActionRemoveLayer"_L1 ) return QObject::tr( "Remove" );
    if ( obj == "mActionMeasure"_L1 ) return QObject::tr( "Line" );
    if ( obj == "mActionMeasureArea"_L1 ) return QObject::tr( "Area" );
    if ( obj == "mActionMeasureBearing"_L1 ) return QObject::tr( "Bearing" );
    if ( obj == "mActionMeasureAngle"_L1 ) return QObject::tr( "Angle" );
    if ( obj == "mActionStatisticalSummary"_L1 ) return QObject::tr( "Statistics" );
    if ( obj == "mActionOpenFieldCalc"_L1 ) return QObject::tr( "Field Calc" );
    if ( obj == "mActionShowPythonDialog"_L1 ) return QObject::tr( "Python" );
    if ( obj == "mActionToggleEditing"_L1 ) return QObject::tr( "Edit" );
    if ( obj == "mActionSaveLayerEdits"_L1 ) return QObject::tr( "Save Edits" );
    if ( obj == "mActionAllEdits"_L1 ) return QObject::tr( "All Edits" );
    if ( obj == "mActionAddFeature"_L1 ) return QObject::tr( "Add Feature" );
    if ( obj == "mActionDigitizeWithCurve"_L1 ) return QObject::tr( "Curve" );
    if ( obj == "mActionStreamDigitize"_L1 ) return QObject::tr( "Stream" );
    if ( obj == "mActionVertexTool"_L1 ) return QObject::tr( "Vertex Tool" );
    if ( obj == "mActionVertexToolActiveLayer"_L1 ) return QObject::tr( "Vertex" );
    if ( obj == "mActionMultiEditAttributes"_L1 ) return QObject::tr( "Multi-Edit" );
    if ( obj == "mActionMoveFeature"_L1 ) return QObject::tr( "Move" );
    if ( obj == "mActionDeleteSelected"_L1 ) return QObject::tr( "Delete" );
    if ( obj == "mActionCutFeatures"_L1 ) return QObject::tr( "Cut" );
    if ( obj == "mActionCopyFeatures"_L1 ) return QObject::tr( "Copy" );
    if ( obj == "mActionPasteFeatures"_L1 ) return QObject::tr( "Paste" );
    if ( obj == "mActionSnappingOptions"_L1 ) return QObject::tr( "Snapping" );
    if ( obj == "mActionSplitFeatures"_L1 ) return QObject::tr( "Split" );
    if ( obj == "mActionSelectFeatures"_L1 ) return QObject::tr( "Select" );
    if ( obj == "mActionSelectPolygon"_L1 ) return QObject::tr( "Polygon" );
    if ( obj == "mActionSelectFreehand"_L1 ) return QObject::tr( "Freehand" );
    if ( obj == "mActionSelectRadius"_L1 ) return QObject::tr( "Radius" );
    if ( obj == "mActionSelectByValue"_L1 ) return QObject::tr( "By Value" );
    if ( obj == "mActionSelectByExpression"_L1 ) return QObject::tr( "Expression" );
    if ( obj == "mActionDeselectAll"_L1 ) return QObject::tr( "Deselect" );
    if ( obj == "mActionInvertSelection"_L1 ) return QObject::tr( "Invert" );
    if ( obj == "mActionSelectAll"_L1 ) return QObject::tr( "Select All" );
    if ( obj == "mActionLabeling"_L1 ) return QObject::tr( "Labels" );
    if ( obj == "mActionMoveLabel"_L1 ) return QObject::tr( "Move Label" );
    if ( obj == "mActionRotateLabel"_L1 ) return QObject::tr( "Rotate" );
    if ( obj == "mActionShowPinnedLabels"_L1 ) return QObject::tr( "Pin Labels" );
    if ( obj == "mActionShowHideLabels"_L1 ) return QObject::tr( "Show/Hide" );
    if ( obj == "mActionLocalHistogramStretch"_L1 ) return QObject::tr( "Local" );
    if ( obj == "mActionFullHistogramStretch"_L1 ) return QObject::tr( "Full" );
    if ( obj == "mActionLocalCumulativeCutStretch"_L1 ) return QObject::tr( "Cumulative" );
    if ( obj == "mActionFullCumulativeCutStretch"_L1 ) return QObject::tr( "2% - 98%" );
    if ( obj == "mActionIncreaseBrightness"_L1 ) return QObject::tr( "Bright +" );
    if ( obj == "mActionDecreaseBrightness"_L1 ) return QObject::tr( "Bright -" );
    if ( obj == "mActionIncreaseContrast"_L1 ) return QObject::tr( "Contrast +" );
    if ( obj == "mActionDecreaseContrast"_L1 ) return QObject::tr( "Contrast -" );
    if ( obj == "mActionIncreaseGamma"_L1 ) return QObject::tr( "Gamma +" );
    if ( obj == "mActionDecreaseGamma"_L1 ) return QObject::tr( "Gamma -" );
    if ( obj == "mActionManagePlugins"_L1 ) return QObject::tr( "Manage" );
    if ( obj == "mActionNewMeshLayer"_L1 ) return QObject::tr( "New Mesh" );
    if ( obj == "mActionShowMeshCalculator"_L1 ) return QObject::tr( "Calculator" );
    if ( obj == "mActionNewMapCanvas"_L1 ) return QObject::tr( "New View" );
    if ( obj == "mActionNew3DMapCanvas"_L1 ) return QObject::tr( "3D View" );
    if ( obj == "mActionNew3DMapCanvasGlobe"_L1 ) return QObject::tr( "Globe 3D" );
    if ( obj == "mActionNewBookmark"_L1 ) return QObject::tr( "New" );
    if ( obj == "mActionShowBookmarks"_L1 ) return QObject::tr( "Show" );
    if ( obj == "mActionTemporalController"_L1 ) return QObject::tr( "Temporal" );
    if ( obj == "mActionToggleFullScreen"_L1 ) return QObject::tr( "Full Screen" );
    if ( obj == "mActionHelpAPI"_L1 ) return QObject::tr( "API Docs" );
    if ( obj == "mActionAbout"_L1 ) return QObject::tr( "About" );
    if ( obj == "mActionQgisHomePage"_L1 ) return QObject::tr( "Home Page" );
    if ( obj == "mActionNewPrintLayout"_L1 ) return QObject::tr( "New Layout" );
    if ( obj == "mActionShowLayoutManager"_L1 ) return QObject::tr( "Layouts" );
    if ( obj == "mActionShowRasterCalculator"_L1 ) return QObject::tr( "Calculator" );
    if ( obj == "mActionToolSearch"_L1 ) return QObject::tr( "Search" );
    if ( obj == "mActionShowDbManager"_L1 || obj == "mActionDbManager"_L1 || action->text().contains( "DB Manager", Qt::CaseInsensitive ) ) return QObject::tr( "Database" );
    if ( obj == "mActionShowMetaSearch"_L1 || obj == "action_run"_L1 || action->text().contains( "MetaSearch", Qt::CaseInsensitive ) ) return QObject::tr( "Web" );
    if ( obj == "mVectorMenu"_L1 || action->text().contains( "Vector", Qt::CaseInsensitive ) ) return QObject::tr( "Vector" );
    if ( obj == "mRasterMenu"_L1 || action->text().contains( "Raster", Qt::CaseInsensitive ) ) return QObject::tr( "Raster" );
    if ( obj == "mViewMenu"_L1 || action->text().contains( "View", Qt::CaseInsensitive ) ) return QObject::tr( "View" );
    if ( obj == "mMeshMenu"_L1 || action->text().contains( "Mesh", Qt::CaseInsensitive ) ) return QObject::tr( "Mesh" );
    if ( obj == "mHelpMenu"_L1 || obj == "mActionHelpContents"_L1 || action->text().contains( "Help", Qt::CaseInsensitive ) ) return QObject::tr( "Help" );
    if ( obj == "mPluginMenu"_L1 || action->text().contains( "Plugin", Qt::CaseInsensitive ) || action->text().contains( "Extension", Qt::CaseInsensitive ) ) return QObject::tr( "Extensions" );

    QString text = action->text();
    int tabIdx = text.indexOf( '\t' );
    if ( tabIdx != -1 )
      text = text.left( tabIdx );
    text.remove( '&' );
    text.remove( u"…"_s );
    text.remove( "..."_L1 );
    text.replace( "Hake GeoDesk – Desktop GIS", QString() );
    text.replace( "Hake GeoDesk", QString() );
    text = text.trimmed();
    if ( text.startsWith( "Show " ) )
      text = text.mid( 5 );
    if ( text.startsWith( "Toggle " ) )
      text = text.mid( 7 );
    if ( text.startsWith( "Add " ) )
      text = text.mid( 4 );
    if ( text.contains( "Circle", Qt::CaseInsensitive ) ) return QObject::tr( "Circle" );
    if ( text.contains( "Rectangle", Qt::CaseInsensitive ) ) return QObject::tr( "Rectangle" );
    if ( text.contains( "Ellipse", Qt::CaseInsensitive ) ) return QObject::tr( "Ellipse" );
    if ( text.contains( "Regular Polygon", Qt::CaseInsensitive ) ) return QObject::tr( "Polygon" );
    if ( text.contains( "Circular String", Qt::CaseInsensitive ) ) return QObject::tr( "Curve" );
    return text.trimmed();
  }
} // namespace

// Group caption that elides instead of forcing its group wider than its buttons.
// Color comes from the theme QSS (#HakeAppRibbonGroupCaption) via the palette.
class QgsAppRibbonCaption : public QWidget
{
  public:
    QgsAppRibbonCaption( const QString &text, QWidget *parent )
      : QWidget( parent )
      , mText( text )
    {
      setObjectName( u"HakeAppRibbonGroupCaption"_s );
      setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
      setAttribute( Qt::WA_TransparentForMouseEvents, true );
    }

    QSize sizeHint() const override
    {
      const QFontMetrics fm( font() );
      return QSize( fm.horizontalAdvance( mText ) + fm.averageCharWidth() * 2, fm.height() );
    }

    QSize minimumSizeHint() const override { return QSize( 0, QFontMetrics( font() ).height() ); }

  protected:
    void paintEvent( QPaintEvent * ) override
    {
      QPainter p( this );
      const bool isNight = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night );
      p.setPen( isNight ? QColor( 148, 163, 184 ) : QColor( 96, 125, 148 ) );
      const QString shown = fontMetrics().elidedText( mText, Qt::ElideRight, width() );
      p.drawText( rect(), Qt::AlignHCenter | Qt::AlignVCenter, shown );
    }

  private:
    QString mText;
};

/**
 * The ribbon command button: presents a crisp vector icon at the top with a clear,
 * concise label directly beneath it (TextUnderIcon style), matching professional desktop
 * GIS suites. Shows active states cleanly without shifting geometry.
 */
class QgsAppRibbonButton : public QToolButton
{
  public:
    QgsAppRibbonButton( QWidget *parent, const QgsAppRibbonMetrics &metrics )
      : QToolButton( parent )
      , mButtonHeight( metrics.buttonAreaHeight )
      , mCaret( metrics.caretWidth )
    {
      setObjectName( u"HakeAppRibbonButton"_s );
      setAutoRaise( true );
      // Reachable by keyboard, but a mouse click must not steal focus from the map canvas.
      setFocusPolicy( Qt::TabFocus );
      setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
      setIconSize( QSize( 28, 28 ) );
      setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
      setFixedHeight( mButtonHeight );
    }

    void setDefaultAction( QAction *action )
    {
      QToolButton::setDefaultAction( action );
      if ( action )
      {
        connect( action, &QAction::toggled, this, [this]( bool ) { update(); } );
        connect( action, &QAction::changed, this, [this] { update(); } );
      }
    }

    void mousePressEvent( QMouseEvent *e ) override
    {
      QToolButton::mousePressEvent( e );
      update();
    }

    void mouseReleaseEvent( QMouseEvent *e ) override
    {
      QToolButton::mouseReleaseEvent( e );
      update();
    }

    //! Shown instead of the action text; the action itself is not modified.
    void setLabel( const QString &label )
    {
      mLabel = label;
      updateGeometry();
    }

    void setDropDown( bool dropDown )
    {
      mDropDown = dropDown;
      if ( dropDown )
        setPopupMode( QToolButton::InstantPopup );
    }

    //! Shows only the icon; otherwise the label under the icon is painted.
    void setIconOnly( bool iconOnly )
    {
      mIconOnly = iconOnly;
      updateGeometry();
    }

    void setCompact( bool compact )
    {
      mCompact = compact;
      updateGeometry();
    }

    QString shownLabel() const
    {
      if ( mIconOnly )
        return QString();
      return cleanActionLabel( defaultAction(), mLabel );
    }

    QSize sizeHint() const override
    {
      const QString lbl = shownLabel();
      if ( lbl.isEmpty() )
        return QSize( 38 + ( mDropDown ? mCaret : 0 ), mButtonHeight );

      QFont f = font();
      f.setPixelSize( 10 );
      const QFontMetrics fm( f );
      int textW = fm.horizontalAdvance( lbl );
      if ( mCompact )
      {
        int w = std::clamp( textW + 8, 42, 84 );
        return QSize( w + ( mDropDown ? mCaret : 0 ), mButtonHeight );
      }
      int w = std::clamp( textW + 12, 48, 98 );
      return QSize( w + ( mDropDown ? mCaret : 0 ), mButtonHeight );
    }

    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent( QPaintEvent * ) override
    {
      QPainter p( this );
      QStyleOptionToolButton opt;
      initStyleOption( &opt );

      bool active = isChecked() || ( defaultAction() && defaultAction()->isChecked() );
      if ( active && QgisApp::instance() && QgisApp::instance()->isWelcomeScreenVisible() )
      {
        active = false;
      }

      const bool isNight = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night );

      QColor bg = Qt::transparent;
      QColor border = Qt::transparent;
      QColor textColor;

      if ( active )
      {
        if ( isNight )
        {
          bg = QColor( 56, 189, 248, 55 );
          border = QColor( 56, 189, 248 );
          textColor = QColor( 56, 189, 248 );
        }
        else
        {
          bg = QColor( 214, 228, 244 );
          border = QColor( 22, 74, 115 );
          textColor = QColor( 22, 74, 115 );
        }
      }
      else if ( isDown() || ( opt.state & QStyle::State_Sunken ) )
      {
        if ( isNight )
        {
          bg = QColor( 15, 23, 42 );
          border = QColor( 56, 189, 248, 120 );
          textColor = QColor( 241, 245, 249 );
        }
        else
        {
          bg = QColor( 226, 234, 243 );
          border = QColor( 160, 185, 210 );
          textColor = QColor( 22, 74, 115 );
        }
      }
      else if ( ( opt.state & QStyle::State_MouseOver ) && isEnabled() )
      {
        if ( isNight )
        {
          bg = QColor( 30, 41, 59 );
          border = QColor( 51, 65, 85 );
          textColor = QColor( 241, 245, 249 );
        }
        else
        {
          bg = QColor( 241, 245, 249 );
          border = QColor( 203, 213, 225 );
          textColor = QColor( 15, 23, 42 );
        }
      }
      else
      {
        if ( isNight )
          textColor = isEnabled() ? QColor( 226, 232, 240 ) : QColor( 100, 116, 139 );
        else
          textColor = isEnabled() ? QColor( 51, 65, 85 ) : QColor( 156, 163, 175 );
      }

      if ( bg != Qt::transparent || border != Qt::transparent )
      {
        p.save();
        p.setRenderHint( QPainter::Antialiasing );
        p.setBrush( bg );
        p.setPen( QPen( border, active ? 1.5 : 1.0 ) );
        p.drawRoundedRect( rect().adjusted( 1, 1, -1, -1 ), 4, 4 );
        p.restore();
      }

      QIcon::Mode mode = QIcon::Normal;
      if ( !isEnabled() )
        mode = QIcon::Disabled;
      else if ( ( opt.state & QStyle::State_MouseOver ) && ( opt.state & QStyle::State_AutoRaise ) )
        mode = QIcon::Active;
      const QIcon::State state = active ? QIcon::On : QIcon::Off;

      const QString text = shownLabel();
      const int contentW = width() - ( mDropDown ? mCaret : 0 );
      const int iconX = ( contentW - iconSize().width() ) / 2;
      const int iconY = 3;
      const QRect iconRect( iconX, iconY, iconSize().width(), iconSize().height() );
      if ( !icon().isNull() )
      {
        icon().paint( &p, iconRect, Qt::AlignCenter, mode, state );
      }
      else
      {
        QIcon fallback = QgsHakeIcons::icon( u"app/hake-theme-sun.svg"_s );
        if ( mDropDown )
        {
          if ( text.contains( "Vector", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-vector.svg"_s );
          else if ( text.contains( "Raster", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-raster.svg"_s );
          else if ( text.contains( "Web", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s );
          else if ( text.contains( "Database", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-database.svg"_s );
          else if ( text.contains( "View", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-view.svg"_s );
          else if ( text.contains( "Mesh", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-mesh.svg"_s );
          else if ( text.contains( "Help", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-help.svg"_s );
          else if ( text.contains( "Extension", Qt::CaseInsensitive ) || text.contains( "Plugin", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-plugins.svg"_s );
          else if ( text.contains( "Layer", Qt::CaseInsensitive ) )
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-layer.svg"_s );
          else
            fallback = QgsHakeIcons::icon( u"menus/hake-menu-processing.svg"_s );
        }
        if ( !fallback.isNull() )
          fallback.paint( &p, iconRect, Qt::AlignCenter, mode, state );
      }

      if ( !text.isEmpty() )
      {
        p.save();
        QFont f = font();
        f.setPixelSize( 10 );
        if ( active )
          f.setWeight( QFont::DemiBold );
        else
          f.setWeight( QFont::Medium );
        p.setFont( f );
        p.setPen( textColor );
        const QRect textRect( 2, iconY + iconSize().height() + 2, contentW - 4, height() - ( iconY + iconSize().height() + 4 ) );
        p.drawText( textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, text );
        p.restore();
      }

      if ( mDropDown )
      {
        const QRect caretRect( width() - mCaret, 0, mCaret, height() );
        p.setPen( textColor );
        p.drawText( caretRect, Qt::AlignCenter, u"\u25BE"_s );
      }
    }

  private:
    int mButtonHeight = 54;
    int mCaret = 0;
    QString mLabel;
    bool mDropDown = false;
    bool mIconOnly = false;
    bool mCompact = false;
};

//! Subtle vertical group separator, inset from the page edges; color from the theme QSS via the palette.
class QgsAppRibbonSeparator : public QWidget
{
  public:
    explicit QgsAppRibbonSeparator( QWidget *parent )
      : QWidget( parent )
    {
      setObjectName( u"HakeAppRibbonSeparator"_s );
      setFixedWidth( 1 );
      setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
      setAttribute( Qt::WA_TransparentForMouseEvents, true );
    }

    void setInsets( int top, int bottom )
    {
      mTop = top;
      mBottom = bottom;
      update();
    }

  protected:
    void paintEvent( QPaintEvent * ) override
    {
      QPainter p( this );
      const bool isNight = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night );
      p.fillRect( QRect( 0, mTop, width(), std::max( 0, height() - mTop - mBottom ) ),
                  isNight ? QColor( 42, 59, 80 ) : QColor( 216, 223, 229 ) );
    }

  private:
    int mTop = 0;
    int mBottom = 0;
};

class QgsAppRibbonGroup : public QWidget
{
  public:
    enum Level
    {
      Full,
      Compact,
      IconsOnly,
      Collapsed,
    };

    struct Entry
    {
        QPointer<QAction> action;
        bool primary = false;
        //! Dock whose toggleViewAction() fills this entry once the dock exists
        QString dockObjectName;
        //! Menu whose menuAction() fills this entry once the menu exists
        QString menuObjectName;
        //! Action whose objectName fills this entry once created
        QString actionObjectName;
        //! Button label overriding the action text for presentation only
        QString label;
    };

    QgsAppRibbonGroup( const QString &title, QWidget *parent );

    QString title() const { return mTitle; }
    void addEntry( const Entry &entry );
    void setEntries( const QList<Entry> &entries );
    bool resolveDeferred( QObject *root );
    QList<QAction *> commands() const;
    bool hasCommands() const { return !commands().isEmpty(); }
    const QList<Entry> &entries() const { return mEntries; }

    void setMetrics( const QgsAppRibbonMetrics &metrics );
    void rebuild();
    void scheduleRebuild();

    bool isToolsPalette() const { return mToolsPalette; }
    void setToolsPalette( bool toolsPalette )
    {
      mToolsPalette = toolsPalette;
      scheduleRebuild();
    }

    Level level() const { return mLevel; }
    void setLevel( Level level );
    int widthForLevel( Level level ) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    void watchAction( QAction *action );
    void actionChanged();
    QWidget *buildVariant( Level level );
    QgsAppRibbonButton *makeButton( QWidget *parent, const Entry &entry ) const;

    QString mTitle;
    QList<Entry> mEntries;
    QgsAppRibbonMetrics mMetrics;
    Level mLevel = Full;
    QVBoxLayout *mLayout = nullptr;
    QWidget *mContent = nullptr;
    QHBoxLayout *mContentLayout = nullptr;
    QgsAppRibbonCaption *mCaption = nullptr;
    std::array<QWidget *, 4> mVariants { { nullptr, nullptr, nullptr, nullptr } };
    //! Icon-only decision each action was last built with
    QHash<const QAction *, bool> mBuiltIconOnly;
    bool mRebuildPending = false;
    bool mToolsPalette = false;
};

class QgsAppRibbonPage : public QWidget
{
  public:
    explicit QgsAppRibbonPage( QWidget *parent );

    QgsAppRibbonGroup *addGroup( const QString &title );
    const QList<QgsAppRibbonGroup *> &groups() const { return mGroups; }
    void setMetrics( const QgsAppRibbonMetrics &metrics );
    void relayout();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void resizeEvent( QResizeEvent *event ) override;
    void showEvent( QShowEvent *event ) override;

  private:
    int requiredWidth( const QVector<QgsAppRibbonGroup::Level> &levels, const QVector<bool> &hidden, bool overflow ) const;

    QHBoxLayout *mLayout = nullptr;
    QList<QgsAppRibbonGroup *> mGroups;
    QList<QgsAppRibbonSeparator *> mSeparators;
    QToolButton *mOverflow = nullptr;
    QMenu *mOverflowMenu = nullptr;
    bool mInRelayout = false;
};

//
// QgsAppRibbonGroup
//

QgsAppRibbonGroup::QgsAppRibbonGroup( const QString &title, QWidget *parent )
  : QWidget( parent )
  , mTitle( title )
{
  setObjectName( u"HakeAppRibbonGroup"_s );
  setAccessibleName( title );
  setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );

  mLayout = new QVBoxLayout( this );
  mLayout->setContentsMargins( 0, 0, 0, 0 );
  mLayout->setSpacing( 0 );

  mContent = new QWidget( this );
  mContentLayout = new QHBoxLayout( mContent );
  mContentLayout->setContentsMargins( 0, 0, 0, 0 );
  mContentLayout->setSpacing( 0 );
  mLayout->addWidget( mContent, 0 );

  mCaption = new QgsAppRibbonCaption( title, this );
  mLayout->addWidget( mCaption, 0 );
  mLayout->addStretch( 1 );
}

void QgsAppRibbonGroup::watchAction( QAction *action )
{
  if ( !action )
    return;
  connect( action, &QAction::visibleChanged, this, &QgsAppRibbonGroup::scheduleRebuild, Qt::UniqueConnection );
  // Icons can arrive after the ribbon is built (plugin menus, theme switches).
  connect( action, &QAction::changed, this, &QgsAppRibbonGroup::actionChanged, Qt::UniqueConnection );
  // Menu drop-downs appear/disappear as plugins populate or empty the menu.
  if ( QMenu *menu = dropDownMenu( action ) )
    menu->installEventFilter( this );
}

void QgsAppRibbonGroup::actionChanged()
{
  // Enabled/checked changes are frequent; only a change of presentation needs a rebuild.
  const auto *action = qobject_cast<const QAction *>( sender() );
  const auto it = mBuiltIconOnly.constFind( action );
  if ( it != mBuiltIconOnly.constEnd() && *it != presentsIconOnly( action ) )
    scheduleRebuild();
}

bool QgsAppRibbonGroup::eventFilter( QObject *watched, QEvent *event )
{
  if ( ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved ) && qobject_cast<QMenu *>( watched ) )
    scheduleRebuild();
  return QWidget::eventFilter( watched, event );
}

void QgsAppRibbonGroup::addEntry( const Entry &entry )
{
  if ( entry.action && !isPresentableAction( entry.action ) )
    return;
  if ( !entry.action && entry.dockObjectName.isEmpty() && entry.menuObjectName.isEmpty() && entry.actionObjectName.isEmpty() )
    return;
  for ( const Entry &existing : mEntries )
  {
    if ( entry.action && existing.action == entry.action )
      return;
    if ( !entry.actionObjectName.isEmpty() && existing.actionObjectName == entry.actionObjectName )
      return;
    if ( !entry.dockObjectName.isEmpty() && existing.dockObjectName == entry.dockObjectName )
      return;
    if ( !entry.menuObjectName.isEmpty() && existing.menuObjectName == entry.menuObjectName )
      return;
  }
  mEntries.append( entry );
  if ( entry.action )
    watchAction( entry.action );
}

void QgsAppRibbonGroup::setEntries( const QList<Entry> &entries )
{
  mEntries.clear();
  for ( const Entry &entry : entries )
    addEntry( entry );
  scheduleRebuild();
}

bool QgsAppRibbonGroup::resolveDeferred( QObject *root )
{
  bool resolved = false;
  for ( Entry &entry : mEntries )
  {
    if ( !entry.dockObjectName.isEmpty() )
    {
      if ( QDockWidget *dock = root->findChild<QDockWidget *>( entry.dockObjectName ) )
      {
        QAction *toggle = dock->toggleViewAction();
        if ( entry.action != toggle )
        {
          entry.action = toggle;
          if ( entry.dockObjectName == "ProcessingToolbox"_L1 )
          {
            entry.action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
            if ( entry.label.isEmpty() )
              entry.label = tr( "Processing Toolbox" );
          }
          watchAction( entry.action );
          resolved = true;
        }
      }
    }
    else if ( !entry.menuObjectName.isEmpty() )
    {
      if ( QMenu *menu = root->findChild<QMenu *>( entry.menuObjectName ) )
      {
        QAction *act = menu->menuAction();
        if ( entry.action != act )
        {
          entry.action = act;
          if ( entry.menuObjectName == "processing"_L1 )
          {
            entry.action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
            if ( entry.label.isEmpty() )
              entry.label = tr( "Processing" );
          }
          watchAction( entry.action );
          resolved = true;
        }
      }
    }
    else if ( !entry.actionObjectName.isEmpty() )
    {
      if ( QAction *act = root->findChild<QAction *>( entry.actionObjectName ) )
      {
        if ( entry.action != act )
        {
          entry.action = act;
          if ( entry.actionObjectName == "mActionShowPythonDialog"_L1 )
          {
            entry.action->setIcon( QgsHakeIcons::icon( u"analysis/hake-analysis-python-console.svg"_s ) );
            if ( entry.label.isEmpty() )
              entry.label = tr( "Python" );
          }
          watchAction( entry.action );
          resolved = true;
        }
      }
      else if ( entry.actionObjectName == "mActionShowPythonDialog"_L1 )
      {
        if ( auto *app = qobject_cast<QgisApp *>( root ) )
        {
          if ( QAction *py = app->actionShowPythonDialog() )
          {
            if ( entry.action != py )
            {
              entry.action = py;
              entry.action->setIcon( QgsHakeIcons::icon( u"analysis/hake-analysis-python-console.svg"_s ) );
              if ( entry.label.isEmpty() )
                entry.label = tr( "Python" );
              watchAction( entry.action );
              resolved = true;
            }
          }
        }
      }
      else if ( entry.actionObjectName == "mActionShowMetaSearch"_L1 )
      {
        for ( QAction *act : root->findChildren<QAction *>() )
        {
          if ( act->objectName() == "action_run"_L1 && act != entry.action )
          {
            entry.action = act;
            entry.action->setIcon( QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s ) );
            entry.label = tr( "Web" );
            watchAction( entry.action );
            resolved = true;
            break;
          }
        }
      }
      else if ( entry.actionObjectName == "mActionShowDbManager"_L1 )
      {
        for ( QAction *act : root->findChildren<QAction *>() )
        {
          if ( act->objectName() == "mActionDbManager"_L1 || act->text().contains( "DB Manager", Qt::CaseInsensitive ) )
          {
            if ( entry.action != act )
            {
              entry.action = act;
              entry.action->setIcon( QgsHakeIcons::icon( u"database/hake-database-db-manager.svg"_s ) );
              entry.label = tr( "Database" );
              watchAction( entry.action );
              resolved = true;
            }
            break;
          }
        }
      }
    }

    if ( entry.action )
    {
      if ( entry.dockObjectName == "ProcessingToolbox"_L1 && ( entry.action->icon().isNull() || entry.label != tr( "Processing Toolbox" ) ) )
      {
        entry.action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
        entry.label = tr( "Processing Toolbox" );
        resolved = true;
      }
      if ( entry.menuObjectName == "processing"_L1 && entry.action->icon().isNull() )
      {
        entry.action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
        if ( entry.label.isEmpty() )
          entry.label = tr( "Processing" );
        resolved = true;
      }
      if ( entry.actionObjectName == "mActionShowPythonDialog"_L1 && entry.action->icon().isNull() )
      {
        entry.action->setIcon( QgsHakeIcons::icon( u"analysis/hake-analysis-python-console.svg"_s ) );
        if ( entry.label.isEmpty() )
          entry.label = tr( "Python" );
        resolved = true;
      }
      if ( entry.actionObjectName == "mActionShowMetaSearch"_L1 && ( entry.action->icon().isNull() || entry.label != tr( "Web" ) ) )
      {
        entry.action->setIcon( QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s ) );
        entry.label = tr( "Web" );
        resolved = true;
      }
      if ( entry.actionObjectName == "mActionShowDbManager"_L1 && ( entry.action->icon().isNull() || entry.label != tr( "Database" ) ) )
      {
        entry.action->setIcon( QgsHakeIcons::icon( u"database/hake-database-db-manager.svg"_s ) );
        entry.label = tr( "Database" );
        resolved = true;
      }
      if ( auto *app = qobject_cast<QgisApp *>( root ) )
      {
        if ( app->vectorMenu() && entry.action == app->vectorMenu()->menuAction() && entry.action->icon().isNull() )
        {
          entry.action->setIcon( QgsHakeIcons::icon( u"menus/hake-menu-vector.svg"_s ) );
          resolved = true;
        }
        if ( app->rasterMenu() && entry.action == app->rasterMenu()->menuAction() && entry.action->icon().isNull() )
        {
          entry.action->setIcon( QgsHakeIcons::icon( u"menus/hake-menu-raster.svg"_s ) );
          resolved = true;
        }
      }
    }
  }
  return resolved;
}

QList<QAction *> QgsAppRibbonGroup::commands() const
{
  QList<QAction *> list;
  for ( const Entry &entry : mEntries )
  {
    if ( isShowableCommand( entry.action ) )
      list << entry.action;
  }
  return list;
}

void QgsAppRibbonGroup::setMetrics( const QgsAppRibbonMetrics &metrics )
{
  mMetrics = metrics;
  // Every group shares the same command-area height and caption row, so command rows
  // and captions sit on the same baselines on every tab.
  mLayout->setContentsMargins( metrics.spaceSm, 0, metrics.spaceSm, 0 );
  mLayout->setSpacing( metrics.spaceXs );
  mContent->setFixedHeight( metrics.buttonAreaHeight );
  mCaption->setFixedHeight( metrics.captionHeight );
  rebuild();
}

void QgsAppRibbonGroup::scheduleRebuild()
{
  if ( mRebuildPending )
    return;
  mRebuildPending = true;
  QTimer::singleShot( 0, this, [this] {
    mRebuildPending = false;
    rebuild();
    if ( auto *page = dynamic_cast<QgsAppRibbonPage *>( parentWidget() ) )
      page->relayout();
  } );
}

void QgsAppRibbonGroup::rebuild()
{
  // deleteLater: a rebuild can be triggered while one of these buttons is still
  // inside its own click handler (e.g. an action that changes the theme).
  for ( QWidget *&variant : mVariants )
  {
    if ( variant )
    {
      variant->hide();
      variant->deleteLater();
      variant = nullptr;
    }
  }

  mBuiltIconOnly.clear();
  for ( const Entry &entry : std::as_const( mEntries ) )
  {
    if ( entry.action )
      mBuiltIconOnly.insert( entry.action, presentsIconOnly( entry.action ) );
  }

  mCaption->setFont( ribbonCaptionFont( font() ) );
  for ( int l = Full; l <= Collapsed; ++l )
  {
    mVariants[l] = buildVariant( static_cast<Level>( l ) );
    mContentLayout->addWidget( mVariants[l], 0, Qt::AlignLeft | Qt::AlignTop );
    mVariants[l]->ensurePolished();
  }
  setLevel( mLevel );
}

QgsAppRibbonButton *QgsAppRibbonGroup::makeButton( QWidget *parent, const Entry &entry ) const
{
  auto *button = new QgsAppRibbonButton( parent, mMetrics );
  button->setDefaultAction( entry.action );
  button->setLabel( entry.label );
  QMenu *menu = dropDownMenu( entry.action );
  if ( !menu && entry.action && entry.action->menu() )
    menu = entry.action->menu();
  if ( menu )
  {
    button->setDropDown( true );
    button->setMenu( menu );
  }
  else
  {
    button->setDropDown( false );
  }
  const bool iconOnly = ( mLevel == IconsOnly );
  button->setIconOnly( iconOnly );
  return button;
}

QWidget *QgsAppRibbonGroup::buildVariant( Level level )
{
  const QgsAppRibbonMetrics &m = mMetrics;

  // One row of equal cells, vertically centred in the shared command area, so every icon on
  // every tab sits on the same centre line.
  auto *variant = new QWidget( mContent );
  variant->setFixedHeight( m.buttonAreaHeight );
  auto *row = new QHBoxLayout( variant );
  row->setContentsMargins( 0, 0, 0, 0 );
  row->setSpacing( m.spaceXs );

  QList<Entry> visibleEntries;
  for ( const Entry &entry : std::as_const( mEntries ) )
  {
    if ( isShowableCommand( entry.action ) )
      visibleEntries << entry;
  }
  if ( visibleEntries.isEmpty() )
    return variant;

  if ( level == Collapsed )
  {
    // The whole group behind one drop-down cell; the title is its tooltip and accessible name.
    auto *button = new QgsAppRibbonButton( variant, m );
    button->setText( mTitle );
    button->setToolTip( mTitle );
    button->setAccessibleName( mTitle );
    button->setIcon( visibleEntries.first().action ? visibleEntries.first().action->icon() : QIcon() );
    button->setDropDown( true );
    button->setIconOnly( true );
    auto *menu = new QMenu( button );
    menu->addActions( commands() );
    button->setMenu( menu );
    row->addWidget( button, 0, Qt::AlignLeft | Qt::AlignVCenter );
    return variant;
  }

  for ( const Entry &entry : std::as_const( visibleEntries ) )
  {
    auto *btn = makeButton( variant, entry );
    if ( level == IconsOnly )
      btn->setIconOnly( true );
    if ( level == Compact )
      btn->setCompact( true );
    row->addWidget( btn, 0, Qt::AlignVCenter );
  }
  return variant;
}

void QgsAppRibbonGroup::setLevel( Level level )
{
  mLevel = level;
  for ( int l = Full; l <= Collapsed; ++l )
  {
    if ( mVariants[l] )
      mVariants[l]->setVisible( l == level );
  }
  mCaption->setVisible( mMetrics.captions && level != Collapsed );
  updateGeometry();
}

int QgsAppRibbonGroup::widthForLevel( Level level ) const
{
  const QWidget *variant = mVariants[level];
  if ( !variant )
    return 0;
  const int padding = 2 * mMetrics.spaceSm;
  const int content = variant->sizeHint().width();
  if ( !mMetrics.captions || level == Collapsed )
    return content + padding;
  const int caption = mCaption->sizeHint().width();
  if ( level == IconsOnly )
    return std::max( content, std::min( caption, mCaption->fontMetrics().averageCharWidth() * 6 ) ) + padding;
  return std::max( content, caption ) + padding;
}

QSize QgsAppRibbonGroup::sizeHint() const
{
  return QSize( widthForLevel( mLevel ), QWidget::sizeHint().height() );
}

QSize QgsAppRibbonGroup::minimumSizeHint() const
{
  return QSize( widthForLevel( mLevel ), 0 );
}

//
// QgsAppRibbonPage
//

QgsAppRibbonPage::QgsAppRibbonPage( QWidget *parent )
  : QWidget( parent )
{
  setObjectName( u"HakeAppRibbonPage"_s );
  setAttribute( Qt::WA_StyledBackground, true );
  setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );

  mLayout = new QHBoxLayout( this );
  // Gaps between groups come from each group's own padding, so separators sit evenly between them.
  mLayout->setSpacing( 0 );
  mLayout->addStretch( 1 );

  mOverflowMenu = new QMenu( this );
  mOverflow = new QToolButton( this );
  mOverflow->setText( u"\u00BB"_s );
  mOverflow->setToolTip( QObject::tr( "More commands" ) );
  mOverflow->setAccessibleName( QObject::tr( "More commands" ) );
  mOverflow->setToolButtonStyle( Qt::ToolButtonTextOnly );
  mOverflow->setAutoRaise( true );
  mOverflow->setFocusPolicy( Qt::TabFocus );
  mOverflow->setPopupMode( QToolButton::InstantPopup );
  mOverflow->setMenu( mOverflowMenu );
  mOverflow->hide();
  mLayout->addWidget( mOverflow, 0, Qt::AlignVCenter );
}

QgsAppRibbonGroup *QgsAppRibbonPage::addGroup( const QString &title )
{
  auto *group = new QgsAppRibbonGroup( title, this );
  auto *separator = new QgsAppRibbonSeparator( this );

  // Keep the trailing stretch and overflow button at the end.
  const int insertAt = mLayout->count() - 2;
  mLayout->insertWidget( insertAt, group );
  mLayout->insertWidget( insertAt + 1, separator );
  mGroups << group;
  mSeparators << separator;
  return group;
}

void QgsAppRibbonPage::setMetrics( const QgsAppRibbonMetrics &metrics )
{
  // +1 bottom margin reserves the page's 1px bottom border drawn by the theme.
  mLayout->setContentsMargins( metrics.spaceMd, metrics.spaceSm, metrics.spaceMd, metrics.spaceXs + 1 );
  mOverflow->setFixedHeight( metrics.cellSize );
  for ( QgsAppRibbonSeparator *separator : std::as_const( mSeparators ) )
    separator->setInsets( metrics.spaceSm, metrics.spaceXs );
  for ( QgsAppRibbonGroup *group : std::as_const( mGroups ) )
    group->setMetrics( metrics );
  relayout();
}

QSize QgsAppRibbonPage::sizeHint() const
{
  const QVector<QgsAppRibbonGroup::Level> levels( mGroups.size(), QgsAppRibbonGroup::Full );
  const QVector<bool> hidden( mGroups.size(), false );
  return QSize( requiredWidth( levels, hidden, false ), 0 );
}

QSize QgsAppRibbonPage::minimumSizeHint() const
{
  // Height is owned by QgsAppRibbon; width always adapts through relayout().
  return QSize( 0, 0 );
}

int QgsAppRibbonPage::requiredWidth( const QVector<QgsAppRibbonGroup::Level> &levels, const QVector<bool> &hidden, bool overflow ) const
{
  const QMargins margins = mLayout->contentsMargins();
  int width = margins.left() + margins.right();
  int items = 0;
  int visibleGroups = 0;
  for ( int i = 0; i < mGroups.size(); ++i )
  {
    if ( hidden[i] || !mGroups[i]->hasCommands() )
      continue;
    width += mGroups[i]->widthForLevel( levels[i] );
    ++visibleGroups;
    ++items;
  }
  if ( visibleGroups > 1 )
  {
    width += ( visibleGroups - 1 ) * mSeparators.first()->minimumWidth();
    items += visibleGroups - 1;
  }
  if ( overflow )
  {
    width += mOverflow->sizeHint().width();
    ++items;
  }
  if ( items > 1 )
    width += ( items - 1 ) * mLayout->spacing();
  return width;
}

void QgsAppRibbonPage::relayout()
{
  if ( mInRelayout || mGroups.isEmpty() )
    return;
  mInRelayout = true;

  const int available = width();
  const int n = static_cast<int>( mGroups.size() );
  QVector<QgsAppRibbonGroup::Level> levels( n, QgsAppRibbonGroup::Full );
  QVector<bool> hidden( n, false );
  bool overflow = false;

  auto fits = [&] { return requiredWidth( levels, hidden, overflow ) <= available; };

  // Compress one level at a time, rightmost group first, so the most-used
  // commands on the left stay expanded longest.
  // Always preserve text labels under icons for all visible groups (never downgrade to IconsOnly).
  bool done = fits();
  for ( int level : { QgsAppRibbonGroup::Compact, QgsAppRibbonGroup::Collapsed } )
  {
    for ( int i = n - 1; !done && i >= 0; --i )
    {
      // A collapsed drop-down can be as wide as a small group's expanded
      // layout; only collapse when it saves space.
      const bool saves = level != QgsAppRibbonGroup::Collapsed || mGroups[i]->widthForLevel( QgsAppRibbonGroup::Collapsed ) < mGroups[i]->widthForLevel( levels[i] );
      if ( levels[i] < level && saves )
      {
        levels[i] = static_cast<QgsAppRibbonGroup::Level>( level );
        done = fits();
      }
    }
  }
  if ( !done )
  {
    overflow = true;
    for ( int i = n - 1; i >= 0 && !fits(); --i )
      hidden[i] = true;
  }

  const QList<QMenu *> oldSubMenus = mOverflowMenu->findChildren<QMenu *>( Qt::FindDirectChildrenOnly );
  mOverflowMenu->clear();
  for ( QMenu *subMenu : oldSubMenus )
    subMenu->deleteLater();

  int lastVisible = -1;
  for ( int i = 0; i < n; ++i )
  {
    QgsAppRibbonGroup *group = mGroups[i];
    const bool hasCommands = group->hasCommands();
    group->setLevel( levels[i] );
    group->setVisible( hasCommands && !hidden[i] );
    if ( !hasCommands )
      continue;
    if ( hidden[i] )
    {
      QMenu *subMenu = mOverflowMenu->addMenu( group->title() );
      subMenu->addActions( group->commands() );
    }
    else
    {
      lastVisible = i;
    }
  }
  for ( int i = 0; i < n; ++i )
    mSeparators[i]->setVisible( mGroups[i]->isVisibleTo( this ) && i < lastVisible );
  mOverflow->setVisible( overflow );

  mInRelayout = false;
}

void QgsAppRibbonPage::resizeEvent( QResizeEvent *event )
{
  QWidget::resizeEvent( event );
  if ( event->size().width() != event->oldSize().width() )
    relayout();
}

void QgsAppRibbonPage::showEvent( QShowEvent *event )
{
  QWidget::showEvent( event );
  relayout();
}

class QgsAppRibbonTabBar : public QTabBar
{
  public:
    explicit QgsAppRibbonTabBar( QWidget *parent = nullptr )
      : QTabBar( parent )
    {
      setAttribute( Qt::WA_StyledBackground, true );
      setDrawBase( false );
      setElideMode( Qt::ElideNone );
      setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    }

  protected:
    void paintEvent( QPaintEvent *event ) override
    {
      Q_UNUSED( event )
      QPainter p( this );
      p.setRenderHint( QPainter::Antialiasing );

      const int current = currentIndex();
      const int nTabs = count();

      for ( int i = 0; i < nTabs; ++i )
      {
        const QRect rect = tabRect( i );
        if ( !rect.isValid() || rect.width() <= 0 )
          continue;

        const bool isSelected = ( i == current );

        QFont f = font();
        if ( isSelected )
          f.setWeight( QFont::DemiBold );
        else
          f.setWeight( QFont::Medium );
        p.setFont( f );

        const bool isNight = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night );
        const QColor activeColor = isNight ? QColor( 56, 189, 248 ) : QColor( 22, 74, 115 ); // #38BDF8 Cyan in Dark, #164A73 Navy in Light
        const QColor inactiveColor = isNight ? QColor( 148, 163, 184 ) : QColor( 96, 125, 148 ); // #94A3B8 in Dark, #607D94 in Light
        const QColor textColor = isSelected ? activeColor : inactiveColor;

        if ( isSelected )
        {
          // Active Tab indicator: solid 3px line at bottom
          p.fillRect( QRect( rect.left() + 6, rect.bottom() - 2, rect.width() - 12, 3 ), activeColor );
        }

        const QIcon icon = tabIcon( i );
        int textLeft = rect.left();
        if ( !icon.isNull() )
        {
          const QSize isize = iconSize();
          const QRect irect( rect.left() + 8, rect.top() + ( rect.height() - isize.height() ) / 2, isize.width(), isize.height() );
          icon.paint( &p, irect, Qt::AlignCenter, isSelected ? QIcon::Active : QIcon::Normal );
          textLeft = irect.right() + 6;
        }

        p.setPen( textColor );
        const QRect textRect( textLeft, rect.top(), rect.right() - textLeft, rect.height() - ( isSelected ? 3 : 0 ) );
        p.drawText( textRect, Qt::AlignCenter | Qt::TextShowMnemonic, tabText( i ) );
      }
    }
};

//
// QgsAppRibbon
//

QgsAppRibbon::QgsAppRibbon( QWidget *parent, QgisApp *app )
  : QTabWidget( parent )
  , mApp( app )
{
  setTabBar( new QgsAppRibbonTabBar( this ) );
  setObjectName( u"HakeAppRibbon"_s );
  setDocumentMode( true );
  setMovable( false );
  setUsesScrollButtons( true );
  connect( this, &QTabWidget::currentChanged, this, [this]( int ) {
    refreshOptionalActions();
    if ( tabBar() )
      tabBar()->update();
  } );
  // Height comes from measured metrics (sizeHint), never from a fixed pixel value.
  setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  // Fusion leaves the empty region after the last tab unpainted unless QSS backgrounds are forced.
  // WA_StyledBackground is Qt-portable (Wayland-safe); do not use platform window APIs here.
  setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAutoFillBackground( true );
  tabBar()->setExpanding( false );
  tabBar()->setDrawBase( false );
  tabBar()->setElideMode( Qt::ElideNone );
  tabBar()->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );

  // Brand at the right end of the tab strip; the tab bar is sized to end where it begins.
  auto *tabFiller = new QWidget( this );
  tabFiller->setObjectName( u"HakeAppRibbonTabFiller"_s );
  tabFiller->setAttribute( Qt::WA_StyledBackground, true );
  tabFiller->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  auto *fillerLayout = new QHBoxLayout( tabFiller );
  fillerLayout->setContentsMargins( 0, 0, 0, 0 );
  mBrand = new QLabel( u"HAKE GEOSPATIAL"_s, tabFiller );
  mBrand->setObjectName( u"HakeAppRibbonBrand"_s );
  mBrand->setAccessibleName( tr( "Hake Geospatial" ) );
  fillerLayout->addWidget( mBrand, 0, Qt::AlignVCenter );
  setCornerWidget( tabFiller, Qt::TopRightCorner );

  if ( !mApp )
    return;

  // The ribbon tab strip is the only navigation row: exactly these nine tabs, in this order.
  // Classic menus (Project, Edit, View, Layer, Settings, Plugins, ...) are not tabs; they are
  // exposed as drop-downs inside the matching tab.

  // Home: project, editing, navigation and application settings
  {
    QgsAppRibbonPage *page = addPage( tr( "Home" ) );
    QgsAppRibbonGroup *project = addGroup( page, tr( "Project" ) );
    addNamedAction( project, u"mActionNewProject"_s, true );
    addNamedAction( project, u"mActionOpenProject"_s, true );
    addNamedAction( project, u"mActionSaveProject"_s, true );
    addNamedAction( project, u"mActionSaveProjectAs"_s );
    addNamedAction( project, u"mActionProjectProperties"_s );
    addNamedAction( project, u"mActionExit"_s );

    QgsAppRibbonGroup *edit = addGroup( page, tr( "Editing" ) );
    addNamedAction( edit, u"mActionUndo"_s );
    addNamedAction( edit, u"mActionRedo"_s );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );
    addNamedAction( navigation, u"mActionDraw"_s );

    QgsAppRibbonGroup *identify = addGroup( page, tr( "Identify" ) );
    addNamedAction( identify, u"mActionIdentify"_s, true );
    addNamedAction( identify, u"mActionOpenTable"_s );

    QgsAppRibbonGroup *application = addGroup( page, tr( "Application" ) );
    addNamedAction( application, u"mActionOptions"_s, true );
    addNamedAction( application, u"mActionStyleManager"_s );
    addNamedAction( application, u"mActionCustomProjection"_s );
  }

  // Data: layer sources, layouts, database and web services
  {
    QgsAppRibbonPage *page = addPage( tr( "Data" ) );
    QgsAppRibbonGroup *layers = addGroup( page, tr( "Layers" ) );
    addNamedAction( layers, u"mActionDataSourceManager"_s, true );
    addNamedAction( layers, u"mActionAddOgrLayer"_s );
    addNamedAction( layers, u"mActionAddRasterLayer"_s );
    addNamedAction( layers, u"mActionAddMeshLayer"_s );
    addNamedAction( layers, u"mActionAddDelimitedText"_s );
    addNamedAction( layers, u"mActionAddSpatiaLiteLayer"_s );
    addNamedAction( layers, u"mActionAddVirtualLayer"_s );
    addNamedAction( layers, u"mActionAddWmsLayer"_s );
    addNamedAction( layers, u"mActionAddWfsLayer"_s );
    addNamedAction( layers, u"mActionLayerProperties"_s );
    addNamedAction( layers, u"mActionRemoveLayer"_s );
    addNamedAction( layers, u"mActionOpenTable"_s );

    QgsAppRibbonGroup *layout = addGroup( page, tr( "Layout" ) );
    addNamedAction( layout, u"mActionNewPrintLayout"_s, true );
    addNamedAction( layout, u"mActionShowLayoutManager"_s );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );

    QgsAppRibbonGroup *services = addGroup( page, tr( "Services" ) );
    addMenu( services, mApp->databaseMenu(), tr( "Database" ) );
    addMenu( services, mApp->webMenu(), tr( "Web" ) );

    // Plugins (e.g. DB Manager, MetaSearch) populate these toolbars at runtime.
    QgsAppRibbonGroup *dbGroup = addGroup( page, tr( "Database" ) );
    addNamedAction( dbGroup, u"mActionShowDbManager"_s, true );
    mirrorToolbar( dbGroup, mApp->databaseToolBar() );

    QgsAppRibbonGroup *webGroup = addGroup( page, tr( "Web" ) );
    addNamedAction( webGroup, u"mActionShowMetaSearch"_s, true );
    mirrorToolbar( webGroup, mApp->webToolBar() );
  }

  // Analysis
  {
    QgsAppRibbonPage *page = addPage( tr( "Analysis" ) );

    QgsAppRibbonGroup *measure = addGroup( page, tr( "Measure" ) );
    addNamedAction( measure, u"mActionMeasure"_s, true );
    addNamedAction( measure, u"mActionMeasureArea"_s );
    addNamedAction( measure, u"mActionMeasureBearing"_s );
    addNamedAction( measure, u"mActionMeasureAngle"_s );

    QgsAppRibbonGroup *statistics = addGroup( page, tr( "Statistics" ) );
    addNamedAction( statistics, u"mActionStatisticalSummary"_s );
    addNamedAction( statistics, u"mActionOpenFieldCalc"_s );

    QgsAppRibbonGroup *processing = addGroup( page, tr( "Processing" ) );
    addDockToggle( processing, u"ProcessingToolbox"_s, tr( "Processing Toolbox" ), true );
    addNamedAction( processing, u"mActionShowPythonDialog"_s, true );
    addDeferredMenu( processing, u"processing"_s );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );
  }

  // Map: navigation, map views, bookmarks and panels
  {
    QgsAppRibbonPage *page = addPage( tr( "Map" ) );
    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );
    addNamedAction( navigation, u"mActionDraw"_s );
    addNamedAction( navigation, u"mActionPanToSelected"_s );
    addNamedAction( navigation, u"mActionZoomToSelected"_s );
    addNamedAction( navigation, u"mActionZoomToLayers"_s );
    addNamedAction( navigation, u"mActionZoomActualSize"_s );
    addNamedAction( navigation, u"mActionZoomLast"_s );
    addNamedAction( navigation, u"mActionZoomNext"_s );

    QgsAppRibbonGroup *mapViews = addGroup( page, tr( "Map Views" ) );
    addNamedAction( mapViews, u"mActionNewMapCanvas"_s, true );

    QgsAppRibbonGroup *views3d = addGroup( page, tr( "3D" ) );
    addNamedAction( views3d, u"mActionNew3DMapCanvas"_s, true );
    addNamedAction( views3d, u"mActionNew3DMapCanvasGlobe"_s, true );

    QgsAppRibbonGroup *bookmarks = addGroup( page, tr( "Bookmarks" ) );
    addNamedAction( bookmarks, u"mActionNewBookmark"_s, true );
    addNamedAction( bookmarks, u"mActionShowBookmarks"_s );

    QgsAppRibbonGroup *panels = addGroup( page, tr( "Panels" ) );
    addDockToggle( panels, u"Browser"_s, tr( "Geo Explorer" ) );
    addDockToggle( panels, u"Layers"_s, tr( "GIS Layer" ) );
    addNamedAction( panels, u"mActionTemporalController"_s );
    addNamedAction( panels, u"mActionToggleFullScreen"_s );
    addMenu( panels, mApp->viewMenu(), tr( "View" ) );
  }

  // Vector
  {
    QgsAppRibbonPage *page = addPage( tr( "Vector" ) );

    QgsAppRibbonGroup *edit = addGroup( page, tr( "Edit" ) );
    addNamedAction( edit, u"mActionToggleEditing"_s );
    addNamedAction( edit, u"mActionSaveLayerEdits"_s );
    addNamedAction( edit, u"mActionAllEdits"_s );
    addNamedAction( edit, u"mActionUndo"_s );
    addNamedAction( edit, u"mActionRedo"_s );

    QgsAppRibbonGroup *digitize = addGroup( page, tr( "Digitizing" ) );
    addNamedAction( digitize, u"mActionAddFeature"_s );
    addNamedAction( digitize, u"mActionDigitizeWithCurve"_s );
    addNamedAction( digitize, u"mActionStreamDigitize"_s );
    if ( mApp->shapeDigitizeToolBar() )
    {
      const QList<QAction *> shapeActions = mApp->shapeDigitizeToolBar()->actions();
      for ( QAction *act : shapeActions )
      {
        if ( QAction *presented = presentableToolbarAction( act ) )
          digitize->addEntry( { presented, false, QString(), QString(), QString(), QString() } );
      }
    }

    QgsAppRibbonGroup *modify = addGroup( page, tr( "Modify" ) );
    addNamedAction( modify, u"mActionVertexTool"_s );
    addNamedAction( modify, u"mActionSplitFeatures"_s );
    addNamedAction( modify, u"mActionSnappingOptions"_s );
    addNamedAction( modify, u"mActionDeleteSelected"_s );
    addNamedAction( modify, u"mActionCutFeatures"_s );
    addNamedAction( modify, u"mActionCopyFeatures"_s );
    addNamedAction( modify, u"mActionPasteFeatures"_s );

    QgsAppRibbonGroup *selection = addGroup( page, tr( "Selection" ) );
    addNamedAction( selection, u"mActionSelectFeatures"_s );
    addNamedAction( selection, u"mActionSelectPolygon"_s );
    addNamedAction( selection, u"mActionSelectFreehand"_s );
    addNamedAction( selection, u"mActionSelectByExpression"_s );
    addNamedAction( selection, u"mActionDeselectAll"_s );
    addNamedAction( selection, u"mActionSelectAll"_s );

    QgsAppRibbonGroup *labels = addGroup( page, tr( "Labels" ) );
    addNamedAction( labels, u"mActionLabeling"_s );
    addNamedAction( labels, u"mActionMoveLabel"_s );
    addNamedAction( labels, u"mActionRotateLabel"_s );
    addNamedAction( labels, u"mActionShowHideLabels"_s );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addMenu( tools, mApp->vectorMenu(), tr( "Vector" ) );
  }

  // Raster
  {
    QgsAppRibbonPage *page = addPage( tr( "Raster" ) );

    QgsAppRibbonGroup *stretch = addGroup( page, tr( "Stretch" ) );
    addNamedAction( stretch, u"mActionLocalHistogramStretch"_s );
    addNamedAction( stretch, u"mActionFullHistogramStretch"_s );
    addNamedAction( stretch, u"mActionLocalCumulativeCutStretch"_s );
    addNamedAction( stretch, u"mActionFullCumulativeCutStretch"_s );

    QgsAppRibbonGroup *adjust = addGroup( page, tr( "Adjust" ) );
    addNamedAction( adjust, u"mActionIncreaseBrightness"_s );
    addNamedAction( adjust, u"mActionDecreaseBrightness"_s );
    addNamedAction( adjust, u"mActionIncreaseContrast"_s );
    addNamedAction( adjust, u"mActionDecreaseContrast"_s );
    addNamedAction( adjust, u"mActionIncreaseGamma"_s );
    addNamedAction( adjust, u"mActionDecreaseGamma"_s );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addMenu( tools, mApp->rasterMenu(), tr( "Raster" ) );
  }

  // Extensions: extension management (existing plugin manager), plugin toolbar actions,
  // and top-level menus that installed extensions (e.g. HCMGIS) add at runtime.
  {
    QgsAppRibbonPage *page = addPage( tr( "Extensions" ) );
    QgsAppRibbonGroup *manage = addGroup( page, tr( "Manage" ) );
    addNamedAction( manage, u"mActionManagePlugins"_s, true );
    addMenu( manage, mApp->pluginMenu(), tr( "Extensions" ) );

    QgsAppRibbonGroup *installed = addGroup( page, tr( "Installed" ) );
    mirrorToolbar( installed, mApp->pluginToolBar() );

    mExtensionMenus = addGroup( page, tr( "Installed Menus" ) );
  }

  // Mesh
  {
    QgsAppRibbonPage *page = addPage( tr( "Mesh" ) );
    QgsAppRibbonGroup *layers = addGroup( page, tr( "Layers" ) );
    addNamedAction( layers, u"mActionAddMeshLayer"_s, true );
    addNamedAction( layers, u"mActionNewMeshLayer"_s );

    mirrorToolbar( addGroup( page, tr( "Digitizing" ) ), mApp->meshToolBar() );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addNamedAction( tools, u"mActionShowMeshCalculator"_s, true );
    addMenu( tools, mApp->meshMenu(), tr( "Mesh" ) );
  }

  // Help
  {
    QgsAppRibbonPage *page = addPage( tr( "Help" ) );
    QgsAppRibbonGroup *help = addGroup( page, tr( "Help" ) );
    addMenu( help, mApp->helpMenu(), tr( "Help" ) );
    addNamedAction( help, u"mActionToolSearch"_s );

    QgsAppRibbonGroup *documentation = addGroup( page, tr( "Documentation" ) );
    addNamedAction( documentation, u"mActionHelpAPI"_s );

    QgsAppRibbonGroup *about = addGroup( page, tr( "About" ) );
    addNamedAction( about, u"mActionAbout"_s, true );
    addNamedAction( about, u"mActionQgisHomePage"_s );
  }

  watchMenuBar( mApp->menuBar() );
  refreshOptionalActions();
  loadUserHeightOffset();
  updateMetrics();
}

QSize QgsAppRibbon::sizeHint() const
{
  return QSize( tabBar()->sizeHint().width(), tabBar()->sizeHint().height() + mCommandHeight );
}

QSize QgsAppRibbon::minimumSizeHint() const
{
  return QSize( QTabWidget::minimumSizeHint().width(), tabBar()->sizeHint().height() + mCommandHeight );
}

void QgsAppRibbon::updateMetrics()
{
  if ( mUpdatingMetrics || mPages.isEmpty() )
    return;
  mUpdatingMetrics = true;

  ensurePolished();
  const QStyle *st = style();
  QgsAppRibbonMetrics m;
  const int toolBarIcon = st->pixelMetric( QStyle::PM_ToolBarIconSize, nullptr, this );
  m.cellIcon = std::clamp( toolBarIcon, 20, 24 );
  const QFontMetrics fm = fontMetrics();
  m.spaceXs = std::max( 2, fm.height() / 8 );
  m.spaceSm = 2 * m.spaceXs;
  m.spaceMd = 3 * m.spaceXs;
  m.spaceLg = 4 * m.spaceXs;
  m.labelMax = fm.averageCharWidth() * 22;
  m.caretWidth = fm.horizontalAdvance( u"\u25BE"_s ) + m.spaceSm;

  QFont tabFont = font();
  tabFont.setWeight( QFont::Medium );
  tabBar()->setFont( tabFont );

  // Measure real, stylesheet-polished ribbon buttons (a page child picks up the
  // command-area QSS padding and border) instead of guessing sizes.
  {
    QPixmap pixmap( toolBarIcon, toolBarIcon );
    pixmap.fill( Qt::transparent );
    // A labelled large button defines the command-area height (the approved ribbon density),
    // even though commands are presented as icon-only cells.
    QToolButton heightProbe( mPages.first() );
    heightProbe.setObjectName( u"HakeAppRibbonButton"_s );
    heightProbe.setAutoRaise( true );
    heightProbe.setIcon( QIcon( pixmap ) );
    heightProbe.setText( u"Wg"_s );
    heightProbe.setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    heightProbe.setIconSize( QSize( toolBarIcon, toolBarIcon ) );
    m.tallHeight = heightProbe.sizeHint().height();

    // The cell is the icon plus the theme's button chrome and an even inset, squared.
    QToolButton cellProbe( mPages.first() );
    cellProbe.setObjectName( u"HakeAppRibbonButton"_s );
    cellProbe.setAutoRaise( true );
    cellProbe.setIcon( QIcon( pixmap ) );
    cellProbe.setToolButtonStyle( Qt::ToolButtonIconOnly );
    cellProbe.setIconSize( QSize( m.cellIcon, m.cellIcon ) );
    const QSize chromed = cellProbe.sizeHint();
    m.cellSize = std::min( std::max( chromed.width(), chromed.height() ) + 2 * m.spaceXs, m.tallHeight );
  }
  m.captionHeight = QFontMetrics( ribbonCaptionFont( font() ) ).height();

  if ( mBrand )
  {
    QFont brandFont = ribbonCaptionFont( font() );
    brandFont.setWeight( QFont::DemiBold );
    brandFont.setLetterSpacing( QFont::AbsoluteSpacing, 1.2 );
    mBrand->setFont( brandFont );
    mBrand->parentWidget()->layout()->setContentsMargins( fm.averageCharWidth() * 2, 0, fm.averageCharWidth() * 2, 0 );
    mBrand->parentWidget()->layout()->setSpacing( fm.averageCharWidth() );

    // Sized from the brand caption so these buttons never make the tab strip taller.
    const int iconSize = QFontMetrics( brandFont ).height();
    for ( QToolButton *button : { mThemeToggle, mSearchButton } )
    {
      if ( !button )
        continue;
      button->setIconSize( QSize( iconSize, iconSize ) );
      button->setFixedSize( iconSize + 4, iconSize + 4 );
    }
  }

  if ( mSearchWidget && mSearchField )
  {
    // Room for the placeholder plus the leading search and trailing clear buttons.
    const QFontMetrics searchFm = mSearchField->fontMetrics();
    const int inlineButton = searchFm.height() + 2 * m.spaceSm;
    const int natural = searchFm.horizontalAdvance( mSearchField->placeholderText() ) + 2 * inlineButton + 2 * m.spaceLg;
    mSearchMax = std::clamp( natural, 260, 320 );
    mSearchMin = mSearchMax * 3 / 5;
    mSearchWidget->setMinimumWidth( mSearchMin );
  }

  // Page margins (top SM, bottom XS plus the 1px border), command area, the XS gap above
  // the caption, and the caption row.
  const int baseButtonHeight = 58;
  m.buttonAreaHeight = baseButtonHeight;
  m.captionHeight = QFontMetrics( ribbonCaptionFont( font() ) ).height();

  const int chrome = m.spaceSm + m.spaceXs + 1;
  const int command = chrome + m.buttonAreaHeight + m.spaceXs + m.captionHeight;
  mCommandHeight = command;
  m.captions = true;

  if ( m != mMetrics || command != mCommandHeight )
  {
    mMetrics = m;
    for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
      page->setMetrics( m );
  }
  setFixedHeight( tabBar()->sizeHint().height() + mCommandHeight );
  updateGeometry();
  syncChromeTabBarGeometry();
  if ( parentWidget() )
    parentWidget()->updateGeometry();
  if ( mApp && mApp->appRibbonBar() )
    mApp->appRibbonBar()->updateGeometry();
  mUpdatingMetrics = false;
}

bool QgsAppRibbon::event( QEvent *event )
{
  const bool result = QTabWidget::event( event );
  // QTabWidget re-applies the style's corner rect on every layout pass.
  if ( event->type() == QEvent::LayoutRequest )
    syncChromeTabBarGeometry();
#if QT_VERSION >= QT_VERSION_CHECK( 6, 6, 0 )
  if ( event->type() == QEvent::DevicePixelRatioChange )
    updateMetrics();
#endif
  return result;
}

void QgsAppRibbon::changeEvent( QEvent *event )
{
  QTabWidget::changeEvent( event );
  if ( event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange )
    updateMetrics();
}

bool QgsAppRibbon::eventFilter( QObject *watched, QEvent *event )
{
  if ( watched == mSearchField && event->type() == QEvent::KeyPress && mSearchOverlay && mSearchOverlay->isVisible() )
  {
    // Runs before the locator's own filter, which still hides its results on Esc.
    if ( static_cast<QKeyEvent *>( event )->key() == Qt::Key_Escape )
    {
      if ( mSearchEscArmed || mSearchField->text().isEmpty() )
        hideSearchOverlay( true );
      else
        mSearchEscArmed = true;
    }
    else
    {
      mSearchEscArmed = false;
    }
    return QTabWidget::eventFilter( watched, event );
  }

  if ( ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved ) && mMenuBar && watched == mMenuBar.data() )
  {
    // Extensions add or remove top-level menus in bursts; coalesce into one sync.
    if ( !mMenuBarSyncPending )
    {
      mMenuBarSyncPending = true;
      QTimer::singleShot( 0, this, [this] {
        mMenuBarSyncPending = false;
        syncMenuBar();
      } );
    }
  }
  else if ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved )
  {
    auto *toolbar = qobject_cast<QToolBar *>( watched );
    if ( toolbar && mMirroredToolbars.contains( toolbar ) )
    {
      // Plugins often add several actions in a row; coalesce into one rebuild.
      if ( mPendingMirrorSyncs.isEmpty() )
      {
        QTimer::singleShot( 0, this, [this] {
          const QList<QPointer<QToolBar>> pending = std::exchange( mPendingMirrorSyncs, {} );
          for ( const QPointer<QToolBar> &pendingToolbar : pending )
          {
            if ( pendingToolbar )
              syncMirroredGroup( pendingToolbar );
          }
        } );
      }
      if ( !mPendingMirrorSyncs.contains( toolbar ) )
        mPendingMirrorSyncs << toolbar;
    }
    else if ( qobject_cast<QMenu *>( watched ) )
    {
      for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
      {
        if ( page )
        {
          for ( QgsAppRibbonGroup *grp : page->groups() )
          {
            if ( grp )
              grp->scheduleRebuild();
          }
        }
      }
    }
  }
  return QTabWidget::eventFilter( watched, event );
}

void QgsAppRibbon::resizeEvent( QResizeEvent *event )
{
  QTabWidget::resizeEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::showEvent( QShowEvent *event )
{
  QTabWidget::showEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::syncChromeTabBarGeometry()
{
  QTabBar *bar = tabBar();
  if ( !bar )
    return;

  // Portable QWidget geometry only (valid on Wayland/X11/Windows/macOS). Document-mode
  // tab bars often keep sizeHint width (= tabs only); force the bar to span up to the
  // brand corner widget so the strip is filled past the last tab without stretching
  // tab labels, and the scroll arrows never sit underneath the brand.
  const int stripWidth = width();
  if ( stripWidth <= 0 )
    return;

  QWidget *filler = cornerWidget( Qt::TopRightCorner );
  syncSearchGeometry( stripWidth, std::max( bar->height(), bar->sizeHint().height() ) );
  const int barWidth = std::max( 1, stripWidth - ( filler ? filler->sizeHint().width() : 0 ) );

  if ( bar->minimumWidth() != barWidth )
    bar->setMinimumWidth( barWidth );

  // Integer height from sizeHint — avoid fighting layout when already the right width.
  const int h = std::max( bar->sizeHint().height(), 1 );
  if ( bar->x() != 0 || bar->width() != barWidth )
  {
    const QRect target( 0, bar->y(), barWidth, std::max( bar->height(), h ) );
    if ( bar->geometry() != target )
      bar->setGeometry( target );
  }

  if ( filler )
  {
    // The style's corner rect is shorter than the tab bar, so a fixed-height filler placed
    // there grows down into the command area; pin it to the tab bar's own row instead.
    const QRect barRect = bar->geometry();
    const int fillerH = barRect.height() > 0 ? barRect.height() : h;
    if ( filler->minimumHeight() != fillerH )
      filler->setMinimumHeight( fillerH );
    if ( filler->maximumHeight() != fillerH )
      filler->setMaximumHeight( fillerH );
    const QRect target( barRect.right() + 1, barRect.top(), std::max( 0, stripWidth - barRect.width() ), fillerH );
    if ( filler->geometry() != target )
      filler->setGeometry( target );
  }
}

void QgsAppRibbon::refreshOptionalActions()
{
  if ( !mApp )
    return;

  for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
  {
    for ( QgsAppRibbonGroup *group : page->groups() )
    {
      group->resolveDeferred( mApp );
      group->scheduleRebuild();
    }
  }
}

void QgsAppRibbon::refreshIcons()
{
  for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
  {
    for ( QgsAppRibbonGroup *group : page->groups() )
      group->scheduleRebuild();
  }
}

QList<QgsAppRibbon::RibbonCommandInfo> QgsAppRibbon::ribbonCommands() const
{
  QList<RibbonCommandInfo> list;
  QSet<QAction *> seen;

  std::function<void( QMenu *, int, const QString &, const QString & )> extractMenuActions;
  extractMenuActions = [&]( QMenu *menu, int pageIdx, const QString &pageTitle, const QString &parentHierarchy ) {
    if ( !menu )
      return;
    const QList<QAction *> actions = menu->actions();
    for ( QAction *act : actions )
    {
      if ( !act || act->isSeparator() )
        continue;
      if ( QMenu *sub = act->menu() )
      {
        QString cleanSubTitle = sub->title();
        cleanSubTitle.remove( '&' );
        const QString subHierarchy = parentHierarchy + u" \u25B8 "_s + cleanSubTitle.trimmed();
        extractMenuActions( sub, pageIdx, pageTitle, subHierarchy );
      }
      else
      {
        if ( !seen.contains( act ) )
        {
          seen.insert( act );
          RibbonCommandInfo info;
          info.action = act;
          info.pageIndex = pageIdx;
          info.pageTitle = pageTitle;
          info.ribbonLabel = cleanActionLabel( act, QString() );
          info.fullHierarchy = parentHierarchy;
          list.append( info );
        }
      }
    }
  };

  for ( int pageIdx = 0; pageIdx < mPages.size(); ++pageIdx )
  {
    QgsAppRibbonPage *page = mPages[pageIdx];
    if ( !page )
      continue;
    const QString pageTitle = tabText( pageIdx );

    for ( QgsAppRibbonGroup *group : page->groups() )
    {
      if ( !group )
        continue;
      const QString groupTitle = group->title();
      const QString baseHierarchy = pageTitle + u" \u25B8 "_s + groupTitle;

      for ( const QgsAppRibbonGroup::Entry &entry : group->entries() )
      {
        QAction *act = entry.action.data();
        if ( !act )
          continue;

        if ( QMenu *menu = dropDownMenu( act ) )
        {
          extractMenuActions( menu, pageIdx, pageTitle, baseHierarchy );
        }
        else if ( act->menu() )
        {
          extractMenuActions( act->menu(), pageIdx, pageTitle, baseHierarchy );
        }

        if ( !seen.contains( act ) )
        {
          seen.insert( act );
          RibbonCommandInfo info;
          info.action = act;
          info.pageIndex = pageIdx;
          info.pageTitle = pageTitle;
          info.groupTitle = groupTitle;
          info.ribbonLabel = !entry.label.isEmpty() ? entry.label : cleanActionLabel( act, QString() );
          info.fullHierarchy = baseHierarchy;
          list.append( info );
        }
      }
    }
  }

  return list;
}

void QgsAppRibbon::activateRibbonCommand( QAction *action )
{
  if ( !action )
    return;

  // 1. Switch Ribbon tab to the page containing this command
  const QList<RibbonCommandInfo> cmds = ribbonCommands();
  for ( const RibbonCommandInfo &cmd : cmds )
  {
    if ( cmd.action == action && cmd.pageIndex >= 0 && cmd.pageIndex < count() )
    {
      setCurrentIndex( cmd.pageIndex );
      break;
    }
  }

  // 2. If it's a map navigation / interaction tool, ensure map canvas is visible
  if ( mApp )
  {
    const QString objName = action->objectName();
    const QString txt = action->text();
    if ( mApp->isWelcomeScreenVisible() )
    {
      if ( objName.contains( "Pan", Qt::CaseInsensitive )
           || objName.contains( "Zoom", Qt::CaseInsensitive )
           || objName.contains( "Select", Qt::CaseInsensitive )
           || objName.contains( "Identify", Qt::CaseInsensitive )
           || objName.contains( "Measure", Qt::CaseInsensitive )
           || objName.contains( "Feature", Qt::CaseInsensitive )
           || objName.contains( "Draw", Qt::CaseInsensitive )
           || txt.contains( "Pan", Qt::CaseInsensitive )
           || txt.contains( "Zoom", Qt::CaseInsensitive ) )
      {
        mApp->showMapCanvas();
      }
    }
  }

  // 3. Trigger the action
  action->trigger();
}

void QgsAppRibbon::setThemeToggleAction( QAction *action )
{
  if ( !mBrand || !action )
    return;

  if ( !mThemeToggle )
  {
    QWidget *tabFiller = mBrand->parentWidget();
    mThemeToggle = new QToolButton( tabFiller );
    mThemeToggle->setObjectName( u"HakeAppRibbonThemeToggle"_s );
    mThemeToggle->setAutoRaise( true );
    mThemeToggle->setToolButtonStyle( Qt::ToolButtonIconOnly );
    mThemeToggle->setFocusPolicy( Qt::TabFocus );
    if ( QHBoxLayout *fillerLayout = qobject_cast<QHBoxLayout *>( tabFiller->layout() ) )
      fillerLayout->insertWidget( fillerLayout->indexOf( mBrand ), mThemeToggle, 0, Qt::AlignVCenter );
  }
  // Icon and button size are set from the brand font in updateMetrics().
  mThemeToggle->setDefaultAction( action );
}

void QgsAppRibbon::setSearchWidget( QgsLocatorWidget *locator )
{
  if ( !mBrand || !locator || mSearchWidget )
    return;

  QWidget *tabFiller = mBrand->parentWidget();
  auto *fillerLayout = qobject_cast<QHBoxLayout *>( tabFiller->layout() );
  if ( !fillerLayout )
    return;

  const QString accessibleName = tr( "Tool Search" );
  const QString toolTip = tr( "Search tools, layers, and commands" );

  mSearchWidget = locator;
  mSearchField = locator->findChild<QLineEdit *>();
  locator->setPlaceholderText( tr( "Search tools, layers, commands..." ) );
  // Results open beneath the field, right edges aligned, so they stay clear of the window edge.
  locator->setResultContainerAnchors( QgsFloatingWidget::TopRight, QgsFloatingWidget::BottomRight );
  if ( mSearchField )
  {
    mSearchField->setToolTip( toolTip );
    mSearchField->setAccessibleName( accessibleName );
    mSearchField->installEventFilter( this );
  }

  mSearchHost = new QWidget( tabFiller );
  mSearchHost->setObjectName( u"HakeAppRibbonSearch"_s );
  mSearchHost->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
  auto *hostLayout = new QHBoxLayout( mSearchHost );
  hostLayout->setContentsMargins( 0, 0, 0, 0 );
  hostLayout->setSpacing( 0 );

  mSearchButton = new QToolButton( mSearchHost );
  mSearchButton->setObjectName( u"HakeAppRibbonSearchButton"_s );
  mSearchButton->setAutoRaise( true );
  mSearchButton->setToolButtonStyle( Qt::ToolButtonIconOnly );
  mSearchButton->setFocusPolicy( Qt::TabFocus );
  mSearchButton->setToolTip( toolTip );
  mSearchButton->setAccessibleName( accessibleName );
  mSearchButton->setIcon( QgsApplication::getThemeIcon( u"/search.svg"_s ) );
  mSearchButton->hide();
  connect( mSearchButton, &QToolButton::clicked, this, [this] {
    revealSearch();
    if ( mSearchWidget )
      mSearchWidget->search( QString() );
  } );

  hostLayout->addWidget( mSearchButton, 0, Qt::AlignVCenter );
  hostLayout->addWidget( locator, 1, Qt::AlignVCenter );
  fillerLayout->insertWidget( fillerLayout->indexOf( mThemeToggle ? static_cast<QWidget *>( mThemeToggle ) : mBrand ), mSearchHost, 0, Qt::AlignVCenter );

  // Narrow strips: the same locator is shown below the search button, over the workspace.
  mSearchOverlay = new QgsFloatingWidget( window() );
  mSearchOverlay->setObjectName( u"HakeAppRibbonSearchOverlay"_s );
  mSearchOverlay->setAttribute( Qt::WA_StyledBackground, true );
  mSearchOverlay->setAutoFillBackground( true );
  mSearchOverlay->setAnchorWidget( mSearchButton );
  mSearchOverlay->setAnchorPoint( QgsFloatingWidget::TopRight );
  mSearchOverlay->setAnchorWidgetPoint( QgsFloatingWidget::BottomRight );
  new QHBoxLayout( mSearchOverlay );
  mSearchOverlay->hide();

  connect( qApp, &QApplication::focusChanged, this, [this] {
    if ( !mSearchOverlay || !mSearchOverlay->isVisible() )
      return;
    // Deferred so focus moving between the field and the locator results settles first.
    QTimer::singleShot( 0, this, [this] {
      if ( !mSearchOverlay || !mSearchOverlay->isVisible() )
        return;
      QWidget *focus = QApplication::focusWidget();
      // Menus, dialogs and other windows opened from the search keep it open.
      if ( focus && focus->window() != window() )
        return;
      for ( QWidget *w = focus; w; w = w->parentWidget() )
      {
        if ( w == mSearchOverlay )
          return;
        if ( auto *floating = qobject_cast<QgsFloatingWidget *>( w ) )
        {
          if ( mSearchField && floating->anchorWidget() == mSearchField )
            return;
        }
      }
      hideSearchOverlay( false );
    } );
  } );

  updateMetrics();
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::revealSearch()
{
  if ( !mSearchOverlay || !mSearchCollapsed || !mSearchVisible )
    return;

  if ( !mSearchOverlay->isVisible() )
  {
    QWidget *focus = QApplication::focusWidget();
    mFocusBeforeSearch = focus && !mSearchOverlay->isAncestorOf( focus ) ? focus : nullptr;
  }
  mSearchEscArmed = false;

  const int margin = mMetrics.spaceSm;
  if ( QLayout *overlayLayout = mSearchOverlay->layout() )
    overlayLayout->setContentsMargins( margin, margin, margin, margin );
  const int available = window()->width() - 2 * margin;
  mSearchOverlay->setFixedWidth( std::max( mSearchMin, std::min( mSearchMax, available ) ) );
  mSearchOverlay->adjustSize();
  mSearchOverlay->show();
  mSearchOverlay->raise();
}

void QgsAppRibbon::setSearchVisible( bool visible )
{
  mSearchVisible = visible;
  if ( !mSearchHost )
    return;

  if ( !visible )
    hideSearchOverlay( false );
  mSearchHost->setVisible( visible );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::syncSearchGeometry( int stripWidth, int rowHeight )
{
  // Search sizes come from updateMetrics(), which needs the ribbon pages.
  if ( !mSearchHost || !mSearchWidget || !mSearchVisible || !mBrand || mSearchMax <= 0 )
    return;

  const auto layoutWidth = []( const QWidget *w ) { return std::clamp( w->sizeHint().width(), w->minimumWidth(), w->maximumWidth() ); };

  QLayout *fillerLayout = mBrand->parentWidget()->layout();
  const QMargins margins = fillerLayout->contentsMargins();
  const int spacing = std::max( 0, fillerLayout->spacing() );
  int corner = margins.left() + margins.right() + layoutWidth( mBrand ) + spacing;
  if ( mThemeToggle && !mThemeToggle->isHidden() )
    corner += layoutWidth( mThemeToggle ) + spacing;

  // The tabs keep their natural width; the search takes what is left, or yields to a button.
  const int spare = stripWidth - tabBar()->sizeHint().width() - corner;
  const bool collapse = spare < mSearchMin;
  setSearchCollapsed( collapse );

  const int hostWidth = collapse ? layoutWidth( mSearchButton ) : std::min( spare, mSearchMax );
  if ( mSearchHost->minimumWidth() != hostWidth || mSearchHost->maximumWidth() != hostWidth )
    mSearchHost->setFixedWidth( hostWidth );

  const int fieldHeight = std::max( 1, rowHeight - 2 * mMetrics.spaceXs );
  if ( mSearchWidget->maximumHeight() != fieldHeight )
    mSearchWidget->setMaximumHeight( fieldHeight );
}

void QgsAppRibbon::setSearchCollapsed( bool collapsed )
{
  if ( !mSearchWidget || collapsed == mSearchCollapsed )
    return;

  mSearchCollapsed = collapsed;
  hideSearchOverlay( false );
  if ( collapsed )
  {
    mSearchOverlay->layout()->addWidget( mSearchWidget );
    mSearchButton->show();
  }
  else
  {
    static_cast<QHBoxLayout *>( mSearchHost->layout() )->addWidget( mSearchWidget, 1, Qt::AlignVCenter );
    mSearchButton->hide();
  }
  mSearchWidget->show();
}

void QgsAppRibbon::hideSearchOverlay( bool restoreFocus )
{
  if ( !mSearchOverlay || !mSearchOverlay->isVisible() )
    return;

  mSearchOverlay->hide();
  if ( restoreFocus && mFocusBeforeSearch )
    mFocusBeforeSearch->setFocus( Qt::OtherFocusReason );
  mFocusBeforeSearch = nullptr;
}

QgsAppRibbonPage *QgsAppRibbon::addPage( const QString &title )
{
  auto *page = new QgsAppRibbonPage( this );
  addTab( page, title );
  mPages << page;
  return page;
}

QgsAppRibbonGroup *QgsAppRibbon::addGroup( QgsAppRibbonPage *page, const QString &title )
{
  return page->addGroup( title );
}

void QgsAppRibbon::addNamedAction( QgsAppRibbonGroup *group, const QString &objectName, bool primary )
{
  if ( !mApp || !group )
    return;
  QAction *action = mApp->findChild<QAction *>( objectName );
  if ( !action && objectName == "mActionShowPythonDialog"_L1 )
    action = mApp->actionShowPythonDialog();
  if ( !action && objectName == "mActionAllEdits"_L1 )
    action = mApp->actionAllEdits();
  if ( !action && objectName == "mActionShowPythonDialog"_L1 )
  {
    action = new QAction( QgsHakeIcons::icon( u"analysis/hake-analysis-python-console.svg"_s ), tr( "Python" ), this );
    action->setObjectName( objectName );
    connect( action, &QAction::triggered, this, [this]() {
      if ( mApp )
      {
        mApp->showNativePythonConsole();
      }
    } );
  }
  if ( !action && objectName == "mActionShowMetaSearch"_L1 )
  {
    action = new QAction( QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s ), tr( "Web" ), this );
    action->setObjectName( objectName );
    action->setToolTip( tr( "MetaSearch (Metadata Catalog Search)" ) );
    connect( action, &QAction::triggered, this, [this]() {
      if ( mApp )
      {
        mApp->showMetaSearch();
      }
    } );
  }
  if ( !action && objectName == "mActionShowDbManager"_L1 )
  {
    action = new QAction( QgsHakeIcons::icon( u"database/hake-database-db-manager.svg"_s ), tr( "Database" ), this );
    action->setObjectName( objectName );
    action->setToolTip( tr( "Database Manager" ) );
    connect( action, &QAction::triggered, this, [this]() {
      if ( mApp )
      {
        for ( QAction *act : mApp->findChildren<QAction *>() )
        {
          if ( act->objectName() == "mActionDbManager"_L1 || act->text().contains( "DB Manager", Qt::CaseInsensitive ) )
          {
            act->trigger();
            return;
          }
        }
        if ( QAction *qh = mApp->findChild<QAction *>( u"mActionQueryHistory"_s ) )
        {
          qh->trigger();
        }
        else if ( mApp->visibleMessageBar() )
        {
          mApp->visibleMessageBar()->pushMessage( tr( "Database" ), tr( "Database Manager is loading or unavailable." ), Qgis::MessageLevel::Info );
        }
      }
    } );
  }
  if ( action )
  {
    if ( objectName == "mActionShowPythonDialog"_L1 )
    {
      action->setIcon( QgsHakeIcons::icon( u"analysis/hake-analysis-python-console.svg"_s ) );
      group->addEntry( { action, primary, QString(), QString(), objectName, tr( "Python" ) } );
    }
    else if ( objectName == "mActionShowMetaSearch"_L1 )
    {
      action->setIcon( QgsHakeIcons::icon( u"menus/hake-menu-web.svg"_s ) );
      group->addEntry( { action, primary, QString(), QString(), objectName, tr( "Web" ) } );
    }
    else if ( objectName == "mActionShowDbManager"_L1 )
    {
      action->setIcon( QgsHakeIcons::icon( u"database/hake-database-db-manager.svg"_s ) );
      group->addEntry( { action, primary, QString(), QString(), objectName, tr( "Database" ) } );
    }
    else
    {
      group->addEntry( { action, primary, QString(), QString(), objectName, QString() } );
    }
  }
  else
  {
    group->addEntry( { nullptr, primary, QString(), QString(), objectName, QString() } );
  }
}

void QgsAppRibbon::addDockToggle( QgsAppRibbonGroup *group, const QString &dockObjectName, const QString &label, bool primary )
{
  if ( !group )
    return;
  QAction *action = nullptr;
  if ( mApp )
  {
    if ( QDockWidget *dock = mApp->findChild<QDockWidget *>( dockObjectName ) )
    {
      action = dock->toggleViewAction();
      if ( dockObjectName == "ProcessingToolbox"_L1 )
        action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
    }
  }
  if ( !action && dockObjectName == "ProcessingToolbox"_L1 )
  {
    action = new QAction( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ), label.isEmpty() ? tr( "Processing Toolbox" ) : label, this );
    action->setObjectName( u"mRibbonActionProcessingToolbox"_s );
    action->setCheckable( true );
    connect( action, &QAction::triggered, this, [this, action]() {
      if ( mApp )
      {
        if ( QDockWidget *dock = mApp->findChild<QDockWidget *>( u"ProcessingToolbox"_s ) )
        {
          const bool vis = !dock->isVisible();
          dock->setVisible( vis );
          if ( vis )
            dock->raise();
          action->setChecked( vis );
          return;
        }
        if ( QAction *act = mApp->findChild<QAction *>( u"toolboxAction"_s ) )
        {
          act->trigger();
          return;
        }
        if ( mApp->visibleMessageBar() )
        {
          mApp->visibleMessageBar()->pushMessage( tr( "Processing" ), tr( "Processing Toolbox dock is loading or unavailable." ), Qgis::MessageLevel::Info );
        }
      }
      action->setChecked( false );
    } );
  }
  group->addEntry( { action, primary, dockObjectName, QString(), QString(), label } );
}

void QgsAppRibbon::addMenu( QgsAppRibbonGroup *group, QMenu *menu, const QString &overrideLabel )
{
  if ( group && menu )
  {
    group->addEntry( { menu->menuAction(), false, QString(), QString(), QString(), overrideLabel } );
    menu->installEventFilter( this );
  }
}

void QgsAppRibbon::addDeferredMenu( QgsAppRibbonGroup *group, const QString &menuObjectName )
{
  if ( !group )
    return;
  QAction *action = nullptr;
  if ( mApp )
  {
    if ( QMenu *menu = mApp->findChild<QMenu *>( menuObjectName ) )
    {
      action = menu->menuAction();
      if ( menuObjectName == "processing"_L1 )
        action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
    }
  }
  if ( !action && menuObjectName == "processing"_L1 )
  {
    QMenu *menu = new QMenu( tr( "Processing" ), this );
    menu->setObjectName( menuObjectName );

    QAction *actToolbox = menu->addAction( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ), tr( "Toolbox" ) );
    connect( actToolbox, &QAction::triggered, this, [this]() {
      if ( mApp )
      {
        if ( QDockWidget *dock = mApp->findChild<QDockWidget *>( u"ProcessingToolbox"_s ) )
        {
          dock->setVisible( true );
          dock->raise();
        }
        else if ( QAction *act = mApp->findChild<QAction *>( u"toolboxAction"_s ) )
        {
          act->trigger();
        }
        else if ( mApp->visibleMessageBar() )
        {
          mApp->visibleMessageBar()->pushMessage( tr( "Processing" ), tr( "Processing Toolbox is loading or unavailable." ), Qgis::MessageLevel::Info );
        }
      }
    } );

    QAction *actModeler = menu->addAction( QgsHakeIcons::icon( u"processing/hake-processing-model-designer.svg"_s ), tr( "Model Designer…" ) );
    connect( actModeler, &QAction::triggered, this, [this]() {
      if ( QgsPythonRunner::isValid() )
      {
        QgsPythonRunner::run( u"from processing.modeler.ModelerDialog import ModelerDialog; ModelerDialog.create()"_s );
        return;
      }
      QgsModelDesignerDialog *dlg = new QgsModelDesignerDialog( mApp );
      dlg->show();
    } );

    QAction *actHistory = menu->addAction( QgsHakeIcons::icon( u"processing/hake-processing-history.svg"_s ), tr( "History…" ) );
    connect( actHistory, &QAction::triggered, this, [this]() {
      QgsProcessingHistoryDialog dlg( mApp );
      dlg.exec();
    } );

    QAction *actResults = menu->addAction( QgsHakeIcons::icon( u"processing/hake-processing-results-viewer.svg"_s ), tr( "Results Viewer" ) );
    connect( actResults, &QAction::triggered, this, [this]() {
      if ( mApp && mApp->visibleMessageBar() )
        mApp->visibleMessageBar()->pushMessage( tr( "Processing" ), tr( "Results Viewer is unavailable." ), Qgis::MessageLevel::Info );
    } );

    action = menu->menuAction();
    action->setIcon( QgsHakeIcons::icon( u"processing/hake-processing-toolbox.svg"_s ) );
  }
  group->addEntry( { action, false, QString(), menuObjectName, QString(), menuObjectName == "processing"_L1 ? tr( "Processing" ) : QString() } );
}

void QgsAppRibbon::mirrorToolbar( QgsAppRibbonGroup *group, QToolBar *toolbar )
{
  if ( !group || !toolbar )
    return;
  mMirroredToolbars.insert( toolbar, group );
  toolbar->installEventFilter( this );
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    if ( QAction *presented = presentableToolbarAction( action ) )
      group->addEntry( { presented, false, QString(), QString(), QString(), QString() } );
  }
}

void QgsAppRibbon::syncMirroredGroup( QToolBar *toolbar )
{
  QgsAppRibbonGroup *group = mMirroredToolbars.value( toolbar );
  if ( !group )
    return;
  QList<QgsAppRibbonGroup::Entry> entries;
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    if ( QAction *presented = presentableToolbarAction( action ) )
      entries.append( QgsAppRibbonGroup::Entry { presented, false, QString(), QString(), QString(), QString() } );
  }
  group->setEntries( entries );
}

void QgsAppRibbon::watchMenuBar( QMenuBar *menuBar )
{
  if ( !menuBar )
    return;
  mMenuBar = menuBar;
  menuBar->installEventFilter( this );
  syncMenuBar();
}

bool QgsAppRibbon::isStandardMenu( const QMenu *menu ) const
{
  if ( !mApp || !menu )
    return false;
  // Processing is a core plugin with its own drop-down in the Analysis tab.
  if ( menu->objectName() == "processing"_L1 )
    return true;
  const QList<const QMenu *> standardMenus {
    mApp->projectMenu(),
    mApp->editMenu(),
    mApp->viewMenu(),
    mApp->layerMenu(),
    mApp->settingsMenu(),
    mApp->pluginMenu(),
    mApp->vectorMenu(),
    mApp->rasterMenu(),
    mApp->databaseMenu(),
    mApp->webMenu(),
    mApp->meshMenu(),
    mApp->windowMenu(),
    mApp->helpMenu(),
  };
  return standardMenus.contains( menu );
}

void QgsAppRibbon::syncMenuBar()
{
  if ( !mApp || !mMenuBar )
    return;

  const QList<QAction *> barActions = mMenuBar->actions();
  const QList<QAction *> appActions = mApp->actions();
  for ( const QPointer<QAction> &adopted : std::as_const( mAdoptedMenuBarActions ) )
  {
    if ( adopted && !barActions.contains( adopted.data() ) )
      mApp->removeAction( adopted );
  }
  mAdoptedMenuBarActions.clear();

  QList<QgsAppRibbonGroup::Entry> extensionEntries;
  for ( QAction *action : barActions )
  {
    // The menu bar is hidden; registering its menus on the main window keeps their shortcuts active.
    if ( !appActions.contains( action ) )
      mApp->addAction( action );
    mAdoptedMenuBarActions << action;

    QMenu *menu = action->menu();
    if ( menu && !isStandardMenu( menu ) )
      extensionEntries.append( QgsAppRibbonGroup::Entry { action, false, QString(), QString(), QString(), QString() } );
  }

  if ( mApp->pluginMenu() )
  {
    const QList<QAction *> pluginActions = mApp->pluginMenu()->actions();
    for ( QAction *action : pluginActions )
    {
      if ( !action )
        continue;
      QMenu *menu = action->menu();
      if ( menu && !isStandardMenu( menu ) )
      {
        bool alreadyIncluded = false;
        for ( const auto &entry : std::as_const( extensionEntries ) )
        {
          if ( entry.action == action || ( entry.action && entry.action->text() == action->text() ) )
          {
            alreadyIncluded = true;
            break;
          }
        }
        if ( !alreadyIncluded )
        {
          extensionEntries.append( QgsAppRibbonGroup::Entry { action, false, QString(), QString(), QString(), QString() } );
        }
      }
    }
  }

  if ( mExtensionMenus )
    mExtensionMenus->setEntries( extensionEntries );
}

void QgsAppRibbon::setUserHeightOffset( int offset )
{
  Q_UNUSED( offset )
  mUserHeightOffset = 0;
}

void QgsAppRibbon::loadUserHeightOffset()
{
  QgsSettings settings;
  settings.remove( u"/Hake/Ribbon/ExtraHeight"_s );
  mUserHeightOffset = 0;
}

void QgsAppRibbon::saveUserHeightOffset() const
{
  QgsSettings settings;
  settings.remove( u"/Hake/Ribbon/ExtraHeight"_s );
}

//
// QgsAppRibbonResizeGrip
//

QgsAppRibbonResizeGrip::QgsAppRibbonResizeGrip( QgsAppRibbon *ribbon, QWidget *parent )
  : QWidget( parent )
  , mRibbon( ribbon )
{
  setObjectName( u"HakeAppRibbonResizeGrip"_s );
  setFixedHeight( 6 );
  setCursor( Qt::SplitVCursor );
  setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  setMouseTracking( true );
  setToolTip( tr( "Drag to resize upper navbar height; double-click to reset" ) );
}

void QgsAppRibbonResizeGrip::paintEvent( QPaintEvent * )
{
  QPainter p( this );
  const bool isNight = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night );

  p.setPen( isNight ? QColor( 30, 41, 59 ) : QColor( 210, 220, 230 ) );
  p.drawLine( 0, 0, width(), 0 );

  if ( mHovered || mDragging )
  {
    p.setPen( isNight ? QColor( 56, 189, 248, 180 ) : QColor( 22, 74, 115, 180 ) );
    p.drawLine( 0, 2, width(), 2 );

    const int cx = width() / 2;
    p.fillRect( cx - 12, 1, 6, 2, isNight ? QColor( 56, 189, 248 ) : QColor( 22, 74, 115 ) );
    p.fillRect( cx - 2, 1, 6, 2, isNight ? QColor( 56, 189, 248 ) : QColor( 22, 74, 115 ) );
    p.fillRect( cx + 8, 1, 6, 2, isNight ? QColor( 56, 189, 248 ) : QColor( 22, 74, 115 ) );
  }
}

void QgsAppRibbonResizeGrip::enterEvent( QEnterEvent * )
{
  mHovered = true;
  update();
}

void QgsAppRibbonResizeGrip::leaveEvent( QEvent * )
{
  mHovered = false;
  update();
}

void QgsAppRibbonResizeGrip::mousePressEvent( QMouseEvent *event )
{
  if ( event->button() == Qt::LeftButton )
  {
    mDragging = true;
    mDragStartY = event->globalPosition().toPoint().y();
    mStartHeightOffset = mRibbon ? mRibbon->userHeightOffset() : 0;
    event->accept();
    update();
  }
}

void QgsAppRibbonResizeGrip::mouseMoveEvent( QMouseEvent *event )
{
  if ( mDragging && mRibbon )
  {
    const int deltaY = event->globalPosition().toPoint().y() - mDragStartY;
    mRibbon->setUserHeightOffset( mStartHeightOffset + deltaY );
    event->accept();
  }
}

void QgsAppRibbonResizeGrip::mouseReleaseEvent( QMouseEvent *event )
{
  if ( mDragging )
  {
    mDragging = false;
    if ( mRibbon )
      mRibbon->saveUserHeightOffset();
    event->accept();
    update();
  }
}

void QgsAppRibbonResizeGrip::mouseDoubleClickEvent( QMouseEvent *event )
{
  if ( event->button() == Qt::LeftButton && mRibbon )
  {
    mRibbon->setUserHeightOffset( 0 );
    mRibbon->saveUserHeightOffset();
    event->accept();
    update();
  }
}
