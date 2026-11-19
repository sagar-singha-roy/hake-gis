#!/usr/bin/env python3
"""
Hake GeoDesk Native Plugin Launcher
Launches and displays Python plugins with GUI dialogs / docks on macOS & Linux.
Provides dynamic QGIS compatibility stubs so QGIS 3.x/4.x plugins run smoothly.
"""
import sys
import os
import types
import importlib
import traceback

def create_dynamic_mock(name="Mock"):
    """Creates a callable, iterable, attribute-accepting mock object."""
    default_name = name
    class DynamicMock:
        def __init__(self, obj_name=None, *args, **kwargs):
            self._obj_name = default_name if obj_name is None else obj_name

        def __call__(self, *args, **kwargs):
            return DynamicMock(self._obj_name)

        def __getattr__(self, item):
            if item.startswith("__") and item.endswith("__"):
                raise AttributeError(item)
            return DynamicMock(f"{self._obj_name}.{item}")

        def __setattr__(self, key, value):
            if key.startswith("_"):
                super().__setattr__(key, value)
            else:
                self.__dict__[key] = value

        def __bool__(self):
            return True

        def __iter__(self):
            return iter([])

        def __len__(self):
            return 0

        def __str__(self):
            return self._obj_name

        def __repr__(self):
            return f"<QgsMock {self._obj_name}>"

        def isValid(self):
            return True

        def name(self):
            return self._obj_name

        def id(self):
            return self._obj_name

        def extent(self):
            return DynamicMock("QgsRectangle")

        def crs(self):
            return DynamicMock("QgsCoordinateReferenceSystem")

        def authid(self):
            return "EPSG:4326"

        def mapLayers(self):
            return {}

        def layers(self):
            return []

    return DynamicMock()

