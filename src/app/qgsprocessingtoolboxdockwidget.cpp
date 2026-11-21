/***************************************************************************
 *  qgsprocessingtoolboxdockwidget.cpp
 *  -------------------
 *  Native Processing Toolbox dock widget for Hake GeoDesk
 ***************************************************************************/

#include "qgsprocessingtoolboxdockwidget.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsfilterlineedit.h"
#include "qgsgui.h"
#include "qgshakeicons.h"
#include "qgsmessagebar.h"
#include "qgspanelwidget.h"
#include "qgsprocessingalgorithm.h"
#include "qgsprocessingalgrunnertask.h"
#include "qgsprocessingalgorithmwidgetbase.h"
#include "qgsprocessingfavoritealgorithmmanager.h"
#include "qgsprocessingguiregistry.h"
#include "qgsprocessinghistorywidget.h"
#include "processing/models/qgsmodeldesignerdialog.h"
#include "qgspythonrunner.h"
#include "qgsprocessingregistry.h"
#include "qgsprocessingtoolboxtreeview.h"
#include "qgsprocessingwidgetwrapper.h"
#include "qgsproject.h"

#include <QAction>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QToolBar>

#include "moc_qgsprocessingtoolboxdockwidget.cpp"

using namespace Qt::StringLiterals;

//
// QgsNativeProcessingParametersPanel
// Custom parameter panel generating widgets for native C++ algorithms
//
class QgsNativeProcessingParametersPanel : public QgsPanelWidget, public QgsProcessingParametersGenerator
{
    Q_OBJECT
  public:
    QgsNativeProcessingParametersPanel( const QgsProcessingAlgorithm *algorithm, QWidget *parent = nullptr )
      : QgsPanelWidget( parent )
      , mAlgorithm( algorithm )
    {
      QVBoxLayout *panelLayout = new QVBoxLayout( this );
      panelLayout->setContentsMargins( 0, 0, 0, 0 );

      QScrollArea *scrollArea = new QScrollArea( this );
      scrollArea->setWidgetResizable( true );
      scrollArea->setFrameShape( QFrame::NoFrame );

      QWidget *scrollContents = new QWidget( scrollArea );
      QVBoxLayout *scrollLayout = new QVBoxLayout( scrollContents );
      scrollLayout->setContentsMargins( 12, 12, 12, 12 );
      scrollLayout->setSpacing( 10 );

      mContext.setProject( QgsProject::instance() );

      QgsProcessingParameterWidgetContext widgetContext;
      widgetContext.setProject( QgsProject::instance() );
      if ( QgisApp::instance() )
      {
        widgetContext.setMapCanvas( QgisApp::instance()->mapCanvas() );
        widgetContext.setMessageBar( QgisApp::instance()->visibleMessageBar() );
      }

      if ( mAlgorithm )
      {
        const QgsProcessingParameterDefinitions defs = mAlgorithm->parameterDefinitions();
        for ( const QgsProcessingParameterDefinition *param : defs )
        {
          if ( param->flags() & Qgis::ProcessingParameterFlag::Hidden )
            continue;

          QgsAbstractProcessingParameterWidgetWrapper *wrapper =
            QgsGui::processingGuiRegistry()->createParameterWidgetWrapper( param, Qgis::ProcessingMode::Standard );
          if ( wrapper )
          {
            wrapper->setWidgetContext( widgetContext );
            QWidget *label = wrapper->createWrappedLabel();
            QWidget *widget = wrapper->createWrappedWidget( mContext );
            if ( label )
              scrollLayout->addWidget( label );
            if ( widget )
              scrollLayout->addWidget( widget );
            mWrappers.append( wrapper );
          }
        }
      }

      scrollLayout->addStretch( 1 );
      scrollContents->setLayout( scrollLayout );
      scrollArea->setWidget( scrollContents );
      panelLayout->addWidget( scrollArea );
      setLayout( panelLayout );
    }

    ~QgsNativeProcessingParametersPanel() override
    {
      qDeleteAll( mWrappers );
      mWrappers.clear();
    }

    QVariantMap createProcessingParameters( QgsProcessingParametersGenerator::Flags flags = QgsProcessingParametersGenerator::Flags() ) override
    {
      Q_UNUSED( flags );
      QVariantMap params;
      for ( QgsAbstractProcessingParameterWidgetWrapper *wrapper : std::as_const( mWrappers ) )
      {
        if ( wrapper && wrapper->parameterDefinition() )
        {
          params.insert( wrapper->parameterDefinition()->name(), wrapper->parameterValue() );
        }
      }
      return params;
    }

    QgsProcessingContext &context() { return mContext; }

  private:
    const QgsProcessingAlgorithm *mAlgorithm = nullptr;
    QgsProcessingContext mContext;
    QList<QgsAbstractProcessingParameterWidgetWrapper *> mWrappers;
};

