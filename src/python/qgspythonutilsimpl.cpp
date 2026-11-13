/***************************************************************************
    qgspythonutils.cpp - routines for interfacing Python
    ---------------------
    begin                : October 2006
    copyright            : (C) 2006 by Martin Dobias
    email                : wonder.sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// python should be first include
// otherwise issues some warnings
#ifdef _MSC_VER
#ifdef _DEBUG
#undef _DEBUG
#endif
#endif
#include <Python.h>

#include "qgspythonutilsimpl.h"

#include "qgis.h"
#include "qgsapplication.h"
#include "qgslogger.h"
#include "qgsmessageoutput.h"
#include "qgssettings.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

using namespace Qt::StringLiterals;

PyThreadState *_mainState = nullptr;

QgsPythonUtilsImpl::QgsPythonUtilsImpl()
{}

QgsPythonUtilsImpl::~QgsPythonUtilsImpl()
{
  exitPython();
}

bool QgsPythonUtilsImpl::setMainDictString( const char *name, const QString &value )
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *obj = PyUnicode_FromString( value.toUtf8().constData() );
  const int rc = obj ? PyDict_SetItemString( mMainDict, name, obj ) : -1;
  Py_XDECREF( obj );
  PyGILState_Release( gstate );
  return rc == 0;
}

bool QgsPythonUtilsImpl::setMainDictStringList( const char *name, const QStringList &values )
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *list = PyList_New( values.size() );
  if ( !list )
  {
    PyGILState_Release( gstate );
    return false;
  }

  for ( int i = 0; i < values.size(); ++i )
  {
    PyObject *item = PyUnicode_FromString( values.at( i ).toUtf8().constData() );
    if ( !item )
    {
      Py_DECREF( list );
      PyGILState_Release( gstate );
      return false;
    }
    PyList_SET_ITEM( list, i, item ); // steals reference
  }

  const int rc = PyDict_SetItemString( mMainDict, name, list );
  Py_DECREF( list );
  PyGILState_Release( gstate );
  return rc == 0;
}

bool QgsPythonUtilsImpl::checkSystemImports()
{
  runString( u"import sys"_s );     // import sys module (for display / exception hooks)
  runString( u"import os"_s );      // import os module (for environ variables)
  runString( u"import pathlib"_s ); // import pathlib module (for path manipulation)

  const QString pyPath = pythonPath();
  const QString plugPath = pluginsPath();
  QString homePyPath = QgsApplication::qgisSettingsDirPath();
  if ( QDir::cleanPath( homePyPath ) == QDir::homePath() + u"/.qgis3"_s )
    homePyPath = QDir::homePath() + u"/.qgis3/python"_s;
  else
    homePyPath += u"python"_s;
  const QString homePlugPath = homePyPath + u"/plugins"_s;
  const QString expectedQgisDir = QDir::cleanPath( pyPath + u"/qgis"_s );
  const QStringList extraPaths = extraPluginsPaths();

  if ( !setMainDictString( "_hake_python_path", pyPath )
       || !setMainDictString( "_hake_plugins_path", plugPath )
       || !setMainDictString( "_hake_home_python_path", homePyPath )
       || !setMainDictString( "_hake_home_plugins_path", homePlugPath )
       || !setMainDictString( "_hake_expected_qgis_dir", expectedQgisDir )
       || !setMainDictStringList( "_hake_extra_plugin_paths", extraPaths ) )
  {
    return false;
  }

  // support for PYTHONSTARTUP-like environment variable: PYQGIS_STARTUP
  // (unlike PYTHONHOME and PYTHONPATH, PYTHONSTARTUP is not supported for embedded interpreter by default)
  // this is different than user's 'startup.py' (below), since it is loaded just after Py_Initialize
  // it is very useful for cleaning sys.path, which may have undesirable paths, or for
  // isolating/loading the initial environ without requiring a virt env, e.g. homebrew or MacPorts installs on Mac
  runString( u"pyqgstart = os.getenv('PYQGIS_STARTUP')"_s );
  runString(
    QStringLiteral( R""""(
exec(
    compile(
        """
class StartupScriptRunner:
    def __init__(self):
        self.info_messages: list[str] = []
        self.warning_messages: list[str] = []

    def run_startup_script(self, script_path: 'pathlib.Path | str | None') -> bool:
        script_executed = False
        if not script_path:
            return script_executed

        p1 = pathlib.Path(script_path)
        if p1.exists():
            self.info_messages.append(f"Executed startup script: {p1}")
            code = compile(p1.read_text(), p1, 'exec')
            exec(code, globals())
            script_executed = True

        p2 = pathlib.Path(_hake_python_path) / script_path
        if p2.exists() and p2 != p1:
            self.info_messages.append(f"Executed startup script: {p2}")
            code = compile(p2.read_text(), p2, 'exec')
            exec(code, globals())
            script_executed = True

        if not script_executed:
            self.warning_messages.append(
                f"Startup script not executed - neither {p1} nor {p2} exist!"
            )

        return script_executed

    def log_messages(self):
        from qgis.core import Qgis, QgsMessageLog

        for msg in self.info_messages:
            QgsMessageLog.logMessage(msg, "QGIS", Qgis.MessageLevel.Info)

        for msg in self.warning_messages:
            QgsMessageLog.logMessage(msg, "QGIS", Qgis.MessageLevel.Warning)

_ssr = StartupScriptRunner()
        """,
        'QgsPythonUtilsImpl::checkSystemImports [run_startup_script]',
        'exec',
    ),
    globals(),
)
)"""" ),
    QObject::tr( "Couldn't create run_startup_script." ),
    true
  );
  runString( u"is_startup_script_executed = _ssr.run_startup_script(pyqgstart)"_s );

#ifdef Q_OS_WIN
  runString( "oldhome=None" );
  runString( "if 'HOME' in os.environ: oldhome=os.environ['HOME']\n" );
  runString( "os.environ['HOME']=os.environ['USERPROFILE']\n" );
#endif

  for ( const QString &path : extraPaths )
  {
    if ( !QDir( path ).exists() )
    {
      QgsMessageOutput *msg = QgsMessageOutput::createMessageOutput();
      msg->setTitle( QObject::tr( "Python error" ) );
      msg->setMessage( QObject::tr( "The extra plugin path '%1' does not exist!" ).arg( path ), Qgis::StringFormat::PlainText );
      msg->showMessage();
    }
  }

  // expect that bindings are installed locally, so add the path to modules
  // also add path to plugins. Paths are Unicode objects, not interpolated source.
  runString(
    u"sys.path = [_hake_python_path, _hake_home_python_path] + _hake_extra_plugin_paths + "
    "[_hake_home_plugins_path, _hake_plugins_path] + sys.path"_s
  );

  // Ensure `import qgis` resolves to this build/install (not a system Qt5 package).
  // Must run before any qgis.* import to avoid loading mismatched native modules.
  {
    const QString pathCheckError = QObject::tr( "Couldn't load PyHake Geospatial." ) + '\n'
                                   + QObject::tr( "Python support will be disabled." );
    if ( !runString(
           u"import importlib.util, os\n"
           "_exp = os.path.realpath(_hake_expected_qgis_dir)\n"
           "_spec = importlib.util.find_spec('qgis')\n"
           "if _spec is None:\n"
           "    raise ImportError('qgis package not found')\n"
           "_loc = None\n"
           "if getattr(_spec, 'submodule_search_locations', None):\n"
           "    _loc = list(_spec.submodule_search_locations)[0]\n"
           "elif _spec.origin:\n"
           "    _loc = os.path.dirname(_spec.origin)\n"
           "_loc = os.path.realpath(_loc) if _loc else ''\n"
           "if not _loc or os.path.commonpath([_loc, _exp]) != _exp:\n"
           "    raise ImportError('Wrong qgis package: %r (expected under %r)' % (_loc, _exp))\n"_s,
           pathCheckError,
           false
         ) )
    {
      return false;
    }
  }

  // import SIP
  if ( !runString( u"from qgis.PyQt import sip"_s, QObject::tr( "Couldn't load SIP module." ) + '\n' + QObject::tr( "Python support will be disabled." ) ) )
  {
    return false;
  }

  // import Qt bindings
  if ( !runString( u"from qgis.PyQt import QtCore, QtGui"_s, QObject::tr( "Couldn't load PyQt." ) + '\n' + QObject::tr( "Python support will be disabled." ) ) )
  {
    return false;
  }

  // Ensure optional PyQt6 modules imported by SIP (e.g. QtMultimedia, QtPositioning)
  // have safe stubs if the system did not install python3-pyqt6.qtmultimedia
  runString(
    QStringLiteral(
      "try:\n"
      "    import PyQt6.QtMultimedia\n"
      "except ImportError:\n"
      "    import sys, types\n"
      "    try:\n"
      "        import PyQt6\n"
      "        _fake_mm = types.ModuleType('PyQt6.QtMultimedia')\n"
      "        sys.modules['PyQt6.QtMultimedia'] = _fake_mm\n"
      "        setattr(PyQt6, 'QtMultimedia', _fake_mm)\n"
      "    except Exception:\n"
      "        pass\n"
      "try:\n"
      "    import PyQt6.QtPositioning\n"
      "except ImportError:\n"
      "    import sys, types\n"
      "    try:\n"
      "        import PyQt6\n"
      "        _fake_pos = types.ModuleType('PyQt6.QtPositioning')\n"
      "        sys.modules['PyQt6.QtPositioning'] = _fake_pos\n"
      "        setattr(PyQt6, 'QtPositioning', _fake_pos)\n"
      "    except Exception:\n"
      "        pass\n"
    )
  );

  // import QGIS bindings
  QString error_msg = QObject::tr( "Couldn't load PyHake Geospatial." ) + '\n' + QObject::tr( "Python support will be disabled." );
  if (
    !runString( u"from qgis.core import *"_s, error_msg )
#ifdef HAVE_GUI
    || !runString( u"from qgis.gui import *"_s, error_msg )
#endif
  )
  {
    return false;
  }

  // import QGIS utils
  error_msg = QObject::tr( "Couldn't load Hake Geospatial utils." ) + '\n' + QObject::tr( "Python support will be disabled." );
  if ( !runString( u"import qgis.utils"_s, error_msg ) )
  {
    return false;
  }

  // tell the utils script where to look for the plugins
  runString( u"qgis.utils.plugin_paths = _hake_extra_plugin_paths + [_hake_home_plugins_path, _hake_plugins_path]"_s );
  runString( u"qgis.utils.sys_plugin_path = _hake_plugins_path"_s );
  runString( u"qgis.utils.HOME_PLUGIN_PATH = _hake_home_plugins_path"_s );

#ifdef Q_OS_WIN
  runString( "if oldhome: os.environ['HOME']=oldhome\n" );
#endif

  // now, after successful import of `qgis` module, we can show logs from `StartupScriptRunner`
  runString( u"_ssr.log_messages()"_s );

  return true;
}

void QgsPythonUtilsImpl::init()
{
#if defined( PY_MAJOR_VERSION ) && defined( PY_MINOR_VERSION ) && ( ( PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 8 ) || PY_MAJOR_VERSION > 3 )
  PyStatus status;
  PyPreConfig preconfig;
  PyPreConfig_InitPythonConfig( &preconfig );

  preconfig.utf8_mode = 1;

  status = Py_PreInitialize( &preconfig );
  if ( PyStatus_Exception( status ) )
  {
    Py_ExitStatusException( status );
  }
#endif

  PyConfig config;
  PyConfig_InitPythonConfig( &config );

#ifdef QGIS_MAC_BUNDLE
  // If we package QGIS as a mac app, we deploy Qt plugins into [app]/Contents/PlugIns
  if ( qgetenv( "PYTHONHOME" ).isNull() )
  {
    status = PyConfig_SetString( &config, &config.home, QgsApplication::libraryPath().toStdWString().c_str() );
    if ( PyStatus_Exception( status ) )
    {
      qWarning() << "Failed to set python home";
    }
  }
#endif

  status = Py_InitializeFromConfig( &config );
  if ( PyStatus_Exception( status ) )
  {
    qWarning() << "Failed to initialize from config";
  }
  PyConfig_Clear( &config );

  mPythonEnabled = true;

  mMainModule = PyImport_AddModule( "__main__" ); // borrowed reference
  mMainDict = PyModule_GetDict( mMainModule );    // borrowed reference
}

void QgsPythonUtilsImpl::finish()
{
  // release GIL!
  // Later on, we acquire GIL just before doing some Python calls and
  // release GIL again when the work with Python API is done.
  // (i.e. there must be PyGILState_Ensure + PyGILState_Release pair
  // around any calls to Python API, otherwise we may segfault!)
  _mainState = PyEval_SaveThread();
}

bool QgsPythonUtilsImpl::checkQgisUser()
{
  // import QGIS user
  const QString error_msg = QObject::tr( "Couldn't load qgis.user." ) + '\n' + QObject::tr( "Python support will be disabled." );
  if ( !runString( u"import qgis.user"_s, error_msg ) )
  {
    // Should we really bail because of this?!
    return false;
  }
  return true;
}

void QgsPythonUtilsImpl::doCustomImports()
{
  const QStringList startupPaths = QStandardPaths::locateAll( QStandardPaths::AppDataLocation, u"startup.py"_s );
  if ( startupPaths.isEmpty() )
  {
    return;
  }

  runString( u"import importlib.util"_s );

  QStringList::const_iterator iter = startupPaths.constBegin();
  for ( ; iter != startupPaths.constEnd(); ++iter )
  {
    runString( u"spec = importlib.util.spec_from_file_location('startup','%1')"_s.arg( *iter ) );
    runString( u"module = importlib.util.module_from_spec(spec)"_s );
    runString( u"spec.loader.exec_module(module)"_s );
  }
}

void QgsPythonUtilsImpl::initPython( QgisInterface *interface, const bool installErrorHook, const QString &faultHandlerLogPath )
{
  init();
  if ( !checkSystemImports() )
  {
    exitPython();
    return;
  }

  if ( !faultHandlerLogPath.isEmpty() )
  {
    runString( u"import faulthandler"_s );
    QString escapedPath = faultHandlerLogPath;
    escapedPath.replace( '\\', "\\\\"_L1 );
    escapedPath.replace( '\'', "\\'"_L1 );
    runString( u"qgis.utils.__qgis_fault_handler_file_path='%1'"_s.arg( escapedPath ) );
    runString( u"qgis.utils.__qgis_fault_handler_file=open('%1', 'wt')"_s.arg( escapedPath ) );
    runString( u"faulthandler.enable(file=qgis.utils.__qgis_fault_handler_file)"_s );
    mFaultHandlerLogPath = faultHandlerLogPath;
  }

  if ( interface )
  {
    // initialize 'iface' object
    runString( u"qgis.utils.initInterface(%1)"_s.arg( reinterpret_cast<quint64>( interface ) ) );
  }

  if ( !checkQgisUser() )
  {
    exitPython();
    return;
  }
  doCustomImports();
  if ( installErrorHook )
    QgsPythonUtilsImpl::installErrorHook();
  finish();
}


#ifdef HAVE_SERVER_PYTHON_PLUGINS
void QgsPythonUtilsImpl::initServerPython( QgsServerInterface *interface )
{
  init();
  if ( !checkSystemImports() )
  {
    exitPython();
    return;
  }

  // This is the main difference with initInterface() for desktop plugins
  // import QGIS Server bindings
  const QString error_msg = QObject::tr( "Couldn't load PyHake Geospatial Server." ) + '\n' + QObject::tr( "Python support will be disabled." );
  if ( !runString( u"from qgis.server import *"_s, error_msg ) )
  {
    return;
  }

  // This is the other main difference with initInterface() for desktop plugins
  runString( u"qgis.utils.initServerInterface(%1)"_s.arg( reinterpret_cast<quint64>( interface ) ) );

  doCustomImports();
  finish();
}

bool QgsPythonUtilsImpl::startServerPlugin( QString packageName )
{
  QString output;
  evalString( u"qgis.utils.startServerPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

#endif // End HAVE_SERVER_PYTHON_PLUGINS

void QgsPythonUtilsImpl::exitPython()
{
  // don't try to gracefully cleanup faulthandler on windows -- see https://github.com/qgis/QGIS/issues/53473
#ifndef Q_OS_WIN
  if ( !mFaultHandlerLogPath.isEmpty() )
  {
    runString( u"faulthandler.disable()"_s );
    runString( u"qgis.utils.__qgis_fault_handler_file.close()"_s );

    // remove fault handler log file only if it's empty
    QFile faultHandlerFile( mFaultHandlerLogPath );
    bool faultHandlerLogEmpty = false;
    if ( faultHandlerFile.open( QIODevice::ReadOnly ) )
    {
      faultHandlerLogEmpty = faultHandlerFile.size() == 0;
      faultHandlerFile.close();
    }
    if ( faultHandlerLogEmpty )
    {
      QFile::remove( mFaultHandlerLogPath );
    }

    mFaultHandlerLogPath.clear();
  }
#endif

  if ( mErrorHookInstalled )
    uninstallErrorHook();

  // causes segfault!
  //Py_Finalize();
  mMainModule = nullptr;
  mMainDict = nullptr;
  mPythonEnabled = false;
}


bool QgsPythonUtilsImpl::isEnabled()
{
  return mPythonEnabled;
}

void QgsPythonUtilsImpl::installErrorHook()
{
  runString( u"qgis.utils.installErrorHook()"_s );
  mErrorHookInstalled = true;
}

void QgsPythonUtilsImpl::uninstallErrorHook()
{
  runString( u"qgis.utils.uninstallErrorHook()"_s );
}

QString QgsPythonUtilsImpl::runStringUnsafe( const QString &command, bool single )
{
  if ( !mMainDict )
  {
    return QStringLiteral( "Python interpreter is not initialized (mMainDict is null)" );
  }

  // acquire global interpreter lock to ensure we are in a consistent state
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  QString ret;

  // TODO: convert special characters from unicode strings u"…" to \uXXXX
  // so that they're not mangled to utf-8
  // (non-unicode strings can be mangled)
  PyObject *obj = PyRun_String( command.toUtf8().constData(), single ? Py_single_input : Py_file_input, mMainDict, mMainDict );
  PyObject *errobj = PyErr_Occurred();
  if ( nullptr != errobj )
  {
    ret = getTraceback();
  }
  Py_XDECREF( obj );

  // we are done calling python API, release global interpreter lock
  PyGILState_Release( gstate );

  return ret;
}

bool QgsPythonUtilsImpl::runString( const QString &command, QString msgOnError, bool single )
{
  bool res = true;
  const QString traceback = runStringUnsafe( command, single );
  if ( traceback.isEmpty() )
    return true;
  else
    res = false;

  if ( msgOnError.isEmpty() )
  {
    // use some default message if custom hasn't been specified
    msgOnError = QObject::tr( "An error occurred during execution of following code:" ) + "\n<tt>" + command + "</tt>";
  }

  // TODO: use python implementation

  QString path, version;
  evalString( u"str(sys.path)"_s, path );
  evalString( u"sys.version"_s, version );

  QString str = "<font color=\"red\">"
                + msgOnError
                + "</font><br><pre>\n"
                + traceback
                + "\n</pre>"
                + QObject::tr( "Python version:" )
                + "<br>"
                + version
                + "<br><br>"
                + QObject::tr( "Product:" )
                + "<br>"
                + Qgis::productDisplayName()
                + "<br><br>"
                + QObject::tr( "Build version:" )
                + "<br>"
                + Qgis::productVersionLabel()
                + "<br><br>"
                + QObject::tr( "Python path:" )
                + "<br>"
                + path;
  str.replace( '\n', "<br>"_L1 ).replace( "  "_L1, "&nbsp; "_L1 );

  qDebug() << str;
  QgsMessageOutput *msg = QgsMessageOutput::createMessageOutput();
  msg->setTitle( QObject::tr( "Python error" ) );
  msg->setMessage( str, Qgis::StringFormat::Html );
  msg->showMessage();

  return res;
}

QString QgsPythonUtilsImpl::runFileUnsafe( const QString &filename )
{
  // acquire global interpreter lock to ensure we are in a consistent state
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  QString ret;

  PyObject *obj, *errobj;

  QFile file( filename );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    ret = u"Cannot open file"_s;
    goto error;
  }

  obj = PyRun_String( file.readAll().constData(), Py_file_input, mMainDict, mMainDict );
  errobj = PyErr_Occurred();
  if ( nullptr != errobj )
  {
    ret = getTraceback();
  }
  Py_XDECREF( obj );

error:
  // we are done calling python API, release global interpreter lock
  PyGILState_Release( gstate );

  return ret;
}

bool QgsPythonUtilsImpl::runFile( const QString &filename, const QString &messageOnError )
{
  const QString traceback = runFileUnsafe( filename );
  if ( traceback.isEmpty() )
    return true;

  // use some default message if custom hasn't been specified
  const QString errMsg = !messageOnError.isEmpty() ? messageOnError : QObject::tr( "An error occurred during execution of following file:" ) + "\n<tt>" + filename + "</tt>";

  QString path, version;
  evalString( u"str(sys.path)"_s, path );
  evalString( u"sys.version"_s, version );

  QString str = "<font color=\"red\">"
                + errMsg
                + "</font><br><pre>\n"
                + traceback
                + "\n</pre>"
                + QObject::tr( "Python version:" )
                + "<br>"
                + version
                + "<br><br>"
                + QObject::tr( "Product:" )
                + "<br>"
                + Qgis::productDisplayName()
                + "<br><br>"
                + QObject::tr( "Build version:" )
                + "<br>"
                + Qgis::productVersionLabel()
                + "<br><br>"
                + QObject::tr( "Python path:" )
                + "<br>"
                + path;
  str.replace( '\n', "<br>"_L1 ).replace( "  "_L1, "&nbsp; "_L1 );

  QgsMessageOutput *msg = QgsMessageOutput::createMessageOutput();
  msg->setTitle( QObject::tr( "Python error" ) );
  msg->setMessage( str, Qgis::StringFormat::Html );
  msg->showMessage();

  return false;
}

QString QgsPythonUtilsImpl::setArgvUnsafe( const QStringList &arguments )
{
  // acquire global interpreter lock to ensure we are in a consistent state
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  QString ret;

  PyObject *sysobj = nullptr, *errobj = nullptr, *argsobj = nullptr;
  sysobj = PyImport_ImportModule( "sys" );
  if ( !sysobj )
  {
    errobj = PyErr_Occurred();
    if ( errobj )
      ret = QString( "SetArgvTraceback" ) + getTraceback();
    else
      ret = "Error occurred in PyImport_ImportModule";
    goto error;
  }
  argsobj = PyList_New( arguments.size() );
  if ( !argsobj )
  {
    ret = "Error occurred in PyList_New";
    goto error;
  }
  for ( int i = 0; i != arguments.size(); ++i )
    PyList_SET_ITEM( argsobj, i, PyUnicode_FromString( arguments[i].toUtf8().constData() ) );
  if ( PyObject_SetAttrString( sysobj, "argv", argsobj ) != 0 )
  {
    ret = "Error occurred in PyObject_SetAttrString";
    goto error;
  }
error:
  Py_XDECREF( argsobj );
  Py_XDECREF( sysobj );

  // we are done calling python API, release global interpreter lock
  PyGILState_Release( gstate );

  return ret;
}

bool QgsPythonUtilsImpl::setArgv( const QStringList &arguments, const QString &messageOnError )
{
  const QString traceback = setArgvUnsafe( arguments );
  if ( traceback.isEmpty() )
    return true;

  // use some default message if custom hasn't been specified
  const QString errMsg = !messageOnError.isEmpty() ? messageOnError : QObject::tr( "An error occurred while setting sys.argv from following list:" ) + "\n<tt>" + arguments.join( ',' ) + "</tt>";

  QString path, version;
  evalString( u"str(sys.path)"_s, path );
  evalString( u"sys.version"_s, version );

  QString str = "<font color=\"red\">"
                + errMsg
                + "</font><br><pre>\n"
                + traceback
                + "\n</pre>"
                + QObject::tr( "Python version:" )
                + "<br>"
                + version
                + "<br><br>"
                + QObject::tr( "Product:" )
                + "<br>"
                + Qgis::productDisplayName()
                + "<br><br>"
                + QObject::tr( "Build version:" )
                + "<br>"
                + Qgis::productVersionLabel()
                + "<br><br>"
                + QObject::tr( "Python path:" )
                + "<br>"
                + path;
  str.replace( '\n', "<br>"_L1 ).replace( "  "_L1, "&nbsp; "_L1 );

  QgsMessageOutput *msg = QgsMessageOutput::createMessageOutput();
  msg->setTitle( QObject::tr( "Python error" ) );
  msg->setMessage( str, Qgis::StringFormat::Html );
  msg->showMessage();

  return false;
}


QString QgsPythonUtilsImpl::getTraceback()
{
#define TRACEBACK_FETCH_ERROR( what ) \
  {                                   \
    errMsg = what;                    \
    goto done;                        \
  }

  QString errMsg;
  QString result;

  PyObject *modStringIO = nullptr;
  PyObject *modTB = nullptr;
  PyObject *obStringIO = nullptr;
  PyObject *obResult = nullptr;

  PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;

  PyErr_Fetch( &type, &value, &traceback );
  PyErr_NormalizeException( &type, &value, &traceback );

  const char *iomod = "io";

  modStringIO = PyImport_ImportModule( iomod );
  if ( !modStringIO )
    TRACEBACK_FETCH_ERROR( u"can't import %1"_s.arg( iomod ) );

  obStringIO = PyObject_CallMethod( modStringIO, reinterpret_cast<const char *>( "StringIO" ), nullptr );

  /* Construct a cStringIO object */
  if ( !obStringIO )
    TRACEBACK_FETCH_ERROR( u"cStringIO.StringIO() failed"_s );

  modTB = PyImport_ImportModule( "traceback" );
  if ( !modTB )
    TRACEBACK_FETCH_ERROR( u"can't import traceback"_s );

  obResult
    = PyObject_CallMethod( modTB, reinterpret_cast<const char *>( "print_exception" ), reinterpret_cast<const char *>( "OOOOO" ), type, value ? value : Py_None, traceback ? traceback : Py_None, Py_None, obStringIO );

  if ( !obResult )
    TRACEBACK_FETCH_ERROR( u"traceback.print_exception() failed"_s );

  Py_DECREF( obResult );

  obResult = PyObject_CallMethod( obStringIO, reinterpret_cast<const char *>( "getvalue" ), nullptr );
  if ( !obResult )
    TRACEBACK_FETCH_ERROR( u"getvalue() failed."_s );

  /* And it should be a string all ready to go - duplicate it. */
  if ( !PyUnicode_Check( obResult ) )
    TRACEBACK_FETCH_ERROR( u"getvalue() did not return a string"_s );

  result = QString::fromUtf8( PyUnicode_AsUTF8( obResult ) );