class DynamicModule(types.ModuleType):
    """Dynamic module that returns stubs for missing attributes instead of raising ImportError."""
    def __init__(self, name):
        super().__init__(name)

    def __getattr__(self, item):
        if item.startswith("__") and item.endswith("__"):
            raise AttributeError(item)
        stub = create_dynamic_mock(f"{self.__name__}.{item}")
        setattr(self, item, stub)
        return stub

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

    # 1. Setup Qt framework
    app = None
    QtWidgets = None
    QtCore = None
    QtGui = None
    uic = None

    try:
        from PyQt6 import QtWidgets as _w, QtCore as _c, QtGui as _g
        QtWidgets = _w
        QtCore = _c
        QtGui = _g
        try:
            import PyQt6.uic as _uic
            uic = _uic
        except Exception:
            pass
    except ImportError:
        try:
            from PyQt5 import QtWidgets as _w, QtCore as _c, QtGui as _g
            QtWidgets = _w
            QtCore = _c
            QtGui = _g
            try:
                import PyQt5.uic as _uic
                uic = _uic
            except Exception:
                pass
        except ImportError:
            pass

    if QtWidgets:
        app = QtWidgets.QApplication.instance()
        if not app:
            app = QtWidgets.QApplication([f"Hake GeoDesk — {package_name}"])

    # 2. Setup dynamic QGIS compatibility layer
    if "qgis" not in sys.modules:
        qgis = DynamicModule("qgis")
        qgis_core = DynamicModule("qgis.core")
        qgis_gui = DynamicModule("qgis.gui")
        qgis_utils = DynamicModule("qgis.utils")
        qgis_PyQt = DynamicModule("qgis.PyQt")

        # Map Qt modules into qgis.PyQt
        if QtWidgets and QtCore and QtGui:
            qgis_PyQt.QtWidgets = QtWidgets
            qgis_PyQt.QtCore = QtCore
            qgis_PyQt.QtGui = QtGui
            if uic:
                qgis_PyQt.uic = uic
            # Critical: many QGIS plugins import QAction from QtWidgets even in PyQt6
            if hasattr(QtGui, "QAction") and not hasattr(QtWidgets, "QAction"):
                QtWidgets.QAction = QtGui.QAction

        # Provide common QGIS Core classes
        class MockQgsProject:
            _instance = None
            def __init__(self):
                self._layers = {}
                self._title = "Hake GeoDesk Project"
            @classmethod
            def instance(cls):
                if cls._instance is None:
                    cls._instance = MockQgsProject()
                return cls._instance
            def mapLayers(self):
                return self._layers
            def layers(self):
                return list(self._layers.values())
            def addMapLayer(self, layer, addToLegend=True):
                if layer:
                    lid = getattr(layer, "id", lambda: str(id(layer)))()
                    self._layers[lid] = layer
                return layer
            def removeMapLayer(self, lid):
                self._layers.pop(lid, None)
            def crs(self):
                mock_crs = create_dynamic_mock("QgsCoordinateReferenceSystem")
                mock_crs.authid = lambda: "EPSG:4326"
                return mock_crs
            def fileName(self):
                return ""
            def homePath(self):
                return os.path.expanduser("~")

        class MockQgsMessageLog:
            @staticmethod
            def logMessage(message, tag="QGIS", level=0, *args, **kwargs):
                print(f"[{tag}] {message}")

        class MockQgis:
            class MessageLevel:
                Info = 0
                Warning = 1
                Critical = 2
                Success = 3
            Info = 0
            Warning = 1
            Critical = 2
            Success = 3

        qgis_core.QgsProject = MockQgsProject
        qgis_core.QgsMessageLog = MockQgsMessageLog
        qgis_core.Qgis = MockQgis
        qgis_core.QgsCoordinateReferenceSystem = lambda auth="EPSG:4326": create_dynamic_mock("QgsCRS")

        qgis.core = qgis_core
        qgis.gui = qgis_gui
        qgis.utils = qgis_utils
        qgis.PyQt = qgis_PyQt

        sys.modules["qgis"] = qgis
        sys.modules["qgis.core"] = qgis_core
        sys.modules["qgis.gui"] = qgis_gui
        sys.modules["qgis.utils"] = qgis_utils
        sys.modules["qgis.PyQt"] = qgis_PyQt
        sys.modules["qgis.PyQt.QtWidgets"] = QtWidgets
        sys.modules["qgis.PyQt.QtCore"] = QtCore
        sys.modules["qgis.PyQt.QtGui"] = QtGui

    # 3. Build interactive plugin host window
    registered_actions = []

    class PluginHostWindow(QtWidgets.QMainWindow if QtWidgets else object):
        def __init__(self, title):
            super().__init__()
            self.setWindowTitle(f"Hake GeoDesk — {title}")
            self.resize(720, 480)

            # Center on screen
            if app:
                screen = app.primaryScreen()
                if screen:
                    geo = screen.availableGeometry()
                    self.move((geo.width() - self.width()) // 2, (geo.height() - self.height()) // 2)

            self.main_menu_bar = self.menuBar()
            self.main_toolbar = self.addToolBar(title)
            self.main_toolbar.setMovable(False)

            # Central UI
            central = QtWidgets.QWidget(self)
            self.setCentralWidget(central)
            layout = QtWidgets.QVBoxLayout(central)
            layout.setContentsMargins(24, 20, 24, 20)
            layout.setSpacing(16)

            # Header
            header = QtWidgets.QLabel(f"<h2>{title}</h2><p style='color:#555;'>Plugin loaded successfully in Hake GeoDesk.</p>", central)
            layout.addWidget(header)

            # Action buttons host
            self.btn_scroll = QtWidgets.QScrollArea(central)
            self.btn_scroll.setWidgetResizable(True)
            self.btn_scroll.setFrameShape(QtWidgets.QFrame.Shape.NoFrame if hasattr(QtWidgets, "QFrame") else 0)
            self.scroll_content = QtWidgets.QWidget(self.btn_scroll)
            self.grid_layout = QtWidgets.QGridLayout(self.scroll_content)
            self.grid_layout.setContentsMargins(4, 4, 4, 4)
            self.grid_layout.setSpacing(12)
            self.btn_scroll.setWidget(self.scroll_content)
            layout.addWidget(self.btn_scroll, 1)

            # Status bar
            self.statusBar().showMessage(f"Ready • {title}")

        def register_action(self, action, menu_name=None):
            if not action or action in registered_actions:
                return
            registered_actions.append(action)

            # Add to Toolbar
            self.main_toolbar.addAction(action)

            # Add to Menu
            menu_title = menu_name if menu_name else "Tools"
            target_menu = None
            for action_menu in self.main_menu_bar.actions():
                if action_menu.text() == menu_title:
                    target_menu = action_menu.menu()
                    break
            if not target_menu:
                target_menu = self.main_menu_bar.addMenu(menu_title)
            target_menu.addAction(action)

            # Add to Central View Grid
            row = (len(registered_actions) - 1) // 2
            col = (len(registered_actions) - 1) % 2

            btn = QtWidgets.QPushButton(action.icon(), f"  {action.text()}", self.scroll_content)
            btn.setMinimumHeight(44)
            btn.setStyleSheet(
                "QPushButton { text-align: left; padding: 8px 16px; font-size: 13px; font-weight: 500; "
                "border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; } "
                "QPushButton:hover { background-color: #f3f4f6; border-color: #0969da; color: #0969da; }"
            )
            btn.clicked.connect(action.trigger)
            self.grid_layout.addWidget(btn, row, col)

    host_window = PluginHostWindow(package_name) if QtWidgets else None

    # 4. Mock QgisInterface connecting to host window
    class StubIface:
        def __init__(self, win):
            self._main_window = win

        def mainWindow(self):
            return self._main_window

        def addToolBarIcon(self, action):
            if self._main_window and hasattr(self._main_window, "register_action"):
                self._main_window.register_action(action, "Tools")

        def removeToolBarIcon(self, action):
            pass

        def addPluginToMenu(self, name, action):
            if self._main_window and hasattr(self._main_window, "register_action"):
                self._main_window.register_action(action, name)

        def removePluginMenu(self, name, action):
            pass

        def addDockWidget(self, area, dock):
            if dock and self._main_window:
                try:
                    self._main_window.addDockWidget(area, dock)
                    dock.show()
                except Exception:
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
                    return create_dynamic_mock("QgsRectangle")
                def setExtent(self, *args):
                    pass
                def refresh(self):
                    pass
                def scale(self):
                    return 100000.0
                def mapSettings(self):
                    return create_dynamic_mock("QgsMapSettings")
            return StubCanvas()

    iface = StubIface(host_window)
    if "qgis.utils" in sys.modules:
        sys.modules["qgis.utils"].iface = iface

    # 5. Load and initialize the plugin
    try:
        mod = importlib.import_module(package_name)
    except Exception as e:
        err_msg = f"Failed to import plugin '{package_name}':\n\n{traceback.format_exc()}"
        print(err_msg, file=sys.stderr)
        if QtWidgets and app:
            QtWidgets.QMessageBox.critical(None, f"Plugin Error — {package_name}", err_msg)
        return 1

    instance = None
    if hasattr(mod, "classFactory"):
        try:
            instance = mod.classFactory(iface)
            if hasattr(instance, "initGui"):
                instance.initGui()
        except Exception as e:
            err_msg = f"Error during {package_name}.initGui():\n\n{traceback.format_exc()}"
            print(err_msg, file=sys.stderr)
            if QtWidgets and app:
                QtWidgets.QMessageBox.critical(None, f"Plugin Error — {package_name}", err_msg)

    # 6. Show the host window and launch action
    if host_window:
        host_window.show()
        host_window.raise_()
        host_window.activateWindow()

        # If the plugin only registers 1 single action (or has a run method), trigger it automatically
        if len(registered_actions) == 1:
            try:
                registered_actions[0].trigger()
            except Exception as e:
                print(f"Error triggering primary action: {e}", file=sys.stderr)
        elif hasattr(instance, "run") and callable(getattr(instance, "run")):
            try:
                instance.run()
            except Exception as e:
                print(f"Error running instance.run(): {e}", file=sys.stderr)

    if app:
        try:
            app.exec()
        except Exception:
            pass

    return 0

if __name__ == "__main__":
    sys.exit(main())
