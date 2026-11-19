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

        def __int__(self):
            return 0

        def __float__(self):
            return 0.0

        def __add__(self, other):
            return self

        def __sub__(self, other):
            return self

        def __mul__(self, other):
            return self

        def __truediv__(self, other):
            return 1.0

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
            mock_crs = DynamicMock("QgsCoordinateReferenceSystem")
            mock_crs.authid = lambda: "EPSG:4326"
            mock_crs.description = lambda: "WGS 84"
            mock_crs.toWkt = lambda: "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]"
            return mock_crs

        def authid(self):
            return "EPSG:4326"

        def mapLayers(self):
            return {}

        def layers(self):
            return []

        def x(self):
            return 90.6244

        def y(self):
            return 24.9406

        def center(self):
            return self

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

        class MockQgsPointXY:
            def __init__(self, x=90.6244, y=24.9406, *args, **kwargs):
                self._x = float(x) if isinstance(x, (int, float)) else 90.6244
                self._y = float(y) if isinstance(y, (int, float)) else 24.9406
            def x(self): return self._x
            def y(self): return self._y
            def setX(self, v): self._x = float(v)
            def setY(self, v): self._y = float(v)
            def toString(self, precision=4): return f"{self._x:.{precision}f}, {self._y:.{precision}f}"
            def __str__(self): return f"PointXY({self._x}, {self._y})"
            def __repr__(self): return f"<QgsPointXY: {self._x}, {self._y}>"

        class MockQgsRectangle:
            def __init__(self, xmin=88.0, ymin=23.0, xmax=92.0, ymax=26.0, *args, **kwargs):
                self._xmin = 88.0
                self._ymin = 23.0
                self._xmax = 92.0
                self._ymax = 26.0
            def xMinimum(self): return self._xmin
            def xMaximum(self): return self._xmax
            def yMinimum(self): return self._ymin
            def yMaximum(self): return self._ymax
            def center(self): return MockQgsPointXY((self._xmin + self._xmax) / 2.0, (self._ymin + self._ymax) / 2.0)
            def width(self): return self._xmax - self._xmin
            def height(self): return self._ymax - self._ymin
            def isEmpty(self): return False
            def isNull(self): return False
            def asWktCoordinates(self): return f"{self._xmin} {self._ymin}, {self._xmax} {self._ymax}"
            def __str__(self): return f"Rectangle({self._xmin}, {self._ymin}, {self._xmax}, {self._ymax})"

        class MockCoordinateTransform:
            def __init__(self, *args, **kwargs):
                pass
            def transform(self, pt, *args, **kwargs):
                return pt
            def transformBoundingBox(self, bbox, *args, **kwargs):
                return bbox

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
                mock_crs.description = lambda: "WGS 84"
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

        qgis_core.QgsPointXY = MockQgsPointXY
        qgis_core.QgsPoint = MockQgsPointXY
        qgis_core.QgsRectangle = MockQgsRectangle
        qgis_core.QgsCoordinateTransform = MockCoordinateTransform
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
    registered_widgets = []

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
            idx = len(registered_actions) + len(registered_widgets) - 1
            row = idx // 2
            col = idx % 2

            btn = QtWidgets.QPushButton(action.icon(), f"  {action.text()}", self.scroll_content)
            btn.setMinimumHeight(44)
            btn.setStyleSheet(
                "QPushButton { text-align: left; padding: 8px 16px; font-size: 13px; font-weight: 500; "
                "border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; } "
                "QPushButton:hover { background-color: #f3f4f6; border-color: #0969da; color: #0969da; }"
            )
            btn.clicked.connect(action.trigger)
            self.grid_layout.addWidget(btn, row, col)

        def register_widget(self, widget):
            if not widget or widget in registered_widgets:
                return None
            registered_widgets.append(widget)

            # Add to Toolbar
            action = self.main_toolbar.addWidget(widget)

            # Add to Central View Grid
            idx = len(registered_actions) + len(registered_widgets) - 1
            row = idx // 2
            col = idx % 2
            self.grid_layout.addWidget(widget, row, col)
            widget.show()
            return action

    host_window = PluginHostWindow(package_name) if QtWidgets else None

    # 4. Mock QgisInterface connecting to host window with full API coverage
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

        def addToolBarWidget(self, widget):
            if self._main_window and hasattr(self._main_window, "register_widget"):
                act = self._main_window.register_widget(widget)
                if act:
                    return act
            if hasattr(QtGui, "QAction"):
                return QtGui.QAction(widget)
            elif QtWidgets and hasattr(QtWidgets, "QAction"):
                return QtWidgets.QAction(widget)
            return create_dynamic_mock("QAction")

        def addRasterToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addVectorToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addDatabaseToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addRasterToolBarIcon(self, action):
            return self.addToolBarIcon(action)

        def addVectorToolBarIcon(self, action):
            return self.addToolBarIcon(action)

        def addDatabaseToolBarIcon(self, action):
            return self.addToolBarIcon(action)

        def addWebToolBarIcon(self, action):
            return self.addToolBarIcon(action)

        def addCustomToolBar(self, name):
            if self._main_window:
                return self._main_window.addToolBar(name)
            return create_dynamic_mock("QToolBar")

        def addPluginToMenu(self, name, action):
            if self._main_window and hasattr(self._main_window, "register_action"):
                self._main_window.register_action(action, name)

        def removePluginMenu(self, name, action):
            pass

        def addPluginToDatabaseMenu(self, name, action):
            return self.addPluginToMenu(name, action)

        def addPluginToRasterMenu(self, name, action):
            return self.addPluginToMenu(name, action)

        def addPluginToVectorMenu(self, name, action):
            return self.addPluginToMenu(name, action)

        def addPluginToWebMenu(self, name, action):
            return self.addPluginToMenu(name, action)

        def addPluginToMeshMenu(self, name, action):
            return self.addPluginToMenu(name, action)

        def addDockWidget(self, area, dock):
            if dock and self._main_window:
                try:
                    self._main_window.addDockWidget(area, dock)
                    dock.show()
                except Exception:
                    dock.show()

        def removeDockWidget(self, dock):
            pass

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
                    return qgis_core.QgsRectangle()
                def setExtent(self, *args):
                    pass
                def refresh(self):
                    pass
                def scale(self):
                    return 100000.0
                def size(self):
                    return QtCore.QSize(800, 600) if QtCore else (800, 600)
                def mapSettings(self):
                    ms = create_dynamic_mock("QgsMapSettings")
                    ms.destinationCrs = lambda: qgis_core.QgsProject.instance().crs()
                    ms.extent = lambda: qgis_core.QgsRectangle()
                    return ms
            return StubCanvas()

        def activeLayer(self):
            return None

        def setActiveLayer(self, layer):
            pass

        def __getattr__(self, item):
            """Fallback for any other QgisInterface method: intercepts widgets/actions and never crashes."""
            def method_stub(*args, **kwargs):
                for arg in args:
                    if hasattr(arg, "trigger") and hasattr(arg, "text"):
                        self.addToolBarIcon(arg)
                    elif QtWidgets and isinstance(arg, QtWidgets.QWidget):
                        self.addToolBarWidget(arg)
                return create_dynamic_mock(f"iface.{item}")
            return method_stub

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
        if len(registered_actions) == 1 and not registered_widgets:
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
