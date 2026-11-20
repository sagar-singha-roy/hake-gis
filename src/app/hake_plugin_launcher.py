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
import builtins

# 0. Provide requests fallback if not installed globally
try:
    import requests
except ImportError:
    try:
        import pip._vendor.requests as _vendored_requests
        sys.modules["requests"] = _vendored_requests
    except ImportError:
        pass

def create_dynamic_mock(name="Mock"):
    """
    Creates a dynamic class/type that acts as both a valid Python type (for pyqtSignal,
    isinstance, subclassing) and an instance factory for QGIS compatibility.
    """
    default_name = name

    class DynamicMeta(type):
        def __getattr__(cls, item):
            if item.startswith("__") and item.endswith("__"):
                raise AttributeError(item)
            return create_dynamic_mock(f"{cls.__name__}.{item}")

        def __str__(cls):
            return cls.__name__

        def __repr__(cls):
            return f"<QgsMockClass {cls.__name__}>"

        def __bool__(cls):
            return True

        def __iter__(cls):
            return iter([create_dynamic_mock("Item1"), create_dynamic_mock("Item2")])

        def __enter__(cls):
            return cls

        def __exit__(cls, *args):
            return False

        def __call__(cls, *args, **kwargs):
            return super().__call__(*args, **kwargs)

    class DynamicMock(metaclass=DynamicMeta):
        def __init__(self, *args, **kwargs):
            if len(args) >= 2 and isinstance(args[1], str) and args[1]:
                self._obj_name = args[1]
            elif len(args) >= 1 and isinstance(args[0], str) and args[0]:
                self._obj_name = args[0]
            else:
                self._obj_name = default_name
            self._args = args
            self._kwargs = kwargs

        def __call__(self, *args, **kwargs):
            return self

        def __getattr__(self, item):
            if item.startswith("__") and item.endswith("__"):
                raise AttributeError(item)
            return create_dynamic_mock(f"{self._obj_name}.{item}")

        def __setattr__(self, key, value):
            if key.startswith("_"):
                super().__setattr__(key, value)
            else:
                self.__dict__[key] = value

        def __bool__(self):
            return True

        def __iter__(self):
            return iter([create_dynamic_mock("Item1"), create_dynamic_mock("Item2")])

        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

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
            return str(self._obj_name)

        def __repr__(self):
            return f"<{self._obj_name} instance>"

        def isValid(self):
            return True

        def name(self):
            return str(self._obj_name)

        def id(self):
            return f"{self._obj_name}_{id(self)}"

        def source(self):
            return self._args[0] if (hasattr(self, "_args") and len(self._args) > 0 and isinstance(self._args[0], str)) else ""

        def extent(self):
            return create_dynamic_mock("QgsRectangle")()

        def crs(self):
            mock_crs = create_dynamic_mock("QgsCoordinateReferenceSystem")()
            mock_crs.authid = lambda: "EPSG:4326"
            mock_crs.description = lambda: "WGS 84"
            mock_crs.toWkt = lambda: 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'
            return mock_crs

        def authid(self):
            return "EPSG:4326"

        def dataProvider(self):
            mock_prov = create_dynamic_mock("QgsDataProvider")()
            mock_prov.crs = self.crs
            mock_prov.extent = self.extent
            mock_prov.name = lambda: "wms"
            return mock_prov

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

        def fields(self):
            return create_dynamic_mock("QgsFields")()

        def geometry(self):
            return create_dynamic_mock("QgsGeometry")()

        def type(self):
            return 1

        def trigger(self, *args, **kwargs):
            pass

    DynamicMock.__name__ = name
    return DynamicMock

class DynamicModule(types.ModuleType):
    """Dynamic module that returns stubs for missing attributes instead of raising ImportError."""
    def __init__(self, name, is_package=False):
        super().__init__(name)
        if is_package:
            self.__path__ = []

    def __getattr__(self, item):
        if item == "__all__":
            if hasattr(self, "_dynamic_all"):
                return self._dynamic_all
            raise AttributeError("__all__")
        if item.startswith("__") and item.endswith("__"):
            raise AttributeError(item)
        stub = create_dynamic_mock(f"{self.__name__}.{item}")
        setattr(self, item, stub)
        return stub

