#ifndef HAKE_GLOBE_WIDGET_H
#define HAKE_GLOBE_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include "qgis_app.h"

/**
 * \brief High-performance vector animated 3D Earth globe widget for Hake GeoDesk.
 *
 * Renders:
 * - 3D shaded oceanic globe with atmospheric corona halo
 * - Rotating latitude parallels and longitude meridians
 * - Geodetically accurate continental coordinate clusters
 * - Orbiting telemetry satellite with glowing radar beacon
 * - Subtle deep space star field
 */
class APP_EXPORT HakeGlobeWidget : public QWidget
{
  Q_OBJECT

public:
  explicit HakeGlobeWidget( QWidget *parent = nullptr );
  ~HakeGlobeWidget() override = default;

  void startAnimation();
  void stopAnimation();

protected:
  void paintEvent( QPaintEvent *event ) override;
  QSize sizeHint() const override { return QSize( 230, 230 ); }
  QSize minimumSizeHint() const override { return QSize( 180, 180 ); }

private slots:
  void onTick();

private:
  struct GeoPoint
  {
    double lat = 0.0;
    double lon = 0.0;
    int cluster = 0;
  };

  struct Star
  {
    double x = 0.0;
    double y = 0.0;
    double size = 1.0;
    double phase = 0.0;
  };

  void initLandPoints();
  void initStars();

  QTimer *mTimer = nullptr;
  double mRotationAngle = 0.0;
  double mSatelliteAngle = 0.0;
  double mPulsePhase = 0.0;
  bool mIsRunning = false;

  QVector<GeoPoint> mLandPoints;
  QVector<Star> mStars;
};

#endif // HAKE_GLOBE_WIDGET_H