done:

  // All finished - first see if we encountered an error
  if ( result.isEmpty() && !errMsg.isEmpty() )
  {
    result = errMsg;
  }

  Py_XDECREF( modStringIO );
  Py_XDECREF( modTB );
  Py_XDECREF( obStringIO );
  Py_XDECREF( obResult );
  Py_XDECREF( value );
  Py_XDECREF( traceback );
  Py_XDECREF( type );

  return result;
}

QString QgsPythonUtilsImpl::getTypeAsString( PyObject *obj )
{
  if ( !obj )
    return QString();

  if ( PyType_Check( obj ) )
  {
    QgsDebugMsgLevel( u"got type"_s, 2 );
    return QString( ( ( PyTypeObject * ) obj )->tp_name );
  }
  else
  {
    QgsDebugMsgLevel( u"got object"_s, 2 );
    return PyObjectToQString( obj );
  }
}

bool QgsPythonUtilsImpl::getError( QString &errorClassName, QString &errorText )
{
  // acquire global interpreter lock to ensure we are in a consistent state
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  if ( !PyErr_Occurred() )
  {
    PyGILState_Release( gstate );
    return false;
  }

  PyObject *err_type = nullptr;
  PyObject *err_value = nullptr;
  PyObject *err_tb = nullptr;

  // get the exception information and clear error
  PyErr_Fetch( &err_type, &err_value, &err_tb );

  // get exception's class name
  errorClassName = getTypeAsString( err_type );

  // get exception's text
  if ( nullptr != err_value && err_value != Py_None )
  {
    errorText = PyObjectToQString( err_value );
  }
  else
    errorText.clear();

  // cleanup
  Py_XDECREF( err_type );
  Py_XDECREF( err_value );
  Py_XDECREF( err_tb );

  // we are done calling python API, release global interpreter lock
  PyGILState_Release( gstate );

  return true;
}


