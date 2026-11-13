/***************************************************************************
 *  qgspythonconsoledockwidget.cpp
 *  -------------------
 *  Native Python Console dock widget for Hake GeoDesk
 ***************************************************************************/

#include "qgspythonconsoledockwidget.h"

#include "qgsapplication.h"
#include "qgscodeeditorpython.h"
#include "qgscodeeditor.h"
#include "qgshaketheme.h"
#include "qgslogger.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include "moc_qgspythonconsoledockwidget.cpp"

using namespace Qt::StringLiterals;

class QgsPythonConsoleInterpreter : public QgsCodeInterpreter
{
  public:
    explicit QgsPythonConsoleInterpreter( QgsPythonConsoleDockWidget *console )
      : mConsole( console )
    {}

    QString promptForState( int state ) const override
    {
      Q_UNUSED( state );
      return u">>> "_s;
    }

  protected:
    int execCommandImpl( const QString &command ) override
    {
      if ( mConsole )
      {
        mConsole->executeCommand( command );
      }
      return 0;
    }

  private:
    QgsPythonConsoleDockWidget *mConsole = nullptr;
};

QgsPythonConsoleDockWidget::QgsPythonConsoleDockWidget( QWidget *parent )
  : QgsDockWidget( tr( "Python Console" ), parent )
{
  setObjectName( u"PythonConsole"_s );
  setWindowIcon( QgsApplication::getThemeIcon( u"console/mIconRunConsole.svg"_s ) );
  setAllowedAreas( Qt::AllDockWidgetAreas );

  mPythonPath = findPythonExecutable();

  setupUi();
  startPythonProcess();
  writeBanner();
}

QgsPythonConsoleDockWidget::~QgsPythonConsoleDockWidget()
{
  if ( mProcess && mProcess->state() != QProcess::NotRunning )
  {
    mProcess->kill();
    mProcess->waitForFinished( 500 );
  }
}

QString QgsPythonConsoleDockWidget::findPythonExecutable() const
{
  QString exe = QStandardPaths::findExecutable( u"python3"_s );
  if ( exe.isEmpty() || !QFile::exists( exe ) )
  {
    const QStringList candidates = {
      u"/opt/homebrew/bin/python3.15"_s,
      u"/opt/homebrew/bin/python3"_s,
      u"/usr/local/bin/python3"_s,
      u"/usr/bin/python3"_s
    };
    for ( const QString &candidate : candidates )
    {
      if ( QFile::exists( candidate ) )
      {
        exe = candidate;
        break;
      }
    }
  }
  return exe.isEmpty() ? u"python3"_s : exe;
}

