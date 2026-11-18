#include "hakeglobewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

HakeGlobeWidget::HakeGlobeWidget( QWidget *parent )
  : QWidget( parent )
{
  setAttribute( Qt::WA_OpaquePaintEvent, false );
  setAttribute( Qt::WA_NoSystemBackground, true );
  setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

  initStars();
  initLandPoints();

  mTimer = new QTimer( this );
  mTimer->setInterval( 33 ); // ~30 FPS
  connect( mTimer, &QTimer::timeout, this, &HakeGlobeWidget::onTick );

  startAnimation();
}

void HakeGlobeWidget::startAnimation()
{
  if ( !mIsRunning )
  {
    mIsRunning = true;
    mTimer->start();
  }
}

void HakeGlobeWidget::stopAnimation()
{
  if ( mIsRunning )
  {
    mIsRunning = false;
    mTimer->stop();
  }
}

void HakeGlobeWidget::onTick()
{
  mRotationAngle = std::fmod( mRotationAngle + 0.85, 360.0 );
  mSatelliteAngle = std::fmod( mSatelliteAngle + 1.6, 360.0 );
  mPulsePhase = std::fmod( mPulsePhase + 0.08, 6.2831853 );
  update();
}

void HakeGlobeWidget::initStars()
{
  mStars.clear();
  // Pseudorandom static space stars
  const double starCoords[][4] = {
    {0.08, 0.12, 1.2, 0.4}, {0.18, 0.08, 1.5, 1.2}, {0.28, 0.16, 1.0, 2.5},
    {0.82, 0.10, 1.4, 0.8}, {0.92, 0.18, 1.1, 1.9}, {0.88, 0.32, 1.6, 3.1},
    {0.06, 0.45, 1.0, 1.7}, {0.12, 0.62, 1.5, 0.3}, {0.05, 0.82, 1.2, 2.8},
    {0.86, 0.75, 1.3, 1.1}, {0.94, 0.88, 1.7, 2.4}, {0.78, 0.90, 1.0, 0.6},
    {0.22, 0.88, 1.4, 1.5}, {0.35, 0.94, 1.1, 3.3}, {0.68, 0.92, 1.3, 2.1},
    {0.75, 0.06, 1.1, 1.8}, {0.15, 0.30, 1.3, 0.9}, {0.90, 0.55, 1.0, 2.7}
  };
  for ( const auto &sc : starCoords )
  {
    mStars.append( { sc[0], sc[1], sc[2], sc[3] } );
  }
}