QString QgsPythonUtilsImpl::PyObjectToQString( PyObject *obj )
{
  QString result;

  // is it None?
  if ( obj == Py_None )
  {
    return QString();
  }

  // check whether the object is already a unicode string
  if ( PyUnicode_Check( obj ) )
  {
    result = QString::fromUtf8( PyUnicode_AsUTF8( obj ) );
    return result;
  }

  // if conversion to Unicode failed, try to convert it to classic string, i.e. str(obj)
  PyObject *obj_str = PyObject_Str( obj ); // new reference
  if ( obj_str )
  {
    result = QString::fromUtf8( PyUnicode_AsUTF8( obj_str ) );
    Py_XDECREF( obj_str );
    return result;
  }

  // some problem with conversion to Unicode string
  QgsDebugError( u"unable to convert PyObject to a QString!"_s );
  return u"(qgis error)"_s;
}


bool QgsPythonUtilsImpl::evalString( const QString &command, QString &result )
{
  if ( !mMainDict )
  {
    result = QString();
    return false;
  }

  // acquire global interpreter lock to ensure we are in a consistent state
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *res = PyRun_String( command.toUtf8().constData(), Py_eval_input, mMainDict, mMainDict );
  const bool success = nullptr != res;

  // TODO: error handling

  if ( success )
    result = PyObjectToQString( res );

  Py_XDECREF( res );

  // we are done calling python API, release global interpreter lock
  PyGILState_Release( gstate );

  return success;
}