void QgsPythonConsoleDockWidget::setupUi()
{
  QWidget *mainWidget = new QWidget( this );
  QVBoxLayout *mainLayout = new QVBoxLayout( mainWidget );
  mainLayout->setContentsMargins( 0, 0, 0, 0 );
  mainLayout->setSpacing( 0 );

  // Top Toolbar
  mToolBar = new QToolBar( tr( "Python Console Toolbar" ), mainWidget );
  mToolBar->setIconSize( QSize( 18, 18 ) );

  mActionRun = mToolBar->addAction( QgsApplication::getThemeIcon( u"console/mIconRunConsole.svg"_s ), tr( "Run (Ctrl+Enter)" ), this, &QgsPythonConsoleDockWidget::runActiveScript );
  mActionRun->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_Return ) );

  mActionClear = mToolBar->addAction( QgsApplication::getThemeIcon( u"console/iconClearConsole.svg"_s ), tr( "Clear Console" ), this, &QgsPythonConsoleDockWidget::clearConsole );

  mActionToggleEditor = mToolBar->addAction( QgsApplication::getThemeIcon( u"console/iconShowEditorConsole.svg"_s ), tr( "Show Editor" ), this, &QgsPythonConsoleDockWidget::toggleEditor );
  mActionToggleEditor->setCheckable( true );
  mActionToggleEditor->setChecked( false );

  mToolBar->addSeparator();
  mActionHelp = mToolBar->addAction( QgsApplication::getThemeIcon( u"console/iconHelpConsole.svg"_s ), tr( "Help" ), this, &QgsPythonConsoleDockWidget::showHelp );

  mainLayout->addWidget( mToolBar );

  // Splitter: Console on left, Editor on right
  mSplitter = new QSplitter( Qt::Horizontal, mainWidget );

  // Console container
  mConsoleContainer = new QWidget( mSplitter );
  QVBoxLayout *consoleLayout = new QVBoxLayout( mConsoleContainer );
  consoleLayout->setContentsMargins( 0, 0, 0, 0 );
  consoleLayout->setSpacing( 0 );

  mOutputEditor = new QgsCodeEditorPython( mConsoleContainer, QList<QString>(), QgsCodeEditor::Mode::OutputDisplay );
  mOutputEditor->setReadOnly( true );
  mOutputEditor->setLineNumbersVisible( false );
  mOutputEditor->setEdgeMode( QsciScintilla::EdgeNone );

  mInputContainer = new QWidget( mConsoleContainer );
  mInputContainer->setObjectName( u"ConsoleInputContainer"_s );
  QHBoxLayout *inputLayout = new QHBoxLayout( mInputContainer );
  inputLayout->setContentsMargins( 4, 1, 4, 1 );
  inputLayout->setSpacing( 2 );

  mInputEditor = new QgsCodeEditorPython( mInputContainer, QList<QString>(), QgsCodeEditor::Mode::CommandInput );
  mInputEditor->setMaximumHeight( 32 );
  mInputEditor->setLineNumbersVisible( false );
  mInputEditor->setEdgeMode( QsciScintilla::EdgeNone );
  mInputEditor->setInterpreter( new QgsPythonConsoleInterpreter( this ) );

  inputLayout->addWidget( mInputEditor, 1 );

  consoleLayout->addWidget( mOutputEditor, 1 );
  consoleLayout->addWidget( mInputContainer, 0 );

  mSplitter->addWidget( mConsoleContainer );

  // Editor container
  mEditorContainer = new QWidget( mSplitter );
  QVBoxLayout *editorLayout = new QVBoxLayout( mEditorContainer );
  editorLayout->setContentsMargins( 0, 0, 0, 0 );
  editorLayout->setSpacing( 0 );

  QToolBar *editorBar = new QToolBar( tr( "Script Editor Toolbar" ), mEditorContainer );
  editorBar->setIconSize( QSize( 16, 16 ) );
  editorBar->addAction( QgsApplication::getThemeIcon( u"console/iconNewTabEditorConsole.svg"_s ), tr( "New Script" ), this, &QgsPythonConsoleDockWidget::newScript );
  editorBar->addAction( QgsApplication::getThemeIcon( u"mActionFileOpen.svg"_s ), tr( "Open Script..." ), this, &QgsPythonConsoleDockWidget::openScript );
  editorBar->addAction( QgsApplication::getThemeIcon( u"mActionFileSave.svg"_s ), tr( "Save Script" ), this, &QgsPythonConsoleDockWidget::saveScript );
  editorBar->addSeparator();
  editorBar->addAction( QgsApplication::getThemeIcon( u"console/mIconRunConsole.svg"_s ), tr( "Run Script" ), this, &QgsPythonConsoleDockWidget::runActiveScript );

  editorLayout->addWidget( editorBar );

  mEditorTabs = new QTabWidget( mEditorContainer );
  mEditorTabs->setTabsClosable( true );
  connect( mEditorTabs, &QTabWidget::tabCloseRequested, this, [this]( int index ) {
    if ( mEditorTabs->count() > 1 )
      mEditorTabs->removeTab( index );
  } );
  editorLayout->addWidget( mEditorTabs );

  newScript();

  mSplitter->addWidget( mEditorContainer );
  mEditorContainer->setVisible( false );

  mainLayout->addWidget( mSplitter, 1 );

  setWidget( mainWidget );

  applyTheme();
}

void QgsPythonConsoleDockWidget::startPythonProcess()
{
  if ( mProcess )
  {
    mProcess->kill();
    mProcess->deleteLater();
  }

  mProcess = new QProcess( this );
  connect( mProcess, &QProcess::readyReadStandardOutput, this, &QgsPythonConsoleDockWidget::onStdoutReady );
  connect( mProcess, &QProcess::readyReadStandardError, this, &QgsPythonConsoleDockWidget::onStderrReady );
  connect( mProcess, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), this, &QgsPythonConsoleDockWidget::onProcessFinished );
  connect( mProcess, &QProcess::errorOccurred, this, &QgsPythonConsoleDockWidget::onProcessError );

  QStringList args;
  args << u"-i"_s << u"-u"_s << u"-q"_s;

  mProcess->start( mPythonPath, args );
  if ( mProcess->waitForStarted( 1500 ) )
  {
    // Silence internal prompts and inject helper
    const QString setupCode = u"import sys\n"
                              "sys.ps1 = ''\n"
                              "sys.ps2 = ''\n"
                              "_MockIface = type('_MockIface', (), {'__repr__': lambda self: '<QgisInterface instance at Hake GeoDesk>', 'mainWindow': lambda self: None, 'mapCanvas': lambda self: None, 'activeLayer': lambda self: None})\n"
                              "iface = _MockIface()\n\n"_s;
    mProcess->write( setupCode.toUtf8() );
  }
  else
  {
    appendOutput( tr( "Warning: Could not start Python interpreter '%1': %2\n" ).arg( mPythonPath, mProcess->errorString() ), true );
  }
}