class DynamicFallbackFinder:
    """Fallback finder for sys.meta_path to gracefully resolve optional plugin dependencies."""
    @classmethod
    def find_spec(cls, fullname, path, target=None):
        if fullname.startswith("PyQt") or fullname.startswith("qgis") or fullname in sys.modules:
            return None
        from importlib.machinery import ModuleSpec
        return ModuleSpec(fullname, cls, is_package=True)

    @classmethod
    def create_module(cls, spec):
        mod = DynamicModule(spec.name, is_package=True)
        return mod

    @classmethod
    def exec_module(cls, mod):
        pass

sys.meta_path.append(DynamicFallbackFinder)

# Comprehensive list of standard QGIS symbols to expose in qgis.core, qgis.gui, and builtins
QGIS_CORE_CLASSES = [
    "QgsRasterLayer", "QgsVectorLayer", "QgsMapLayer", "QgsPluginLayer", "QgsMeshLayer",
    "QgsVectorTileLayer", "QgsPointCloudLayer", "QgsAnnotationLayer",
    "QgsProject", "QgsPointXY", "QgsPoint", "QgsRectangle", "QgsBox3D",
    "QgsCoordinateReferenceSystem", "QgsCoordinateTransform", "QgsCoordinateTransformContext",
    "QgsFeature", "QgsGeometry", "QgsField", "QgsFields", "QgsAttributes", "QgsFeatureRequest",
    "QgsWkbTypes", "QgsProperty", "QgsPropertyCollection",
    "QgsSymbol", "QgsMarkerSymbol", "QgsLineSymbol", "QgsFillSymbol",
    "QgsExpression", "QgsExpressionContext", "QgsExpressionContextUtils",
    "QgsDistanceArea", "QgsUnitTypes", "QgsDataSourceUri",
    "QgsMessageLog", "QgsApplication", "QgsSettings", "Qgis",
    "QgsTask", "QgsTaskManager", "QgsNetworkAccessManager", "QgsNetworkContentFetcher",
    "QgsProcessing", "QgsProcessingAlgorithm", "QgsProcessingParameterDefinition",
    "QgsProcessingFeedback", "QgsProcessingContext", "QgsProcessingProvider",
    "QgsRasterDataProvider", "QgsVectorDataProvider", "QgsDataProvider",
    "QgsLayerTreeGroup", "QgsLayerTreeLayer", "QgsLayerTreeNode", "QgsLayerTreeModel",
    "QgsMapSettings", "QgsRenderContext",
    "QgsSpatialIndex", "QgsSpatialIndexKDBush",
    "QgsVectorFileWriter", "QgsVectorLayerExporter",
    "QgsRasterFileWriter", "QgsRasterPipe",
    "QgsColorRamp", "QgsColorRampShader",
    "QgsClassificationMethod", "QgsPalLabeling"
]

