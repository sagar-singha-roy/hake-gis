#!/usr/bin/env python3
"""
Hake GeoDesk Native Plugin Launcher
Launches and displays Python plugins with GUI dialogs / docks on macOS & Linux.
"""
import sys
import os
import types
import importlib

def main():
    if len(sys.argv) < 3:
        print("Usage: hake_plugin_launcher.py <plugin_dir> <package_name>", file=sys.stderr)
        sys.exit(1)

    plugin_dir = os.path.abspath(sys.argv[1])
    package_name = sys.argv[2]
    parent_dir = os.path.dirname(plugin_dir)

    for p in [plugin_dir, parent_dir]:
        if p not in sys.path:
            sys.path.insert(0, p)

    # Setup stub qgis modules if not available in python environment
    if "qgis" not in sys.modules:
        try:
            import qgis
        except ImportError:
            qgis = types.ModuleType("qgis")
            qgis_core = types.ModuleType("qgis.core")
            qgis_gui = types.ModuleType("qgis.gui")
            qgis_utils = types.ModuleType("qgis.utils")
            qgis_PyQt = types.ModuleType("qgis.PyQt")

            # Try PyQt6 or PyQt5 for qgis.PyQt
            try:
                from PyQt6 import QtWidgets, QtCore, QtGui
                qgis_PyQt.QtWidgets = QtWidgets
                qgis_PyQt.QtCore = QtCore
                qgis_PyQt.QtGui = QtGui
                qgis_PyQt.uic = importlib.import_module("PyQt6.uic")
            except ImportError:
                try:
                    from PyQt5 import QtWidgets, QtCore, QtGui
                    qgis_PyQt.QtWidgets = QtWidgets
                    qgis_PyQt.QtCore = QtCore
                    qgis_PyQt.QtGui = QtGui
                    qgis_PyQt.uic = importlib.import_module("PyQt5.uic")
                except ImportError:
                    pass

            qgis.core = qgis_core
            qgis.gui = qgis_gui
            qgis.utils = qgis_utils
            qgis.PyQt = qgis_PyQt

            sys.modules["qgis"] = qgis
            sys.modules["qgis.core"] = qgis_core
            sys.modules["qgis.gui"] = qgis_gui
            sys.modules["qgis.utils"] = qgis_utils
            sys.modules["qgis.PyQt"] = qgis_PyQt
            sys.modules["qgis.PyQt.QtWidgets"] = getattr(qgis_PyQt, "QtWidgets", None)
            sys.modules["qgis.PyQt.QtCore"] = getattr(qgis_PyQt, "QtCore", None)
            sys.modules["qgis.PyQt.QtGui"] = getattr(qgis_PyQt, "QtGui", None)

    # Try Qt
    app = None
    try:
        from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget
        app = QApplication.instance() or QApplication([f"Hake GeoDesk - {package_name}"])
    except ImportError:
        try:
            from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget
            app = QApplication.instance() or QApplication([f"Hake GeoDesk - {package_name}"])
        except ImportError:
            pass

    # Mock QgisInterface
    class StubIface:
        def __init__(self):
            self._main_window = None
            if app:
                try:
                    from PyQt6.QtWidgets import QMainWindow
                    self._main_window = QMainWindow()
                except ImportError:
                    try:
                        from PyQt5.QtWidgets import QMainWindow
                        self._main_window = QMainWindow()
                    except ImportError:
                        pass

        def mainWindow(self):
            return self._main_window

        def addToolBarIcon(self, action):
            pass

        def removeToolBarIcon(self, action):
            pass

        def addPluginToMenu(self, name, action):
            pass

        def removePluginMenu(self, name, action):
            pass

        def addDockWidget(self, area, dock):
            if dock:
                dock.show()

        def messageBar(self):
            class StubMsgBar:
                def pushMessage(self, *args, **kwargs):
                    pass
                def pushInfo(self, *args, **kwargs):
                    pass
                def pushSuccess(self, *args, **kwargs):
                    pass
                def pushWarning(self, *args, **kwargs):
                    pass
            return StubMsgBar()

        def mapCanvas(self):
            class StubCanvas:
                def extent(self):
                    return None
                def setExtent(self, *args):
                    pass
                def refresh(self):
                    pass
            return StubCanvas()

    iface = StubIface()

    # Load the plugin
    try:
        mod = importlib.import_module(package_name)
    except Exception as e:
        print(f"Error loading plugin {package_name}: {e}", file=sys.stderr)
        return 1

    if hasattr(mod, "classFactory"):
        try:
            instance = mod.classFactory(iface)
            if hasattr(instance, "initGui"):
                instance.initGui()
            if hasattr(instance, "run"):
                instance.run()
        except Exception as e:
            print(f"Error running plugin {package_name}: {e}", file=sys.stderr)

    if app:
        try:
            from PyQt6.QtWidgets import QApplication
            top_levels = [w for w in QApplication.topLevelWidgets() if w.isVisible()]
            if top_levels:
                for w in top_levels:
                    w.raise_()
                    w.activateWindow()
                app.exec()
        except Exception:
            pass

    return 0

if __name__ == "__main__":
    sys.exit(main())