//
// QgsNativeProcessingAlgorithmDialog
// Dialog executing native C++ processing algorithms
//
class QgsNativeProcessingAlgorithmDialog : public QgsProcessingAlgorithmWidgetBase
{
    Q_OBJECT
  public:
    QgsNativeProcessingAlgorithmDialog( const QgsProcessingAlgorithm *alg, QMainWindow *parent = nullptr )
      : QgsProcessingAlgorithmWidgetBase( parent )
      , mSourceAlgorithm( alg )
    {
      setAttribute( Qt::WA_DeleteOnClose );
      if ( alg )
      {
        setAlgorithm( alg->create() );
        mPanel = new QgsNativeProcessingParametersPanel( alg, this );
        setMainWidget( mPanel );
      }
    }

    QgsProcessingContext *processingContext() override
    {
      if ( mPanel )
        return &mPanel->context();
      if ( !mContext )
      {
        mContext = std::make_unique<QgsProcessingContext>();
        mContext->setProject( QgsProject::instance() );
      }
      return mContext.get();
    }

    QVariantMap createProcessingParameters( QgsProcessingParametersGenerator::Flags flags = QgsProcessingParametersGenerator::Flags() ) override
    {
      if ( mPanel )
        return mPanel->createProcessingParameters( flags );
      return QVariantMap();
    }

  protected:
    void runAlgorithm() override
    {
      if ( !algorithm() )
        return;

      QgsProcessingContext *context = processingContext();
      if ( !context )
      {
        mContext = std::make_unique<QgsProcessingContext>();
        mContext->setProject( QgsProject::instance() );
        context = mContext.get();
      }
      applyContextOverrides( context );
      emit algorithmAboutToRun( context );

      QVariantMap params = mPanel ? mPanel->createProcessingParameters() : createProcessingParameters();

      QString msg;
      if ( !algorithm()->checkParameterValues( params, *context, &msg ) )
      {
        QMessageBox::warning( this, tr( "Unable to execute algorithm" ), msg.isEmpty() ? tr( "One or more parameters have invalid values." ) : msg );
        return;
      }

      blockControlsWhileRunning();
      setExecutedAnyResult( true );
      clearProgress();
      showLog();

      QgsProcessingFeedback *feedback = createFeedback();
      feedback->pushInfo( tr( "Algorithm '%1' starting…" ).arg( algorithm()->displayName() ) );

      QgsProcessingAlgRunnerTask *task = new QgsProcessingAlgRunnerTask( algorithm(), params, *context, feedback );
      connect( task, &QgsProcessingAlgRunnerTask::executed, this, [this]( bool success, const QVariantMap &results ) {
        if ( success )
        {
          for ( auto it = results.constBegin(); it != results.constEnd(); ++it )
          {
            const QString val = it.value().toString();
            if ( !val.isEmpty() && QFile::exists( val ) && QgisApp::instance() )
            {
              QgisApp::instance()->openFile( val );
            }
          }
          if ( QgisApp::instance() && QgisApp::instance()->visibleMessageBar() )
          {
            QgisApp::instance()->visibleMessageBar()->pushSuccess( tr( "Processing" ), tr( "Algorithm '%1' finished successfully." ).arg( algorithm()->displayName() ) );
          }
        }
      } );

      setCurrentTask( task );
    }

  private:
    const QgsProcessingAlgorithm *mSourceAlgorithm = nullptr;
    QgsNativeProcessingParametersPanel *mPanel = nullptr;
    std::unique_ptr<QgsProcessingContext> mContext;
};

//
// QgsProcessingToolboxDockWidget
//
QgsProcessingToolboxDockWidget::QgsProcessingToolboxDockWidget( QWidget *parent )
  : QgsDockWidget( tr( "Processing Toolbox" ), parent )
{
  setObjectName( u"ProcessingToolbox"_s );
  setupUi();
}

QgsProcessingToolboxDockWidget::~QgsProcessingToolboxDockWidget() = default;