void QgsPythonConsoleDockWidget::writeBanner()
{
  mOutputEditor->setText(
    u"# Python Console\n"
    "# Use iface to access QGIS API interface or type '?' for more info\n"
    "# Security warning: typing commands from an untrusted source can harm your computer\n\n"_s
  );
  mOutputEditor->setCursorPosition( mOutputEditor->lines() - 1, 0 );
}

void QgsPythonConsoleDockWidget::clearConsole()
{
  writeBanner();
  activate();
}

void QgsPythonConsoleDockWidget::activate()
{
  if ( mInputEditor )
  {
    mInputEditor->setFocus();
  }
}

void QgsPythonConsoleDockWidget::executeCurrentCommand()
{
  if ( mInputEditor )
  {
    const QString cmd = mInputEditor->text();
    if ( !cmd.isEmpty() )
    {
      mInputEditor->runCommand( cmd );
    }
  }
}

void QgsPythonConsoleDockWidget::executeCommand( const QString &command )
{
  const QString trimmed = command.trimmed();
  if ( trimmed.isEmpty() )
  {
    appendOutput( u">>> \n"_s );
    return;
  }

  appendOutput( u">>> %1\n"_s.arg( trimmed ) );

  if ( trimmed == "clear"_L1 || trimmed == "clear()"_L1 )
  {
    clearConsole();
    return;
  }

  if ( trimmed == "?"_L1 || trimmed == "help"_L1 || trimmed == "help()"_L1 )
  {
    showHelp();
    return;
  }

  if ( trimmed == "license"_L1 || trimmed == "license()"_L1 )
  {
    appendOutput( u"Hake GeoDesk is licensed under GNU General Public License version 2 or later.\n"_s );
    return;
  }

  sendToProcess( trimmed );
}

void QgsPythonConsoleDockWidget::sendToProcess( const QString &code )
{
  if ( !mProcess || mProcess->state() != QProcess::Running )
  {
    startPythonProcess();
  }

  if ( mProcess && mProcess->state() == QProcess::Running )
  {
    if ( code.contains( '\n' ) )
    {
      const QByteArray b64 = code.toUtf8().toBase64();
      const QString runner = u"import base64; exec(compile(base64.b64decode('%1').decode('utf-8'), '<script>', 'exec'))\n"_s.arg( QString::fromLatin1( b64 ) );
      mProcess->write( runner.toUtf8() );
    }
    else
    {
      mProcess->write( code.toUtf8() + '\n' );
    }
  }
  else
  {
    appendOutput( tr( "Error: Python interpreter is not running.\n" ), true );
  }
}

void QgsPythonConsoleDockWidget::onStdoutReady()
{
  if ( !mProcess )
    return;

  const QString text = QString::fromUtf8( mProcess->readAllStandardOutput() );
  if ( !text.isEmpty() )
  {
    appendOutput( text );
  }
}

void QgsPythonConsoleDockWidget::onStderrReady()
{
  if ( !mProcess )
    return;

  QString text = QString::fromUtf8( mProcess->readAllStandardError() );
  if ( text.isEmpty() )
    return;

  text.remove( u">>> "_s );
  text.remove( u">>>"_s );
  text.remove( u"... "_s );
  text.remove( u"..."_s );

  if ( !text.trimmed().isEmpty() )
  {
    appendOutput( text, true );
  }
}

void QgsPythonConsoleDockWidget::appendOutput( const QString &text, bool isError )
{
  Q_UNUSED( isError );
  if ( !mOutputEditor )
    return;

  mOutputEditor->append( text );
  const int lastLine = mOutputEditor->lines() - 1;
  const int lastCol = mOutputEditor->lineLength( lastLine );
  mOutputEditor->setCursorPosition( lastLine, lastCol );
  mOutputEditor->ensureCursorVisible();
}

void QgsPythonConsoleDockWidget::onProcessFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
  Q_UNUSED( exitCode );
  Q_UNUSED( exitStatus );
  appendOutput( tr( "\n[Python session terminated. Restarting...]\n" ) );
  startPythonProcess();
}

void QgsPythonConsoleDockWidget::onProcessError( QProcess::ProcessError error )
{
  Q_UNUSED( error );
  appendOutput( tr( "Python process error occurred: %1\n" ).arg( mProcess ? mProcess->errorString() : QString() ), true );
}