QString QgsPythonUtilsImpl::pythonPath() const
{
  if ( QgsApplication::isRunningFromBuildDir() )
    return QgsApplication::buildOutputPath() + u"/python"_s;
  else
    return QgsApplication::pkgDataPath() + u"/python"_s;
}

QString QgsPythonUtilsImpl::pluginsPath() const
{
  return pythonPath() + u"/plugins"_s;
}

QString QgsPythonUtilsImpl::homePythonPath() const
{
  QString settingsDir = QgsApplication::qgisSettingsDirPath();
  if ( QDir::cleanPath( settingsDir ) == QDir::homePath() + u"/.qgis3"_s )
  {
    return u"\"%1/.qgis3/python\""_s.arg( QDir::homePath() );
  }
  else
  {
    return u"\""_s + settingsDir.replace( '\\', "\\\\"_L1 ) + u"python\""_s;
  }
}

QString QgsPythonUtilsImpl::homePluginsPath() const
{
  return homePythonPath() + u" + \"/plugins\""_s;
}

QStringList QgsPythonUtilsImpl::extraPluginsPaths() const
{
  if ( !qEnvironmentVariableIsSet( "QGIS_PLUGINPATH" ) )
    return QStringList();

  const QString paths = qEnvironmentVariable( "QGIS_PLUGINPATH" );
#ifndef Q_OS_WIN
  if ( paths.contains( ':' ) )
    return paths.split( ':', Qt::SkipEmptyParts );
#endif
  if ( paths.contains( ';' ) )
    return paths.split( ';', Qt::SkipEmptyParts );
  else
    return QStringList( paths );
}