void QgsProcessingToolboxDockWidget::setupUi()
{
  QWidget *container = new QWidget( this );
  QVBoxLayout *layout = new QVBoxLayout( container );
  layout->setContentsMargins( 4, 4, 4, 4 );
  layout->setSpacing( 4 );

  // Toolbar
  mToolBar = new QToolBar( container );
  mToolBar->setIconSize( QSize( 22, 22 ) );

  mActionModeler = mToolBar->addAction( QgsApplication::getThemeIcon( u"processingModel.svg"_s ), tr( "Model Designer…" ), this, &QgsProcessingToolboxDockWidget::openModelDesigner );
  mActionModeler->setObjectName( u"mActionProcessingModeler"_s );

  mActionHistory = mToolBar->addAction( QgsApplication::getThemeIcon( u"mIconHistory.svg"_s ), tr( "History…" ), this, &QgsProcessingToolboxDockWidget::openHistory );
  mActionHistory->setObjectName( u"mActionProcessingHistory"_s );

  mToolBar->addSeparator();

  mActionExpandAll = mToolBar->addAction( QgsHakeIcons::icon( u"layers/hake-layers-expand-all.svg"_s ), tr( "Expand All" ) );
  mActionCollapseAll = mToolBar->addAction( QgsHakeIcons::icon( u"layers/hake-layers-collapse-all.svg"_s ), tr( "Collapse All" ) );

  layout->addWidget( mToolBar );

  // Search box
  mSearchBox = new QgsFilterLineEdit( container );
  mSearchBox->setObjectName( u"mProcessingSearchBox"_s );
  mSearchBox->setPlaceholderText( tr( "Search algorithms…" ) );
  mSearchBox->setShowSearchIcon( true );
  mSearchBox->setShowClearButton( true );
  layout->addWidget( mSearchBox );

  // Tree view
  mTree = new QgsProcessingToolboxTreeView( container );
  mTree->setObjectName( u"mProcessingTree"_s );
  mTree->setIconSize( QSize( 20, 20 ) );
  mTree->setIndentation( 20 );
  mTree->setRegistry( QgsApplication::processingRegistry(), QgsGui::instance()->processingRecentAlgorithmLog(), QgsGui::instance()->processingFavoriteAlgorithmManager() );
  mTree->setContextMenuPolicy( Qt::CustomContextMenu );
  mTree->header()->hide();

  layout->addWidget( mTree );
  container->setLayout( layout );
  setWidget( container );

  connect( mSearchBox, &QLineEdit::textChanged, mTree, &QgsProcessingToolboxTreeView::setFilterString );
  connect( mActionExpandAll, &QAction::triggered, mTree, &QTreeView::expandAll );
  connect( mActionCollapseAll, &QAction::triggered, mTree, &QTreeView::collapseAll );
  connect( mTree, &QTreeView::doubleClicked, this, &QgsProcessingToolboxDockWidget::onTreeDoubleClicked );
  connect( mTree, &QWidget::customContextMenuRequested, this, &QgsProcessingToolboxDockWidget::onCustomContextMenu );
}

void QgsProcessingToolboxDockWidget::onTreeDoubleClicked( const QModelIndex &index )
{
  const QgsProcessingAlgorithm *alg = mTree->algorithmForIndex( index );
  if ( alg )
  {
    openAlgorithm( alg );
  }
}

void QgsProcessingToolboxDockWidget::onCustomContextMenu( const QPoint &point )
{
  const QModelIndex idx = mTree->indexAt( point );
  const QgsProcessingAlgorithm *alg = mTree->algorithmForIndex( idx );
  if ( !alg )
    return;

  QMenu menu( this );
  QAction *actExec = menu.addAction( tr( "Execute…" ) );
  connect( actExec, &QAction::triggered, this, [this, alg]() { openAlgorithm( alg ); } );

  QAction *actFav = menu.addAction(
    QgsGui::instance()->processingFavoriteAlgorithmManager()->isFavorite( alg->id() )
      ? tr( "Remove from Favorites" )
      : tr( "Add to Favorites" )
  );
  connect( actFav, &QAction::triggered, this, [alg]() {
    if ( QgsGui::instance()->processingFavoriteAlgorithmManager()->isFavorite( alg->id() ) )
      QgsGui::instance()->processingFavoriteAlgorithmManager()->remove( alg->id() );
    else
      QgsGui::instance()->processingFavoriteAlgorithmManager()->add( alg->id() );
  } );

  menu.exec( mTree->viewport()->mapToGlobal( point ) );
}

void QgsProcessingToolboxDockWidget::openAlgorithm( const QgsProcessingAlgorithm *algorithm )
{
  if ( !algorithm )
    return;

  QgsNativeProcessingAlgorithmDialog *dialog = new QgsNativeProcessingAlgorithmDialog( algorithm, QgisApp::instance() );
  dialog->exec();
}

void QgsProcessingToolboxDockWidget::openModelDesigner()
{
  if ( QgsPythonRunner::isValid() )
  {
    QgsPythonRunner::run( u"from processing.modeler.ModelerDialog import ModelerDialog; ModelerDialog.create()"_s );
    return;
  }
  QgsModelDesignerDialog *dlg = new QgsModelDesignerDialog( this );
  dlg->show();
}

void QgsProcessingToolboxDockWidget::openHistory()
{
  QgsProcessingHistoryDialog dlg( this );
  dlg.exec();
}

#include "qgsprocessingtoolboxdockwidget.moc"

