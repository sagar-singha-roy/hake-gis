/***************************************************************************
 *  qgsprocessingtoolboxdockwidget.h
 *  -------------------
 *  Native Processing Toolbox dock widget for Hake GeoDesk
 ***************************************************************************/

#ifndef QGSPROCESSINGTOOLBOXDOCKWIDGET_H
#define QGSPROCESSINGTOOLBOXDOCKWIDGET_H

#include "qgsdockwidget.h"
#include <QPointer>

class QgsProcessingToolboxTreeView;
class QgsFilterLineEdit;
class QgsProcessingAlgorithm;
class QToolBar;
class QAction;

class QgsProcessingToolboxDockWidget : public QgsDockWidget
{
    Q_OBJECT

  public:
    explicit QgsProcessingToolboxDockWidget( QWidget *parent = nullptr );
    ~QgsProcessingToolboxDockWidget() override;

  public slots:
    void openAlgorithm( const QgsProcessingAlgorithm *algorithm );
    void openModelDesigner();
    void openHistory();

  private slots:
    void onTreeDoubleClicked( const QModelIndex &index );
    void onCustomContextMenu( const QPoint &point );

  private:
    void setupUi();

    QToolBar *mToolBar = nullptr;
    QgsFilterLineEdit *mSearchBox = nullptr;
    QgsProcessingToolboxTreeView *mTree = nullptr;
    QAction *mActionModeler = nullptr;
    QAction *mActionHistory = nullptr;
    QAction *mActionExpandAll = nullptr;
    QAction *mActionCollapseAll = nullptr;
};

#endif // QGSPROCESSINGTOOLBOXDOCKWIDGET_H