void HakeGlobeWidget::initLandPoints()
{
  mLandPoints.clear();
  // Geodetically representative coordinate points for Earth continents: (lat, lon, cluster)
  // Cluster: 1=N.America, 2=S.America, 3=Europe, 4=Africa, 5=Asia, 6=Australia

  // North America
  mLandPoints.append( { 64.0, -150.0, 1 } ); // Alaska
  mLandPoints.append( { 58.0, -135.0, 1 } );
  mLandPoints.append( { 55.0, -115.0, 1 } ); // Canada West
  mLandPoints.append( { 52.0, -95.0, 1 } );  // Canada Central
  mLandPoints.append( { 50.0, -75.0, 1 } );  // Canada East
  mLandPoints.append( { 45.0, -122.0, 1 } ); // US Pacific NW
  mLandPoints.append( { 37.0, -122.0, 1 } ); // California
  mLandPoints.append( { 34.0, -118.0, 1 } );
  mLandPoints.append( { 42.0, -88.0, 1 } );  // Chicago / Great Lakes
  mLandPoints.append( { 40.0, -74.0, 1 } );  // New York
  mLandPoints.append( { 35.0, -78.0, 1 } );  // East Coast
  mLandPoints.append( { 30.0, -95.0, 1 } );  // Texas
  mLandPoints.append( { 26.0, -80.0, 1 } );  // Florida
  mLandPoints.append( { 23.0, -102.0, 1 } ); // Mexico
  mLandPoints.append( { 19.0, -99.0, 1 } );  // Mexico City
  mLandPoints.append( { 14.0, -90.0, 1 } );  // Central America

  // South America
  mLandPoints.append( { 8.0, -66.0, 2 } );   // Venezuela
  mLandPoints.append( { 4.0, -73.0, 2 } );   // Colombia
  mLandPoints.append( { -3.0, -60.0, 2 } );  // Amazon
  mLandPoints.append( { -8.0, -37.0, 2 } );  // Brazil NE
  mLandPoints.append( { -12.0, -77.0, 2 } ); // Peru
  mLandPoints.append( { -15.0, -47.0, 2 } ); // Brasilia
  mLandPoints.append( { -23.0, -46.0, 2 } ); // Sao Paulo
  mLandPoints.append( { -20.0, -65.0, 2 } ); // Bolivia
  mLandPoints.append( { -33.0, -70.0, 2 } ); // Chile
  mLandPoints.append( { -34.0, -58.0, 2 } ); // Buenos Aires
  mLandPoints.append( { -45.0, -68.0, 2 } ); // Patagonia

  // Europe
  mLandPoints.append( { 55.0, -3.0, 3 } );   // UK
  mLandPoints.append( { 60.0, 10.0, 3 } );   // Scandinavia
  mLandPoints.append( { 48.0, 2.0, 3 } );    // France
  mLandPoints.append( { 52.0, 13.0, 3 } );   // Germany
  mLandPoints.append( { 40.0, -4.0, 3 } );   // Spain
  mLandPoints.append( { 42.0, 12.0, 3 } );   // Italy
  mLandPoints.append( { 52.0, 21.0, 3 } );   // Poland
  mLandPoints.append( { 56.0, 37.0, 3 } );   // Moscow / West Russia
  mLandPoints.append( { 38.0, 24.0, 3 } );   // Greece

  // Africa
  mLandPoints.append( { 32.0, -5.0, 4 } );   // Morocco
  mLandPoints.append( { 30.0, 31.0, 4 } );   // Egypt
  mLandPoints.append( { 22.0, 12.0, 4 } );   // Sahara
  mLandPoints.append( { 12.0, -8.0, 4 } );   // West Africa
  mLandPoints.append( { 6.0, 3.0, 4 } );     // Nigeria
  mLandPoints.append( { 0.0, 25.0, 4 } );    // Congo
  mLandPoints.append( { -1.0, 37.0, 4 } );   // Kenya / East Africa
  mLandPoints.append( { -18.0, 47.0, 4 } );  // Madagascar
  mLandPoints.append( { -26.0, 28.0, 4 } );  // Johannesburg
  mLandPoints.append( { -34.0, 18.0, 4 } );  // Cape Town

  // Asia
  mLandPoints.append( { 24.0, 45.0, 5 } );   // Saudi Arabia
  mLandPoints.append( { 32.0, 53.0, 5 } );   // Iran
  mLandPoints.append( { 28.0, 77.0, 5 } );   // New Delhi
  mLandPoints.append( { 19.0, 73.0, 5 } );   // Mumbai
  mLandPoints.append( { 13.0, 80.0, 5 } );   // South India
  mLandPoints.append( { 31.0, 121.0, 5 } );  // Shanghai
  mLandPoints.append( { 39.0, 116.0, 5 } );  // Beijing
  mLandPoints.append( { 22.0, 114.0, 5 } );  // Hong Kong
  mLandPoints.append( { 35.0, 139.0, 5 } );  // Tokyo
  mLandPoints.append( { 37.0, 127.0, 5 } );  // Seoul
  mLandPoints.append( { 14.0, 100.0, 5 } );  // Thailand
  mLandPoints.append( { 1.0, 103.0, 5 } );   // Singapore
  mLandPoints.append( { -6.0, 106.0, 5 } );  // Indonesia
  mLandPoints.append( { 55.0, 82.0, 5 } );   // Siberia

  // Australia & NZ
  mLandPoints.append( { -20.0, 134.0, 6 } ); // Central Australia
  mLandPoints.append( { -32.0, 116.0, 6 } ); // Perth
  mLandPoints.append( { -34.0, 151.0, 6 } ); // Sydney
  mLandPoints.append( { -37.0, 145.0, 6 } ); // Melbourne
  mLandPoints.append( { -23.0, 150.0, 6 } ); // Queensland
  mLandPoints.append( { -41.0, 174.0, 6 } ); // New Zealand
}

