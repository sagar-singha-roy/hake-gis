/***************************************************************************
 *  qgspythonconsoledockwidget.h
 *  -------------------
 *  Native Python Console dock widget for Hake GeoDesk
 ***************************************************************************/

#ifndef QGSPYTHONCONSOLEDOCKWIDGET_H
#define QGSPYTHONCONSOLEDOCKWIDGET_H

#include "qgsdockwidget.h"
#include <QProcess>
#include <QStringList>

class QgsCodeEditorPython;
class QToolBar;
class QSplitter;
class QTabWidget;
class QAction;
class QWidget;
class QLabel;
class QKeyEvent;

class QgsPythonConsoleDockWidget : public QgsDockWidget
{
    Q_OBJECT

  public:
    explicit QgsPythonConsoleDockWidget( QWidget *parent = nullptr );
    ~QgsPythonConsoleDockWidget() override;

    //! Activates and focuses the console input
    void activate();

    //! Executes a command string in the console
    void executeCommand( const QString &command );

  public slots:
    //! Executes the command currently typed in the command input
    void executeCurrentCommand();

    //! Executes the script in the active editor tab
    void runActiveScript();

    //! Clears the console output
    void clearConsole();

    //! Toggles the visibility of the script editor side panel
    void toggleEditor( bool visible );

    //! Opens python / console help
    void showHelp();

    //! Opens a script from disk into a new editor tab
    void openScript();

    //! Saves the script in the active tab
    void saveScript();

    //! Creates a new blank script tab
    void newScript();

  protected:
    void changeEvent( QEvent *event ) override;

  private slots:
    void onStdoutReady();
    void onStderrReady();
    void onProcessFinished( int exitCode, QProcess::ExitStatus exitStatus );
    void onProcessError( QProcess::ProcessError error );

  private:
    void setupUi();
    void startPythonProcess();
    void writeBanner();
    void appendOutput( const QString &text, bool isError = false );
    void sendToProcess( const QString &code );
    void applyTheme();
    QString findPythonExecutable() const;

    QToolBar *mToolBar = nullptr;
    QAction *mActionRun = nullptr;
    QAction *mActionClear = nullptr;
    QAction *mActionToggleEditor = nullptr;
    QAction *mActionOptions = nullptr;
    QAction *mActionHelp = nullptr;

    QSplitter *mSplitter = nullptr;

    // Console side
    QWidget *mConsoleContainer = nullptr;
    QgsCodeEditorPython *mOutputEditor = nullptr;
    QWidget *mInputContainer = nullptr;
    QgsCodeEditorPython *mInputEditor = nullptr;

    // Editor side
    QWidget *mEditorContainer = nullptr;
    QTabWidget *mEditorTabs = nullptr;

    // Process
    QProcess *mProcess = nullptr;
    QString mPythonPath;
    bool mSuppressStartupBanner = true;
};

#endif // QGSPYTHONCONSOLEDOCKWIDGET_H