QGIS_GUI_CLASSES = [
    "QgsMapCanvas", "QgsMessageBar", "QgsFilterLineEdit", "QgsCollapsibleGroupBox",
    "QgsColorButton", "QgsDockWidget", "QgsMapTool", "QgsMapToolPan", "QgsMapToolZoom",
    "QgsMapToolEmitPoint", "QgsMapMouseEvent", "QgsRubberBand", "QgsVertexMarker",
    "QgsHighlight", "QgsExtentGroupBox", "QgsProjectionSelectionWidget",
    "QgsFieldComboBox", "QgsMapLayerComboBox", "QgsLayerTreeView"
]

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
        # Patch PyQt6 QAction and QActionGroup onto QtWidgets for Qt5/Qt6 plugin compatibility
        if hasattr(QtGui, "QAction") and not hasattr(QtWidgets, "QAction"):
            QtWidgets.QAction = QtGui.QAction
        if hasattr(QtGui, "QActionGroup") and not hasattr(QtWidgets, "QActionGroup"):
            QtWidgets.QActionGroup = QtGui.QActionGroup
        if hasattr(QtGui, "QShortcut") and not hasattr(QtWidgets, "QShortcut"):
            QtWidgets.QShortcut = QtGui.QShortcut

        app = QtWidgets.QApplication.instance()
        if not app:
            app = QtWidgets.QApplication([f"Hake GeoDesk — {package_name}"])

    # Install custom excepthook to gracefully capture exceptions in Qt slots without qAbort()
    def plugin_excepthook(exc_type, exc_val, exc_tb):
        if issubclass(exc_type, KeyboardInterrupt):
            sys.__excepthook__(exc_type, exc_val, exc_tb)
            return
        formatted = "".join(traceback.format_exception(exc_type, exc_val, exc_tb))
        print(f"[Hake GeoDesk Plugin Warning] Caught exception:\n{formatted}", file=sys.stderr)
        if QtWidgets and app:
            try:
                QtWidgets.QMessageBox.warning(
                    None,
                    "Hake GeoDesk — Plugin Notification",
                    f"Notice while running {package_name}:\n\n{exc_val}\n\nThe application continues operating normally."
                )
            except Exception:
                pass

    sys.excepthook = plugin_excepthook

    # 2. Setup dynamic QGIS compatibility layer
    if "qgis" not in sys.modules:
        qgis = DynamicModule("qgis", is_package=True)
        qgis_core = DynamicModule("qgis.core", is_package=True)
        qgis_gui = DynamicModule("qgis.gui", is_package=True)
        qgis_utils = DynamicModule("qgis.utils")
        qgis_PyQt = DynamicModule("qgis.PyQt", is_package=True)

        # Map Qt modules into qgis.PyQt
        if QtWidgets and QtCore and QtGui:
            qgis_PyQt.QtWidgets = QtWidgets
            qgis_PyQt.QtCore = QtCore
            qgis_PyQt.QtGui = QtGui
            if uic:
                qgis_PyQt.uic = uic

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
            def __init__(self, *args, **kwargs): pass
            def transform(self, pt, *args, **kwargs): return pt
            def transformBoundingBox(self, bbox, *args, **kwargs): return bbox

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
            def mapLayers(self): return self._layers
            def layers(self): return list(self._layers.values())
            def addMapLayer(self, layer, addToLegend=True):
                if layer:
                    lid = getattr(layer, "id", lambda: str(id(layer)))()
                    self._layers[lid] = layer
                return layer
            def removeMapLayer(self, lid):
                self._layers.pop(lid, None)
            def crs(self):
                mock_crs = create_dynamic_mock("QgsCoordinateReferenceSystem")()
                mock_crs.authid = lambda: "EPSG:4326"
                mock_crs.description = lambda: "WGS 84"
                return mock_crs
            def fileName(self): return ""
            def homePath(self): return os.path.expanduser("~")

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

        class MockQgsApplication:
            @staticmethod
            def qgisSettingsDirPath():
                return os.path.expanduser("~/.local/share/hake-geodesk/profiles/default/")
            @staticmethod
            def pkgDataPath():
                return os.path.expanduser("~/.local/share/hake-geodesk/")
            @staticmethod
            def prefixPath():
                return "/usr"
            @staticmethod
            def iconPath(name=""):
                return ""
            @staticmethod
            def getThemeIcon(name=""):
                return QtGui.QIcon() if QtGui else create_dynamic_mock("QIcon")()

        # GUI Widget classes
        class MockQgsMapCanvas(QtWidgets.QGraphicsView if QtWidgets else object):
            def __init__(self, parent=None, *args, **kwargs):
                if QtWidgets:
                    super().__init__(parent)
            def setCanvasColor(self, color): pass
            def enableAntiAliasing(self, flag): pass
            def setWheelFactor(self, factor): pass
            def setDirty(self, flag): pass
            def refresh(self): pass
            def extent(self): return MockQgsRectangle()
            def setExtent(self, *args): pass
            def scale(self): return 100000.0
            def size(self): return QtCore.QSize(800, 600) if QtCore else (800, 600)
            def mapSettings(self): return create_dynamic_mock("QgsMapSettings")()
            def scene(self):
                return QtWidgets.QGraphicsScene(self) if QtWidgets else create_dynamic_mock("QGraphicsScene")()
            def setMapTool(self, *args): pass
            def unsetMapTool(self, *args): pass
            def mapTool(self): return create_dynamic_mock("QgsMapTool")()
            def currentLayer(self): return None
            def setCurrentLayer(self, layer): pass
            def __getattr__(self, item): return create_dynamic_mock(f"QgsMapCanvas.{item}")

        class MockQgsMessageBar(QtWidgets.QFrame if QtWidgets else object):
            def __init__(self, parent=None, *args, **kwargs):
                if QtWidgets:
                    super().__init__(parent)
            def pushMessage(self, *args, **kwargs): pass
            def pushInfo(self, *args, **kwargs): pass
            def pushSuccess(self, *args, **kwargs): pass
            def pushWarning(self, *args, **kwargs): pass
            def pushCritical(self, *args, **kwargs): pass
            def pushWidget(self, *args, **kwargs): pass
            def popWidget(self, *args, **kwargs): pass

        class MockOverrideCursor:
            def __init__(self, *args, **kwargs): pass
            def __enter__(self): return self
            def __exit__(self, *args): return False

        qgis_core.QgsPointXY = MockQgsPointXY
        qgis_core.QgsPoint = MockQgsPointXY
        qgis_core.QgsRectangle = MockQgsRectangle
        qgis_core.QgsCoordinateTransform = MockCoordinateTransform
        qgis_core.QgsProject = MockQgsProject
        qgis_core.QgsMessageLog = MockQgsMessageLog
        qgis_core.Qgis = MockQgis
        qgis_core.QgsApplication = MockQgsApplication
        qgis_core.QgsSettings = (QtCore.QSettings if QtCore else create_dynamic_mock("QgsSettings"))
        qgis_core.QgsCoordinateReferenceSystem = lambda auth="EPSG:4326": create_dynamic_mock("QgsCRS")

        qgis_gui.QgsMapCanvas = MockQgsMapCanvas
        qgis_gui.QgsMessageBar = MockQgsMessageBar
        if QtWidgets:
            qgis_gui.QgsFilterLineEdit = QtWidgets.QLineEdit
            qgis_gui.QgsCollapsibleGroupBox = QtWidgets.QGroupBox
            qgis_gui.QgsColorButton = QtWidgets.QPushButton
            qgis_gui.QgsDockWidget = QtWidgets.QDockWidget

        qgis_utils.OverrideCursor = MockOverrideCursor

        # Pre-populate all standard QGIS symbols into core/gui modules and builtins
        for cls_name in QGIS_CORE_CLASSES:
            if not hasattr(qgis_core, cls_name):
                cls_obj = create_dynamic_mock(cls_name)
                setattr(qgis_core, cls_name, cls_obj)
            setattr(builtins, cls_name, getattr(qgis_core, cls_name))
        qgis_core._dynamic_all = list(QGIS_CORE_CLASSES)
        qgis_core.__all__ = list(QGIS_CORE_CLASSES)

        for cls_name in QGIS_GUI_CLASSES:
            if not hasattr(qgis_gui, cls_name):
                cls_obj = create_dynamic_mock(cls_name)
                setattr(qgis_gui, cls_name, cls_obj)
            setattr(builtins, cls_name, getattr(qgis_gui, cls_name))
        qgis_gui._dynamic_all = list(QGIS_GUI_CLASSES)
        qgis_gui.__all__ = list(QGIS_GUI_CLASSES)

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
        if uic:
            sys.modules["qgis.PyQt.uic"] = uic

    # Setup processing mock module
    if "processing" not in sys.modules:
        proc = DynamicModule("processing")
        proc.run = lambda *args, **kwargs: {}
        proc.runAlgorithm = lambda *args, **kwargs: {}
        proc.execAlgorithmDialog = lambda *args, **kwargs: None
        proc.createAlgorithmDialog = lambda *args, **kwargs: None
        sys.modules["processing"] = proc

    # 3. Build interactive plugin host window
    registered_actions = []
    registered_widgets = []

    def safe_action_trigger(act):
        try:
            act.trigger()
        except Exception:
            plugin_excepthook(*sys.exc_info())

    def safe_widget_click(w):
        try:
            if hasattr(w, "click") and callable(getattr(w, "click")):
                w.click()
            elif hasattr(w, "trigger") and callable(getattr(w, "trigger")):
                w.trigger()
        except Exception:
            plugin_excepthook(*sys.exc_info())

    class PluginHostWindow(QtWidgets.QMainWindow if QtWidgets else object):
        def __init__(self, title):
            super().__init__()
            self.setWindowTitle(f"Hake GeoDesk — {title}")
            self.resize(760, 520)

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

            action_text = action.text().replace("&", "")
            btn = QtWidgets.QPushButton(action.icon(), f"  {action_text}", self.scroll_content)
            btn.setMinimumHeight(44)
            btn.setStyleSheet(
                "QPushButton { text-align: left; padding: 8px 16px; font-size: 13px; font-weight: 500; "
                "border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; } "
                "QPushButton:hover { background-color: #f3f4f6; border-color: #0969da; color: #0969da; }"
            )
            btn.clicked.connect(lambda checked=False, a=action: safe_action_trigger(a))
            self.grid_layout.addWidget(btn, row, col)

        def register_widget(self, widget):
            if not widget or widget in registered_widgets:
                return None
            registered_widgets.append(widget)

            # Add to Toolbar
            action = self.main_toolbar.addWidget(widget)

            # Extract child actions from menus/toolbars attached to this widget (e.g., QToolButton with QMenu)
            extracted_actions = []
            if hasattr(widget, "menu") and callable(getattr(widget, "menu")):
                w_menu = widget.menu()
                if w_menu and hasattr(w_menu, "actions"):
                    extracted_actions = [a for a in w_menu.actions() if hasattr(a, "text") and a.text()]
            elif hasattr(widget, "actions") and callable(getattr(widget, "actions")):
                extracted_actions = [a for a in widget.actions() if hasattr(a, "text") and a.text()]

            for act in extracted_actions:
                self.register_action(act, "Tools")

            # If no child actions were found, create a dedicated launcher button in the central view
            if not extracted_actions:
                idx = len(registered_actions) + len(registered_widgets) - 1
                row = idx // 2
                col = idx % 2
                label_text = widget.toolTip() if hasattr(widget, "toolTip") and widget.toolTip() else (widget.text() if hasattr(widget, "text") and widget.text() else "Execute Tool")
                icon = widget.icon() if (hasattr(widget, "icon") and callable(getattr(widget, "icon")) and not widget.icon().isNull()) else QtGui.QIcon()
                btn = QtWidgets.QPushButton(icon, f"  {label_text}", self.scroll_content)
                btn.setMinimumHeight(44)
                btn.setStyleSheet(
                    "QPushButton { text-align: left; padding: 8px 16px; font-size: 13px; font-weight: 500; "
                    "border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; } "
                    "QPushButton:hover { background-color: #f3f4f6; border-color: #0969da; color: #0969da; }"
                )
                btn.clicked.connect(lambda checked=False, w=widget: safe_widget_click(w))
                self.grid_layout.addWidget(btn, row, col)

            widget.show()
            return action

        def populate_menu_actions(self):
            """Extracts top-level and submenu actions from menuBar into the central view."""
            def _extract_submenu(menu, prefix):
                for act in menu.actions():
                    sub = act.menu()
                    if sub:
                        _extract_submenu(sub, f"{prefix} • {act.text().replace('&', '')}")
                    elif act.text() and not act.isSeparator():
                        self.register_action(act, prefix)

            for act in self.main_menu_bar.actions():
                menu = act.menu()
                if menu:
                    _extract_submenu(menu, act.text().replace("&", ""))

    host_window = PluginHostWindow(package_name) if QtWidgets else None

    # 4. Mock QgisInterface connecting to host window with full API coverage
    class StubIface:
        def __init__(self, win):
            self._main_window = win
            self._canvas = MockQgsMapCanvas() if QtWidgets else create_dynamic_mock("QgsMapCanvas")()
            self._msg_bar = MockQgsMessageBar() if QtWidgets else create_dynamic_mock("QgsMessageBar")()

        def mainWindow(self):
            return self._main_window

        def iconSize(self, *args):
            return QtCore.QSize(24, 24) if QtCore else (24, 24)

        def firstRightStandardMenu(self):
            act = QtGui.QAction("Help", self._main_window) if (QtGui and self._main_window) else None
            class StubMenu:
                def menuAction(self):
                    return act
            return StubMenu()

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
            return create_dynamic_mock("QAction")()

        def addRasterToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addVectorToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addDatabaseToolBarWidget(self, widget):
            return self.addToolBarWidget(widget)

        def addWebToolBarWidget(self, widget):
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
            return create_dynamic_mock("QToolBar")()

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
            return self._msg_bar

        def mapCanvas(self):
            return self._canvas

        def activeLayer(self):
            return None

        def setActiveLayer(self, layer):
            pass

        def layerTreeView(self):
            return QtWidgets.QTreeView() if QtWidgets else create_dynamic_mock("QgsLayerTreeView")()

        def legendInterface(self):
            return create_dynamic_mock("QgsLegendInterface")()

        def pluginMenu(self):
            return self._main_window.menuBar().addMenu("Plugins") if self._main_window else create_dynamic_mock("QMenu")()

        def rasterMenu(self):
            return self._main_window.menuBar().addMenu("Raster") if self._main_window else create_dynamic_mock("QMenu")()

        def vectorMenu(self):
            return self._main_window.menuBar().addMenu("Vector") if self._main_window else create_dynamic_mock("QMenu")()

        def databaseMenu(self):
            return self._main_window.menuBar().addMenu("Database") if self._main_window else create_dynamic_mock("QMenu")()

        def webMenu(self):
            return self._main_window.menuBar().addMenu("Web") if self._main_window else create_dynamic_mock("QMenu")()

        def viewMenu(self):
            return self._main_window.menuBar().addMenu("View") if self._main_window else create_dynamic_mock("QMenu")()

        def editMenu(self):
            return self._main_window.menuBar().addMenu("Edit") if self._main_window else create_dynamic_mock("QMenu")()

        def fileMenu(self):
            return self._main_window.menuBar().addMenu("File") if self._main_window else create_dynamic_mock("QMenu")()

        def helpMenu(self):
            return self._main_window.menuBar().addMenu("Help") if self._main_window else create_dynamic_mock("QMenu")()

        def __getattr__(self, item):
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
        sys.modules["qgis.utils"].plugins = {}
        sys.modules["qgis.utils"].active_plugins = [package_name]
        sys.modules["qgis.utils"].isPluginLoaded = lambda pkg: pkg in sys.modules["qgis.utils"].plugins

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
            if "qgis.utils" in sys.modules and instance:
                sys.modules["qgis.utils"].plugins[package_name] = instance
            if hasattr(instance, "initGui"):
                instance.initGui()
        except Exception as e:
            err_msg = f"Error during {package_name}.initGui():\n\n{traceback.format_exc()}"
            print(err_msg, file=sys.stderr)
            if QtWidgets and app:
                QtWidgets.QMessageBox.critical(None, f"Plugin Error — {package_name}", err_msg)

    # 6. Populate menus and show the host window
    if host_window:
        host_window.populate_menu_actions()
        host_window.show()
        host_window.raise_()
        host_window.activateWindow()

        # If the plugin only registers 1 single action (or has a run method), trigger it safely
        if len(registered_actions) == 1 and not registered_widgets:
            safe_action_trigger(registered_actions[0])
        elif hasattr(instance, "run") and callable(getattr(instance, "run")):
            try:
                instance.run()
            except Exception:
                plugin_excepthook(*sys.exc_info())

    if app:
        try:
            app.exec()
        except Exception:
            pass

    return 0

if __name__ == "__main__":
    sys.exit(main())