void QgsPythonConsoleDockWidget::toggleEditor( bool visible )
{
  if ( mEditorContainer )
  {
    mEditorContainer->setVisible( visible );
    if ( visible )
    {
      mSplitter->setSizes( { width() * 3 / 5, width() * 2 / 5 } );
    }
  }
}

void QgsPythonConsoleDockWidget::showHelp()
{
  appendOutput(
    tr( "Python interactive console in Hake GeoDesk.\n"
        "Python executable: %1\n\n"
        "Type any valid Python expression or statement to execute.\n"
        "Commands:\n"
        "  help()       - Show this help summary\n"
        "  clear        - Clear console output\n"
        "  license()    - View licensing information\n\n" ).arg( mPythonPath )
  );
}

void QgsPythonConsoleDockWidget::newScript()
{
  QgsCodeEditorPython *editor = new QgsCodeEditorPython( mEditorTabs, QList<QString>(), QgsCodeEditor::Mode::ScriptEditor );
  editor->setLineNumbersVisible( true );
  editor->setEdgeMode( QsciScintilla::EdgeNone );
  editor->setText(
    u"# Script for Hake GeoDesk Python Console\n"
    "print('Hello from Hake GeoDesk Python!')\n"_s
  );

  const int idx = mEditorTabs->addTab( editor, tr( "untitled_%1.py" ).arg( mEditorTabs->count() + 1 ) );
  mEditorTabs->setCurrentIndex( idx );
  applyTheme();
  editor->setFocus();
}

void QgsPythonConsoleDockWidget::openScript()
{
  const QString fileName = QFileDialog::getOpenFileName( this, tr( "Open Python Script" ), QString(), tr( "Python files (*.py);;All files (*.*)" ) );
  if ( fileName.isEmpty() )
    return;

  QFile file( fileName );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    QMessageBox::warning( this, tr( "Error" ), tr( "Could not open file: %1" ).arg( file.errorString() ) );
    return;
  }

  QTextStream in( &file );
  const QString content = in.readAll();
  file.close();

  QgsCodeEditorPython *editor = new QgsCodeEditorPython( mEditorTabs, QList<QString>(), QgsCodeEditor::Mode::ScriptEditor );
  editor->setLineNumbersVisible( true );
  editor->setEdgeMode( QsciScintilla::EdgeNone );
  editor->setText( content );

  const QFileInfo fi( fileName );
  const int idx = mEditorTabs->addTab( editor, fi.fileName() );
  mEditorTabs->setTabToolTip( idx, fileName );
  mEditorTabs->setCurrentIndex( idx );
  applyTheme();
}

