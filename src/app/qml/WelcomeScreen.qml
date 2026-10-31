import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "components"

Item {
  id: welcomeScreen

  // Colors come from the "welcomeTheme" map supplied by QgsWelcomeScreen and follow the UI theme.
  property color workspaceColor: welcomeTheme.workspaceColor
  property color pageColor: welcomeTheme.pageColor
  property color elevationColor: welcomeTheme.elevationColor
  property color insetColor: welcomeTheme.insetColor
  property color panelColor: welcomeTheme.panelColor
  property color surfaceColor: welcomeTheme.surfaceColor
  property color pressedSurfaceColor: welcomeTheme.pressedSurfaceColor
  property color primaryColor: welcomeTheme.primaryColor
  property color hoverColor: welcomeTheme.hoverColor
  property color activeColor: welcomeTheme.activeColor
  property color accentColor: welcomeTheme.accentColor
  property color accentSoftColor: welcomeTheme.accentSoftColor
  property color newsAccentColor: welcomeTheme.newsAccentColor
  property color textColor: welcomeTheme.textColor
  property color mutedTextColor: welcomeTheme.mutedTextColor
  property color borderColor: welcomeTheme.borderColor
  property color surfaceBorderColor: welcomeTheme.surfaceBorderColor
  property color statusColor: welcomeTheme.statusColor
  property color onPrimaryTextColor: welcomeTheme.onPrimaryTextColor

  readonly property string uiFont: Qt.platform.os === "osx" ? ".AppleSystemUIFont" : Application.font.family
  readonly property bool narrowLayout: homeSurface.width < welcomeScreen.unit * 40
  readonly property real layoutSizeFactor: homeSurface.width > 1200 && homeSurface.height > 800 ? 1.1 : 1.0

  // All spacing and type derive from the application font so the page follows
  // the user's font size and DPI instead of fixed pixel values.
  FontMetrics {
    id: baseMetrics
    font: Application.font
  }
  readonly property real unit: Math.round(baseMetrics.height)
  readonly property real basePointSize: Application.font.pointSize > 0 ? Application.font.pointSize : 10
  readonly property real buttonHeight: Math.round(baseMetrics.height + 14)
  readonly property real buttonMinWidth: Math.round(baseMetrics.averageCharacterWidth * 14)
  readonly property int cardRadius: 8
  readonly property int surfaceRadius: 10
  readonly property int buttonRadius: 4
  // About 2 logical px, snapped to whole device pixels so the outer edge stays crisp at fractional scaling.
  readonly property real surfaceBorderWidth: Math.max(1, Math.round(2 * Screen.devicePixelRatio)) / Screen.devicePixelRatio
  // Based on the root width (not homeSurface) to avoid a binding loop with narrowLayout.
  readonly property real surfaceMargin: width < unit * 44 ? unit : Math.round(unit * 1.5)

  // Full-bleed workspace tint (self-contained; does not rely on map canvas color)
  Rectangle {
    anchors.fill: parent
    color: welcomeScreen.workspaceColor
  }

  // Hake home surface
  Item {
    id: homeSurface
    anchors.fill: parent
    anchors.margins: welcomeScreen.surfaceMargin

    // Crisp 2px elevation edge (no blur)
    Rectangle {
      anchors.fill: parent
      anchors.topMargin: 2
      anchors.bottomMargin: -2
      radius: welcomeScreen.surfaceRadius
      color: welcomeScreen.elevationColor
    }

    Rectangle {
      anchors.fill: parent
      radius: welcomeScreen.surfaceRadius
      color: welcomeScreen.pageColor
      border.width: welcomeScreen.surfaceBorderWidth
      border.color: welcomeScreen.surfaceBorderColor
    }

    // Decorative contour motif behind the header band; images take no mouse input.
    Image {
      anchors.top: parent.top
      anchors.right: parent.right
      anchors.margins: Math.ceil(welcomeScreen.surfaceBorderWidth)
      width: Math.round(Math.min(parent.width * 0.5, welcomeScreen.unit * 40))
      height: Math.round(width * 2 / 3)
      source: "images/welcome-contours.svg"
      sourceSize.width: width
      sourceSize.height: height
      fillMode: Image.PreserveAspectFit
      mirrorVertically: true
      opacity: 0.1
      asynchronous: true
    }

    ColumnLayout {
      anchors.fill: parent
      anchors.leftMargin: Math.round(welcomeScreen.unit * 1.25)
      anchors.rightMargin: Math.round(welcomeScreen.unit * 1.25)
      anchors.topMargin: Math.round(welcomeScreen.unit * 1.1)
      anchors.bottomMargin: welcomeScreen.unit
      spacing: welcomeScreen.unit

    // Header
    RowLayout {
      Layout.fillWidth: true
      Layout.bottomMargin: Math.round(welcomeScreen.unit * 0.25)
      spacing: Math.round(welcomeScreen.unit * 0.75)

      Image {
        Layout.preferredWidth: Math.round(welcomeScreen.unit * 3.2)
        Layout.preferredHeight: Math.round(welcomeScreen.unit * 3.2)
        source: "images/hake-gis-icon.png"
        sourceSize.width: Layout.preferredWidth
        sourceSize.height: Layout.preferredHeight
        fillMode: Image.PreserveAspectFit
        mipmap: true
        asynchronous: true
      }

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 1

        Label {
          text: qsTr("HAKE GEOSPATIAL")
          color: welcomeScreen.primaryColor
          font.family: welcomeScreen.uiFont
          font.pointSize: welcomeScreen.basePointSize * 0.85
          font.weight: Font.Bold
          font.letterSpacing: 1.4
        }
        Label {
          Layout.fillWidth: true
          text: productDisplayName
          color: welcomeScreen.textColor
          font.family: welcomeScreen.uiFont
          font.pointSize: welcomeScreen.basePointSize * 1.6
          font.weight: Font.DemiBold
          wrapMode: Text.WordWrap
        }
        Label {
          Layout.fillWidth: true
          text: qsTr("Version: %1").arg(appVersion)
          color: welcomeScreen.mutedTextColor
          font.family: welcomeScreen.uiFont
          font.pointSize: welcomeScreen.basePointSize * 0.9
          wrapMode: Text.WordWrap
        }
      }

      RoundButton {
        id: closeButton
        Layout.preferredWidth: Math.round(welcomeScreen.unit * 2)
        Layout.preferredHeight: Math.round(welcomeScreen.unit * 2)
        Layout.alignment: Qt.AlignTop
        text: "×"
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Close")
        Accessible.name: qsTr("Close welcome")
        font.family: welcomeScreen.uiFont
        font.pointSize: welcomeScreen.basePointSize * 1.4
        padding: 0
        onClicked: welcomeScreenController.hideScene()

        contentItem: Text {
          text: closeButton.text
          color: closeButton.hovered ? welcomeScreen.textColor : welcomeScreen.mutedTextColor
          font: closeButton.font
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
          radius: width / 2
          color: closeButton.hovered ? welcomeScreen.accentSoftColor : welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor
        }
      }
    }

    GridLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      columns: welcomeScreen.narrowLayout ? 1 : 2
      columnSpacing: Math.round(welcomeScreen.unit * 1.1)
      rowSpacing: Math.round(welcomeScreen.unit * 0.9)

      // Left: hero, actions, recent projects (3 : 2 with the news panel)
      ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: welcomeScreen.narrowLayout ? -1 : parent.width * 0.6
        spacing: Math.round(welcomeScreen.unit * 0.9)

        Rectangle {
          id: welcomeCard
          Layout.fillWidth: true
          Layout.preferredHeight: heroColumn.implicitHeight + 2 * Math.round(welcomeScreen.unit * 1.1)
          radius: welcomeScreen.cardRadius
          color: welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor
          clip: true

          // 4px left accent bar with rounded outer corners only (per-corner radii
          // need Qt 6.7): a rounded navy plate, its right part masked back to the
          // card surface, then the card's top/bottom border restored over the mask.
          Rectangle {
            width: 4 + 2 * welcomeScreen.cardRadius
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: welcomeScreen.cardRadius
            color: welcomeScreen.primaryColor
          }
          Rectangle {
            x: 4
            width: 2 * welcomeScreen.cardRadius
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: welcomeScreen.panelColor
          }
          Rectangle {
            x: 4
            width: 2 * welcomeScreen.cardRadius
            height: 1
            anchors.top: parent.top
            color: welcomeScreen.borderColor
          }
          Rectangle {
            x: 4
            width: 2 * welcomeScreen.cardRadius
            height: 1
            anchors.bottom: parent.bottom
            color: welcomeScreen.borderColor
          }

          ColumnLayout {
            id: heroColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 4 + Math.round(welcomeScreen.unit * 1.1)
            anchors.rightMargin: Math.round(welcomeScreen.unit * 1.1)
            anchors.topMargin: Math.round(welcomeScreen.unit * 1.1)
            spacing: Math.round(welcomeScreen.unit * 0.4)

            Label {
              Layout.fillWidth: true
              text: qsTr("Welcome to Hake GeoDesk")
              color: welcomeScreen.textColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize * 1.9
              font.weight: Font.DemiBold
              wrapMode: Text.WordWrap
            }
            Label {
              Layout.fillWidth: true
              text: qsTr("Professional GIS for Mapping, Analysis & Spatial Intelligence")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize
              wrapMode: Text.WordWrap
            }

            Flow {
              Layout.fillWidth: true
              Layout.topMargin: Math.round(welcomeScreen.unit * 0.9)
              spacing: Math.round(welcomeScreen.unit * 0.6)

              // Soft
              Button {
                id: gettingStartedButton
                implicitHeight: welcomeScreen.buttonHeight
                implicitWidth: Math.max(welcomeScreen.buttonMinWidth, implicitContentWidth + leftPadding + rightPadding)
                text: qsTr("Getting started")
                hoverEnabled: true
                Accessible.name: text
                onClicked: welcomeScreenController.openGettingStarted()
                contentItem: Text {
                  text: gettingStartedButton.text
                  color: welcomeScreen.accentColor
                  font.family: welcomeScreen.uiFont
                  font.pointSize: welcomeScreen.basePointSize
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 16
                  rightPadding: 16
                }
                background: Rectangle {
                  radius: welcomeScreen.buttonRadius
                  color: gettingStartedButton.hovered || gettingStartedButton.pressed ? welcomeScreen.pressedSurfaceColor : welcomeScreen.accentSoftColor
                  border.width: 1
                  border.color: gettingStartedButton.pressed ? welcomeScreen.hoverColor : welcomeScreen.borderColor
                }
              }

              // Primary
              Button {
                id: openProjectButton
                implicitHeight: welcomeScreen.buttonHeight
                implicitWidth: Math.max(welcomeScreen.buttonMinWidth, implicitContentWidth + leftPadding + rightPadding)
                text: qsTr("Open project")
                hoverEnabled: true
                Accessible.name: text
                onClicked: welcomeScreenController.openProjectDialog()
                contentItem: Text {
                  text: openProjectButton.text
                  color: welcomeScreen.onPrimaryTextColor
                  font.family: welcomeScreen.uiFont
                  font.pointSize: welcomeScreen.basePointSize
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 16
                  rightPadding: 16
                }
                background: Rectangle {
                  radius: welcomeScreen.buttonRadius
                  color: openProjectButton.pressed ? welcomeScreen.activeColor
                       : openProjectButton.hovered ? welcomeScreen.hoverColor
                       : welcomeScreen.primaryColor
                  border.width: 1
                  border.color: color
                }
              }

              // Outline
              Button {
                id: newProjectButton
                implicitHeight: welcomeScreen.buttonHeight
                implicitWidth: Math.max(welcomeScreen.buttonMinWidth, implicitContentWidth + leftPadding + rightPadding)
                text: qsTr("New project")
                hoverEnabled: true
                Accessible.name: text
                onClicked: {
                  welcomeScreenController.createBlankProject()
                  welcomeScreenController.hideScene()
                }
                contentItem: Text {
                  text: newProjectButton.text
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pointSize: welcomeScreen.basePointSize
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 16
                  rightPadding: 16
                }
                background: Rectangle {
                  radius: welcomeScreen.buttonRadius
                  color: newProjectButton.pressed ? welcomeScreen.pressedSurfaceColor
                       : newProjectButton.hovered ? welcomeScreen.accentSoftColor
                       : welcomeScreen.panelColor
                  border.width: 1
                  border.color: newProjectButton.pressed ? welcomeScreen.hoverColor : welcomeScreen.borderColor
                }
              }

              // Outline
              Button {
                id: visitButton
                implicitHeight: welcomeScreen.buttonHeight
                implicitWidth: Math.max(welcomeScreen.buttonMinWidth, implicitContentWidth + leftPadding + rightPadding)
                text: qsTr("Visit Hake")
                hoverEnabled: true
                Accessible.name: text
                onClicked: Qt.openUrlExternally("https://haketech.com")
                contentItem: Text {
                  text: visitButton.text
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pointSize: welcomeScreen.basePointSize
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 16
                  rightPadding: 16
                }
                background: Rectangle {
                  radius: welcomeScreen.buttonRadius
                  color: visitButton.pressed ? welcomeScreen.pressedSurfaceColor
                       : visitButton.hovered ? welcomeScreen.accentSoftColor
                       : welcomeScreen.panelColor
                  border.width: 1
                  border.color: visitButton.pressed ? welcomeScreen.hoverColor : welcomeScreen.borderColor
                }
              }
            }
          }
        }

        Rectangle {
          Layout.fillWidth: true
          Layout.fillHeight: true
          radius: welcomeScreen.cardRadius
          color: welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor
          clip: true

          ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: Math.round(welcomeScreen.unit * 1.1)
            anchors.rightMargin: Math.round(welcomeScreen.unit * 1.1)
            anchors.topMargin: welcomeScreen.unit
            anchors.bottomMargin: welcomeScreen.unit
            spacing: Math.round(welcomeScreen.unit * 0.35)

            RowLayout {
              Layout.fillWidth: true
              Label {
                Layout.fillWidth: true
                text: qsTr("Recent projects")
                color: welcomeScreen.textColor
                font.family: welcomeScreen.uiFont
                font.pointSize: welcomeScreen.basePointSize * 1.15
                font.weight: Font.DemiBold
              }
              Label {
                visible: recentProjectsListView.count === 0
                text: qsTr("No recent projects yet")
                color: welcomeScreen.mutedTextColor
                font.family: welcomeScreen.uiFont
                font.pointSize: welcomeScreen.basePointSize * 0.9
              }
            }

            ListView {
              id: recentProjectsListView
              Layout.fillWidth: true
              Layout.fillHeight: true
              spacing: Math.round(welcomeScreen.unit * 0.6)
              clip: true
              model: recentProjectsModel

              delegate: ProjectCard {
                width: recentProjectsListView.width - 8
                layoutSizeFactor: welcomeScreen.layoutSizeFactor
                title: Title || ""
                subtitle: (ProjectNativePath || ProjectPath || "").replace(/([\\\/])/g, "$1\u200b")
                crs: Crs || ""
                imageSource: PreviewImagePath || ""
                isEnabled: Exists
                isPinned: Pinned
                isSelected: recentProjectsListView.currentIndex === index
                radius: welcomeScreen.cardRadius

                onClicked: (mouse) => {
                  if (mouse.button == Qt.LeftButton && isEnabled) {
                    welcomeScreenController.openProject(ProjectPath)
                    welcomeScreenController.hideScene()
                  } else if (mouse.button == Qt.RightButton) {
                    recentProjectsMenu.projectIndex = index
                    recentProjectsMenu.projectPinned = Pinned
                    recentProjectsMenu.projectExists = Exists
                    recentProjectsMenu.projectHasNativePath = ProjectNativePath != ""
                    const point = mapToItem(recentProjectsListView, mouse.x, mouse.y)
                    recentProjectsMenu.popup(point.x, point.y)
                  }
                }
              }

              ScrollBar.vertical: CustomScrollBar {
                policy: ScrollBar.AsNeeded
              }

              Rectangle {
                width: recentProjectsListView.width
                height: recentProjectsListView.height
                visible: recentProjectsListView.count === 0
                radius: welcomeScreen.cardRadius
                color: welcomeScreen.insetColor
                border.width: 1
                border.color: welcomeScreen.borderColor

                Label {
                  anchors.centerIn: parent
                  width: Math.min(implicitWidth, parent.width - 2 * welcomeScreen.unit)
                  horizontalAlignment: Text.AlignHCenter
                  wrapMode: Text.WordWrap
                  text: qsTr("Open or create a project to see it here.")
                  color: welcomeScreen.mutedTextColor
                  font.family: welcomeScreen.uiFont
                  font.pointSize: welcomeScreen.basePointSize * 0.95
                }
              }

              Menu {
                id: recentProjectsMenu
                property int projectIndex: 0
                property bool projectPinned: false
                property bool projectExists: false
                property bool projectHasNativePath: false

                background: Rectangle {
                  implicitWidth: Math.round(welcomeScreen.unit * 12.5)
                  implicitHeight: welcomeScreen.unit * 10
                  radius: welcomeScreen.cardRadius
                  color: welcomeScreen.panelColor
                  border.color: welcomeScreen.borderColor
                  border.width: 1
                }

                MenuItem {
                  text: recentProjectsMenu.projectPinned ? qsTr("Unpin from List") : qsTr("Pin to List")
                  onClicked: {
                    if (recentProjectsMenu.projectPinned) {
                      recentProjectsModel.unpinProject(recentProjectsMenu.projectIndex)
                    } else {
                      recentProjectsModel.pinProject(recentProjectsMenu.projectIndex)
                    }
                  }
                }
                MenuItem {
                  text: qsTr("Refresh")
                  enabled: !recentProjectsMenu.projectExists
                  visible: enabled
                  height: enabled ? implicitHeight : 0
                  onClicked: recentProjectsModel.recheckProject(recentProjectsMenu.projectIndex)
                }
                MenuItem {
                  text: qsTr("Open Directory…")
                  enabled: recentProjectsMenu.projectExists && recentProjectsMenu.projectHasNativePath
                  visible: enabled
                  height: enabled ? implicitHeight : 0
                  onClicked: recentProjectsModel.openProject(recentProjectsMenu.projectIndex)
                }
                MenuItem {
                  text: qsTr("Remove from List")
                  onClicked: recentProjectsModel.removeProject(recentProjectsMenu.projectIndex)
                }
                MenuSeparator {}
                MenuItem {
                  text: qsTr("Clear List")
                  onClicked: welcomeScreenController.clearRecentProjects()
                }
              }
            }
          }
        }
      }

      // Right: news
      Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: welcomeScreen.narrowLayout ? -1 : parent.width * 0.4
        Layout.minimumHeight: welcomeScreen.narrowLayout ? welcomeScreen.unit * 12 : -1
        radius: welcomeScreen.cardRadius
        color: welcomeScreen.surfaceColor
        border.width: 1
        border.color: welcomeScreen.borderColor
        clip: true

        ColumnLayout {
          anchors.fill: parent
          anchors.leftMargin: Math.round(welcomeScreen.unit * 1.1)
          anchors.rightMargin: Math.round(welcomeScreen.unit * 1.1)
          anchors.topMargin: welcomeScreen.unit
          anchors.bottomMargin: welcomeScreen.unit
          spacing: Math.round(welcomeScreen.unit * 0.45)

          RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Math.round(welcomeScreen.unit * 0.4)
            Label {
              text: qsTr("RECENT NEWS")
              color: welcomeScreen.newsAccentColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize * 0.85
              font.weight: Font.Bold
              font.letterSpacing: 1.2
            }
            Item { Layout.fillWidth: true }
            BusyIndicator {
              Layout.preferredWidth: Math.round(welcomeScreen.unit * 1.1)
              Layout.preferredHeight: Math.round(welcomeScreen.unit * 1.1)
              running: newsFeedParser.isFetching
              visible: running
            }
            Rectangle {
              Layout.preferredWidth: Math.round(welcomeScreen.unit * 0.5)
              Layout.preferredHeight: Math.round(welcomeScreen.unit * 0.5)
              radius: width / 2
              color: welcomeScreen.statusColor
              visible: newsFeedParser.enabled && newsListView.count > 0
            }
          }

          ListView {
            id: newsListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            clip: true
            visible: newsFeedParser.enabled && count > 0
            model: newsFeedModel

            delegate: Item {
              width: newsListView.width
              height: Math.max(welcomeScreen.unit * 6, newsColumn.implicitHeight + Math.round(welcomeScreen.unit * 1.25))

              RowLayout {
                anchors.fill: parent
                anchors.topMargin: Math.round(welcomeScreen.unit * 0.25)
                anchors.bottomMargin: Math.round(welcomeScreen.unit * 0.75)
                spacing: Math.round(welcomeScreen.unit * 0.75)

                Rectangle {
                  Layout.preferredWidth: welcomeScreen.unit * 2
                  Layout.preferredHeight: welcomeScreen.unit * 2
                  Layout.alignment: Qt.AlignTop
                  radius: welcomeScreen.cardRadius
                  color: welcomeScreen.accentSoftColor
                  Label {
                    anchors.centerIn: parent
                    text: "✦"
                    color: welcomeScreen.newsAccentColor
                    font.pointSize: welcomeScreen.basePointSize * 1.05
                    font.weight: Font.DemiBold
                  }
                }

                ColumnLayout {
                  id: newsColumn
                  Layout.fillWidth: true
                  spacing: Math.round(welcomeScreen.unit * 0.25)

                  RowLayout {
                    Label {
                      text: qsTr("NEWS")
                      color: welcomeScreen.newsAccentColor
                      font.family: welcomeScreen.uiFont
                      font.pointSize: welcomeScreen.basePointSize * 0.75
                      font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    RoundButton {
                      Layout.preferredWidth: Math.round(welcomeScreen.unit * 1.4)
                      Layout.preferredHeight: Math.round(welcomeScreen.unit * 1.4)
                      flat: true
                      text: "×"
                      Accessible.name: qsTr("Dismiss news")
                      onClicked: newsFeedParser.dismissEntry(Key)
                      contentItem: Text {
                        text: parent.text
                        color: welcomeScreen.mutedTextColor
                        font.pointSize: welcomeScreen.basePointSize * 1.05
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                        radius: width / 2
                        color: parent.hovered ? welcomeScreen.accentSoftColor : "transparent"
                      }
                    }
                  }
                  Label {
                    Layout.fillWidth: true
                    text: Title || ""
                    color: welcomeScreen.textColor
                    font.family: welcomeScreen.uiFont
                    font.pointSize: welcomeScreen.basePointSize * 1.05
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                  }
                  Label {
                    Layout.fillWidth: true
                    textFormat: Text.RichText
                    text: Content || ""
                    color: welcomeScreen.mutedTextColor
                    font.family: welcomeScreen.uiFont
                    font.pointSize: welcomeScreen.basePointSize * 0.9
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    onLinkActivated: link => Qt.openUrlExternally(link)
                  }
                  Label {
                    Layout.fillWidth: true
                    visible: Link != ""
                    text: qsTr("Read more")
                    color: welcomeScreen.newsAccentColor
                    font.family: welcomeScreen.uiFont
                    font.pointSize: welcomeScreen.basePointSize * 0.85
                    font.weight: Font.Bold
                    font.underline: readMoreArea.containsMouse
                    MouseArea {
                      id: readMoreArea
                      anchors.fill: parent
                      hoverEnabled: true
                      cursorShape: Qt.PointingHandCursor
                      onClicked: Qt.openUrlExternally(Link)
                    }
                  }
                }
              }

              Rectangle {
                visible: index < newsListView.count - 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: welcomeScreen.borderColor
              }
            }

            ScrollBar.vertical: CustomScrollBar {
              policy: ScrollBar.AsNeeded
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Math.round(welcomeScreen.unit * 0.45)
            visible: !newsListView.visible

            Label {
              Layout.fillWidth: true
              text: qsTr("What’s new")
              color: welcomeScreen.textColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize * 1.15
              font.weight: Font.DemiBold
            }
            Label {
              Layout.fillWidth: true
              text: qsTr("Stay updated on new features, releases, and product highlights from Hake Geospatial.")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize
              wrapMode: Text.WordWrap
            }
            Item { Layout.fillHeight: true }
            Button {
              id: enableNewsButton
              Layout.fillWidth: true
              Layout.preferredHeight: welcomeScreen.buttonHeight
              visible: !newsFeedParser.enabled
              text: qsTr("Enable news feed")
              hoverEnabled: true
              onClicked: {
                newsFeedParser.enabled = true
                newsFeedParser.fetch()
              }
              contentItem: Text {
                text: enableNewsButton.text
                color: welcomeScreen.accentColor
                font.family: welcomeScreen.uiFont
                font.pointSize: welcomeScreen.basePointSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
              background: Rectangle {
                radius: welcomeScreen.buttonRadius
                color: enableNewsButton.hovered || enableNewsButton.pressed ? welcomeScreen.pressedSurfaceColor : welcomeScreen.panelColor
                border.width: 1
                border.color: enableNewsButton.pressed ? welcomeScreen.hoverColor : welcomeScreen.borderColor
              }
            }
            Label {
              Layout.fillWidth: true
              visible: newsFeedParser.enabled && !newsFeedParser.isFetching && newsListView.count === 0
              text: qsTr("No news items right now. Check back later.")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pointSize: welcomeScreen.basePointSize * 0.95
              wrapMode: Text.WordWrap
            }
          }
        }
      }
    }

    UpdateNotificationBar {
      id: pluginsUpdateBar
      Layout.fillWidth: true
      Layout.preferredHeight: welcomeScreen.unit * 3
      radius: welcomeScreen.cardRadius
      visible: false
      color: "#0f265c"
      onInstallClicked: {
        visible = false
        welcomeScreenController.showPluginManager()
      }
    }

    UpdateNotificationBar {
      id: qgisUpdateBar
      Layout.fillWidth: true
      Layout.preferredHeight: welcomeScreen.unit * 3
      radius: welcomeScreen.cardRadius
      visible: false
      color: "#0f265c"
      onInstallClicked: Qt.openUrlExternally("https://haketech.com")
    }

    // Footer, bottom-left of the home surface
    ColumnLayout {
      Layout.fillWidth: true
      spacing: 1

      Label {
        Layout.fillWidth: true
        text: qsTr("Powered by Hake Technologies")
        color: welcomeScreen.mutedTextColor
        font.family: welcomeScreen.uiFont
        font.pointSize: welcomeScreen.basePointSize * 0.9
        wrapMode: Text.WordWrap
      }

      Label {
        Layout.fillWidth: true
        text: qsTr("© 2026 Hake Technologies Private Limited")
        color: welcomeScreen.mutedTextColor
        font.family: welcomeScreen.uiFont
        font.pointSize: welcomeScreen.basePointSize * 0.85
        wrapMode: Text.WordWrap
      }
    }
    }
  }

  DropArea {
    anchors.fill: parent
    onDropped: drop => {
      let formatsData = {}
      for (const format of drop.formats) {
        formatsData[format] = drop.getDataAsArrayBuffer(format)
      }
      welcomeScreenController.forwardDrop(drop.text, drop.urls, formatsData)
    }
  }

  Connections {
    target: welcomeScreenController
    function onNewVersionAvailable(versionString) {
      qgisUpdateBar.message = qsTr("%1 %2 is out!").arg(productDisplayName).arg(versionString)
      qgisUpdateBar.visible = true
    }
    function onPluginUpdatesAvailable(plugins) {
      pluginsUpdateBar.message = qsTr("The following plugin(s) have available updates: %1", "", plugins.length).arg(plugins.join(", "))
      pluginsUpdateBar.visible = true
    }
  }

  Component.onCompleted: {
    if (newsFeedParser.enabled) {
      newsFeedParser.fetch()
    }
  }
}
