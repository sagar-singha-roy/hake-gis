/***************************************************************************
 *   qgscoordinateboundspreviewmapwidget.h                                 *
 *   Copyright (C) 2019 by Nyall Dawson                                    *
 *   nyall dot dawson at gmail dot com                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "qgscoordinateboundspreviewmapwidget.h"

#include "qgsapplication.h"
#include "qgsmaptoolpan.h"
#include "qgsrubberband.h"
#include "qgsvectorlayer.h"
#include "qgsvertexmarker.h"

#include <QString>

#include "moc_qgscoordinateboundspreviewmapwidget.cpp"

using namespace Qt::StringLiterals;

QgsCoordinateBoundsPreviewMapWidget::QgsCoordinateBoundsPreviewMapWidget( QWidget *parent )
  : QgsMapCanvas( parent )
{
  mPreviewBand = new QgsRubberBand( this, Qgis::GeometryType::Polygon );
  mPreviewBand->setWidth( 2 );
  mPreviewBand->setColor( QColor( 14, 165, 233, 55 ) );
  mPreviewBand->setStrokeColor( QColor( 2, 132, 199, 230 ) );

  mCanvasPreviewBand = new QgsRubberBand( this, Qgis::GeometryType::Polygon );
  mCanvasPreviewBand->setWidth( 2 );
  const QColor rectColor = QColor( 99, 102, 241, 55 );
  mCanvasPreviewBand->setColor( rectColor );
  mCanvasPreviewBand->setStrokeColor( QColor( 79, 70, 229, 220 ) );

  mCanvasCenterMarker = new QgsVertexMarker( this );
  mCanvasCenterMarker->setIconType( QgsVertexMarker::ICON_CROSS );
  mCanvasCenterMarker->setColor( QColor( 79, 70, 229 ) );
  mCanvasCenterMarker->setPenWidth( 2 );

  const QgsCoordinateReferenceSystem srs( u"EPSG:4326"_s );
  setDestinationCrs( srs );

  const QString layerPath = QgsApplication::pkgDataPath() + u"/resources/data/world_map.gpkg|layername=countries"_s;
  QgsVectorLayer::LayerOptions options;
  options.forceReadOnly = true;
  mLayers << new QgsVectorLayer( layerPath, tr( "World Map" ), u"ogr"_s, options );
  setLayers( mLayers );
  mPanTool = new QgsMapToolPan( this );
  setMapTool( mPanTool );
  setPreviewJobsEnabled( true );
}

QgsCoordinateBoundsPreviewMapWidget::~QgsCoordinateBoundsPreviewMapWidget()
{
  setMapTool( nullptr );


  qDeleteAll( mLayers );
}

void QgsCoordinateBoundsPreviewMapWidget::setPreviewRect( const QgsRectangle &rect )
{
  if ( !qgsDoubleNear( rect.area(), 0.0 ) )
  {
    QgsGeometry geom;
    if ( rect.xMinimum() > rect.xMaximum() )
    {
      const QgsRectangle rect1 = QgsRectangle( -180, rect.yMinimum(), rect.xMaximum(), rect.yMaximum() );
      const QgsRectangle rect2 = QgsRectangle( rect.xMinimum(), rect.yMinimum(), 180, rect.yMaximum() );
      geom = QgsGeometry::fromRect( rect1 );
      geom.addPart( QgsGeometry::fromRect( rect2 ) );
    }
    else
    {
      geom = QgsGeometry::fromRect( rect );
    }
    mPreviewBand->setToGeometry( geom, nullptr );
    mPreviewBand->setColor( QColor( 14, 165, 233, 55 ) );
    mPreviewBand->setStrokeColor( QColor( 2, 132, 199, 230 ) );
    QgsRectangle extent = geom.boundingBox();
    extent.scale( 1.1 );
    setExtent( extent );
    refresh();
    mPreviewBand->show();
  }
  else
  {
    mPreviewBand->hide();
    zoomToFullExtent();
  }
}

QgsRectangle QgsCoordinateBoundsPreviewMapWidget::canvasRect() const
{
  return mCanvasRect;
}

void QgsCoordinateBoundsPreviewMapWidget::setCanvasRect( const QgsRectangle &rect )
{
  mCanvasRect = rect;
  mCanvasPreviewBand->setToGeometry( QgsGeometry::fromRect( mCanvasRect ), nullptr );
  mCanvasPreviewBand->show();
  mCanvasCenterMarker->setCenter( rect.center() );
  mCanvasCenterMarker->show();
}