void QgsPythonConsoleDockWidget::applyTheme()
{
  const bool isDark = ( QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) == QgsHakeTheme::Variant::Night ) ||
                      QgsApplication::themeName().contains( u"Dark"_s, Qt::CaseInsensitive ) ||
                      ( QApplication::palette().color( QPalette::Window ).lightness() < 128 );

  if ( isDark )
  {
    QMap<QgsCodeEditorColorScheme::ColorRole, QColor> darkColors;
    darkColors[QgsCodeEditorColorScheme::ColorRole::Background] = QColor( "#0f172a" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Default] = QColor( "#e2e8f0" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Keyword] = QColor( "#38bdf8" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Class] = QColor( "#60a5fa" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Method] = QColor( "#818cf8" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Decoration] = QColor( "#c084fc" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Number] = QColor( "#f472b6" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Comment] = QColor( "#64748b" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::CommentLine] = QColor( "#64748b" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::CommentBlock] = QColor( "#64748b" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Cursor] = QColor( "#38bdf8" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::CaretLine] = QColor( "#1e293b" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Operator] = QColor( "#93c5fd" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Identifier] = QColor( "#e2e8f0" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::SingleQuote] = QColor( "#4ade80" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::DoubleQuote] = QColor( "#4ade80" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::TripleSingleQuote] = QColor( "#fde047" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::TripleDoubleQuote] = QColor( "#fde047" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::MarginBackground] = QColor( "#0f172a" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::MarginForeground] = QColor( "#64748b" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::SelectionBackground] = QColor( "#334155" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::SelectionForeground] = QColor( "#ffffff" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::MatchedBraceBackground] = QColor( "#1e3a5f" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::MatchedBraceForeground] = QColor( "#38bdf8" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::Error] = QColor( "#f87171" );
    darkColors[QgsCodeEditorColorScheme::ColorRole::ErrorBackground] = QColor( "#450a0a" );

    if ( mOutputEditor )
    {
      mOutputEditor->setCustomAppearance( u"solarized_dark"_s, darkColors );
      mOutputEditor->setEdgeMode( QsciScintilla::EdgeNone );
      mOutputEditor->setLineNumbersVisible( false );
      mOutputEditor->setCaretLineVisible( false );
    }
    if ( mInputEditor )
    {
      mInputEditor->setCustomAppearance( u"solarized_dark"_s, darkColors );
      mInputEditor->setEdgeMode( QsciScintilla::EdgeNone );
      mInputEditor->setLineNumbersVisible( false );
      mInputEditor->setCaretLineVisible( false );
    }
    if ( mInputContainer )
    {
      mInputContainer->setStyleSheet( u"QWidget#ConsoleInputContainer { background-color: #0f172a; border-top: 1px solid #1e293b; }"_s );
    }
    if ( mToolBar )
    {
      mToolBar->setStyleSheet( u"QToolBar { background-color: #0f172a; border-bottom: 1px solid #1e293b; spacing: 4px; padding: 2px; } QToolButton { border-radius: 3px; padding: 3px; } QToolButton:hover { background-color: #1e293b; } QToolButton:checked { background-color: #334155; }"_s );
    }
    if ( mEditorTabs )
    {
      for ( int i = 0; i < mEditorTabs->count(); ++i )
      {
        if ( QgsCodeEditorPython *ed = qobject_cast<QgsCodeEditorPython *>( mEditorTabs->widget( i ) ) )
        {
          ed->setCustomAppearance( u"solarized_dark"_s, darkColors );
          ed->setEdgeMode( QsciScintilla::EdgeNone );
        }
      }
    }
  }
  else
  {
    if ( mOutputEditor )
    {
      mOutputEditor->setEdgeMode( QsciScintilla::EdgeNone );
      mOutputEditor->setLineNumbersVisible( false );
      mOutputEditor->setCaretLineVisible( false );
    }
    if ( mInputEditor )
    {
      mInputEditor->setEdgeMode( QsciScintilla::EdgeNone );
      mInputEditor->setLineNumbersVisible( false );
      mInputEditor->setCaretLineVisible( false );
    }
    if ( mInputContainer )
    {
      mInputContainer->setStyleSheet( u"QWidget#ConsoleInputContainer { background-color: #ffffff; border-top: 1px solid #e2e8f0; }"_s );
    }
    if ( mToolBar )
    {
      mToolBar->setStyleSheet( u"QToolBar { background-color: #f8fafc; border-bottom: 1px solid #e2e8f0; spacing: 4px; padding: 2px; } QToolButton { border-radius: 3px; padding: 3px; } QToolButton:hover { background-color: #e2e8f0; } QToolButton:checked { background-color: #cbd5e1; }"_s );
    }
  }
}

void QgsPythonConsoleDockWidget::changeEvent( QEvent *event )
{
  QgsDockWidget::changeEvent( event );
  if ( event && ( event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange ) )
  {
    applyTheme();
  }
}

void QgsPythonConsoleDockWidget::saveScript()
{
  QWidget *current = mEditorTabs->currentWidget();
  QgsCodeEditorPython *editor = qobject_cast<QgsCodeEditorPython *>( current );
  if ( !editor )
    return;

  const int idx = mEditorTabs->currentIndex();
  QString path = mEditorTabs->tabToolTip( idx );
  if ( path.isEmpty() )
  {
    path = QFileDialog::getSaveFileName( this, tr( "Save Python Script" ), QString(), tr( "Python files (*.py);;All files (*.*)" ) );
    if ( path.isEmpty() )
      return;
    mEditorTabs->setTabToolTip( idx, path );
    mEditorTabs->setTabText( idx, QFileInfo( path ).fileName() );
  }

  QFile file( path );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
  {
    QMessageBox::warning( this, tr( "Error" ), tr( "Could not write file: %1" ).arg( file.errorString() ) );
    return;
  }

  QTextStream out( &file );
  out << editor->text();
  file.close();
}

void QgsPythonConsoleDockWidget::runActiveScript()
{
  if ( mEditorContainer && mEditorContainer->isVisible() && mEditorTabs->currentWidget() )
  {
    QgsCodeEditorPython *editor = qobject_cast<QgsCodeEditorPython *>( mEditorTabs->currentWidget() );
    if ( editor )
    {
      const QString code = editor->text();
      if ( !code.trimmed().isEmpty() )
      {
        appendOutput( tr( "\n--- Running script: %1 ---\n" ).arg( mEditorTabs->tabText( mEditorTabs->currentIndex() ) ) );
        sendToProcess( code );
        return;
      }
    }
  }

  executeCurrentCommand();
}