QStringList QgsPythonUtilsImpl::pluginList()
{
  runString( u"qgis.utils.updateAvailablePlugins(sort_by_dependencies=True)"_s );

  QString output;
  evalString( u"'\\n'.join(qgis.utils.available_plugins)"_s, output );
  return output.split( QChar( '\n' ), Qt::SkipEmptyParts );
}

QString QgsPythonUtilsImpl::getPluginMetadata( const QString &pluginName, const QString &function )
{
  QString res;
  const QString str = u"qgis.utils.pluginMetadata('%1', '%2')"_s.arg( pluginName, function );
  evalString( str, res );
  //QgsDebugMsgLevel("metadata "+pluginName+" - '"+function+"' = "+res, 2);
  return res;
}

bool QgsPythonUtilsImpl::pluginHasProcessingProvider( const QString &pluginName )
{
  return getPluginMetadata( pluginName, u"hasProcessingProvider"_s ).compare( "yes"_L1, Qt::CaseInsensitive ) == 0
         || getPluginMetadata( pluginName, u"hasProcessingProvider"_s ).compare( "true"_L1, Qt::CaseInsensitive ) == 0;
}

bool QgsPythonUtilsImpl::loadPlugin( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.loadPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::startPlugin( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.startPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::startProcessingPlugin( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.startProcessingPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::finalizeProcessingStartup()
{
  QString output;
  evalString( u"qgis.utils.finalizeProcessingStartup()"_s, output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::canUninstallPlugin( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.canUninstallPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::unloadPlugin( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.unloadPlugin('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

bool QgsPythonUtilsImpl::isPluginEnabled( const QString &packageName ) const
{
  return QgsSettings().value( u"/PythonPlugins/"_s + packageName, QVariant( false ) ).toBool();
}

bool QgsPythonUtilsImpl::isPluginLoaded( const QString &packageName )
{
  QString output;
  evalString( u"qgis.utils.isPluginLoaded('%1')"_s.arg( packageName ), output );
  return ( output == "True"_L1 );
}

QStringList QgsPythonUtilsImpl::listActivePlugins()
{
  QString output;
  evalString( u"'\\n'.join(qgis.utils.active_plugins)"_s, output );
  return output.split( QChar( '\n' ), Qt::SkipEmptyParts );
}

void QgsPythonUtilsImpl::initGDAL()
{
  runString( "from osgeo import gdal, ogr, osr" );
  // To avoid FutureWarning with GDAL >= 3.7.0
  runString( "gdal.UseExceptions()" );
  runString( "ogr.UseExceptions()" );
  runString( "osr.UseExceptions()" );
}