void HakeGlobeWidget::paintEvent( QPaintEvent * )
{
  QPainter p( this );
  p.setRenderHint( QPainter::Antialiasing, true );

  int w = width();
  int h = height();
  double cx = w / 2.0;
  double cy = h / 2.0;
  double R = std::min( w, h ) * 0.40;

  if ( R < 20.0 )
    return;

  // 1. Star field
  for ( const Star &s : mStars )
  {
    double alpha = 90.0 + 70.0 * std::sin( mPulsePhase + s.phase );
    p.setPen( Qt::NoPen );
    p.setBrush( QColor( 224, 242, 254, static_cast<int>( alpha ) ) );
    p.drawEllipse( QPointF( s.x * w, s.y * h ), s.size, s.size );
  }

  // 2. Atmospheric Corona Halo
  QRadialGradient corona( cx, cy, R * 1.38 );
  corona.setColorAt( 0.0, QColor( 2, 132, 199, 40 ) );
  corona.setColorAt( 0.70, QColor( 14, 165, 233, 20 ) );
  corona.setColorAt( 0.94, QColor( 56, 189, 248, 6 ) );
  corona.setColorAt( 1.0, QColor( 0, 0, 0, 0 ) );
  p.setPen( Qt::NoPen );
  p.setBrush( corona );
  p.drawEllipse( QPointF( cx, cy ), R * 1.38, R * 1.38 );

  // 3. 3D Oceanic Base Sphere
  QRadialGradient ocean( cx - 0.32 * R, cy - 0.32 * R, R * 1.35 );
  ocean.setColorAt( 0.0, QColor( 37, 99, 235 ) );   // #2563eb bright sapphire
  ocean.setColorAt( 0.35, QColor( 29, 78, 216 ) );  // #1d4ed8 deep ocean
  ocean.setColorAt( 0.65, QColor( 15, 35, 95 ) );   // #0f235f deep navy
  ocean.setColorAt( 0.88, QColor( 6, 18, 48 ) );    // #061230 midnight
  ocean.setColorAt( 1.0, QColor( 2, 6, 18 ) );      // #020612 dark limb
  p.setPen( Qt::NoPen );
  p.setBrush( ocean );
  p.drawEllipse( QPointF( cx, cy ), R, R );

  // 4. Transform to Axial Tilt Coordinate System (-23.5 degrees)
  p.save();
  p.translate( cx, cy );
  p.rotate( -23.5 );

  // Latitude Parallels
  const double parallels[] = { -65.0, -45.0, -23.5, 0.0, 23.5, 45.0, 65.0 };
  p.setPen( QPen( QColor( 56, 189, 248, 38 ), 1.0, Qt::SolidLine ) );
  p.setBrush( Qt::NoBrush );

  for ( double lat : parallels )
  {
    double rad = lat * 3.14159265 / 180.0;
    double y = -R * std::sin( rad );
    double rPar = R * std::cos( rad );
    // Parallel visible arc
    p.drawLine( QPointF( -rPar, y ), QPointF( rPar, y ) );
  }

  // Longitude Meridians
  for ( int k = 0; k < 12; ++k )
  {
    double lonDeg = std::fmod( mRotationAngle + k * 30.0, 360.0 );
    double lonRad = lonDeg * 3.14159265 / 180.0;
    double c = std::cos( lonRad );
    if ( c > 0.0 )
    {
      double wm = R * c;
      int alpha = static_cast<int>( 85.0 * c );
      p.setPen( QPen( QColor( 56, 189, 248, alpha ), 1.0 ) );
      p.drawEllipse( QRectF( -wm, -R, 2 * wm, 2 * R ) );
    }
  }

  // Continental Landmass Nodes (Rotating Coordinates)
  const double PI = 3.1415926535;
  for ( const GeoPoint &pt : mLandPoints )
  {
    double latRad = pt.lat * PI / 180.0;
    double lonRad = std::fmod( pt.lon + mRotationAngle, 360.0 ) * PI / 180.0;

    double z = R * std::cos( latRad ) * std::cos( lonRad );
    if ( z > 0.0 )
    {
      double x = R * std::cos( latRad ) * std::sin( lonRad );
      double y = -R * std::sin( latRad );

      double intensity = z / R;
      int nodeAlpha = static_cast<int>( 230.0 * intensity );
      double nodeRadius = 1.3 + 1.8 * intensity;

      // Glowing geospatial node
      p.setPen( Qt::NoPen );
      p.setBrush( QColor( 52, 211, 153, nodeAlpha ) ); // cartographic emerald
      p.drawEllipse( QPointF( x, y ), nodeRadius, nodeRadius );

      // Subtle pulse on key anchor cities
      if ( pt.cluster == 1 || pt.cluster == 3 )
      {
        double pulseR = nodeRadius + 1.2 + 0.8 * std::sin( mPulsePhase );
        p.setPen( QColor( 56, 189, 248, static_cast<int>( 120.0 * intensity ) ) );
        p.setBrush( Qt::NoBrush );
        p.drawEllipse( QPointF( x, y ), pulseR, pulseR );
      }
    }
  }

  p.restore(); // Back to screen coordinates

  // 5. Atmospheric Rim Crescent Glow (Upper-Left Limb)
  QRadialGradient limb( cx - 0.28 * R, cy - 0.28 * R, R );
  limb.setColorAt( 0.78, QColor( 186, 230, 253, 0 ) );
  limb.setColorAt( 0.95, QColor( 186, 230, 253, 110 ) );
  limb.setColorAt( 1.0, QColor( 255, 255, 255, 140 ) );
  p.setPen( Qt::NoPen );
  p.setBrush( limb );
  p.drawEllipse( QPointF( cx, cy ), R, R );
}
