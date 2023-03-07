/***************************************************************************
    qgsmaphittest.cpp
    ---------------------
    begin                : September 2014
    copyright            : (C) 2014 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaphittest.h"

#include "qgsfeatureiterator.h"
#include "qgsrendercontext.h"
#include "qgsrenderer.h"
#include "qgsvectorlayer.h"
#include "qgssymbollayerutils.h"
#include "qgsgeometry.h"
#include "qgsgeometryengine.h"
#include "qgsexpressioncontextutils.h"
#include "qgsmaplayerstyle.h"

QgsMapHitTest::QgsMapHitTest( const QgsMapSettings &settings, const QgsGeometry &polygon, const LayerFilterExpression &layerFilterExpression )
  : mSettings( settings )
  , mLayerFilterExpression( layerFilterExpression )
  , mOnlyExpressions( false )
{
  if ( !polygon.isNull() && polygon.type() == Qgis::GeometryType::Polygon )
  {
    mPolygon = polygon;
  }
}

QgsMapHitTest::QgsMapHitTest( const QgsMapSettings &settings, const LayerFilterExpression &layerFilterExpression )
  : mSettings( settings )
  , mLayerFilterExpression( layerFilterExpression )
  , mOnlyExpressions( true )
{
}

void QgsMapHitTest::run()
{
  // TODO: do we need this temp image?
  QImage tmpImage( mSettings.outputSize(), mSettings.outputImageFormat() );
  tmpImage.setDotsPerMeterX( mSettings.outputDpi() * 25.4 );
  tmpImage.setDotsPerMeterY( mSettings.outputDpi() * 25.4 );
  QPainter painter( &tmpImage );

  QgsRenderContext context = QgsRenderContext::fromMapSettings( mSettings );
  context.setPainter( &painter ); // we are not going to draw anything, but we still need a working painter

  const QList< QgsMapLayer * > layers = mSettings.layers( true );
  for ( QgsMapLayer *layer : layers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
    if ( !vl || !vl->renderer() )
      continue;

    if ( !mOnlyExpressions )
    {
      if ( !vl->isInScaleRange( mSettings.scale() ) )
      {
        mHitTest[vl] = SymbolSet(); // no symbols -> will not be shown
        mHitTestRuleKey[vl] = SymbolSet();
        continue;
      }

      context.setCoordinateTransform( mSettings.layerTransform( vl ) );
      context.setExtent( mSettings.outputExtentToLayerExtent( vl, mSettings.visibleExtent() ) );
    }

    context.expressionContext() << QgsExpressionContextUtils::layerScope( vl );
    SymbolSet &usedSymbols = mHitTest[vl];
    SymbolSet &usedSymbolsRuleKey = mHitTestRuleKey[vl];
    runHitTestLayer( vl, usedSymbols, usedSymbolsRuleKey, context );
  }

  painter.end();
}

bool QgsMapHitTest::symbolVisible( QgsSymbol *symbol, QgsVectorLayer *layer ) const
{
  if ( !symbol || !layer || !mHitTest.contains( layer ) )
    return false;

  return mHitTest.value( layer ).contains( QgsSymbolLayerUtils::symbolProperties( symbol ) );
}

bool QgsMapHitTest::legendKeyVisible( const QString &ruleKey, QgsVectorLayer *layer ) const
{
  if ( !layer || !mHitTestRuleKey.contains( layer ) )
    return false;

  return mHitTestRuleKey.value( layer ).contains( ruleKey );
}

void QgsMapHitTest::runHitTestLayer( QgsVectorLayer *vl, SymbolSet &usedSymbols, SymbolSet &usedSymbolsRuleKey, QgsRenderContext &context )
{
  QgsMapLayerStyleOverride styleOverride( vl );
  if ( mSettings.layerStyleOverrides().contains( vl->id() ) )
    styleOverride.setOverrideStyle( mSettings.layerStyleOverrides().value( vl->id() ) );

  std::unique_ptr< QgsFeatureRenderer > r( vl->renderer()->clone() );
  const bool moreSymbolsPerFeature = r->capabilities() & QgsFeatureRenderer::MoreSymbolsPerFeature;
  r->startRender( context, vl->fields() );

  QgsFeatureRequest request;

  const QString rendererFilterExpression = r->filter( vl->fields() );
  if ( !rendererFilterExpression.isEmpty() )
  {
    request.setFilterExpression( rendererFilterExpression );
  }

  QSet<QString> requiredAttributes = r->usedAttributes( context );

  QgsGeometry transformedPolygon = mPolygon;
  if ( !mOnlyExpressions && !mPolygon.isNull() )
  {
    if ( mSettings.destinationCrs() != vl->crs() )
    {
      const QgsCoordinateTransform ct( mSettings.destinationCrs(), vl->crs(), mSettings.transformContext() );
      transformedPolygon.transform( ct );
    }
  }

  std::unique_ptr<QgsExpression> expr;
  if ( mLayerFilterExpression.contains( vl->id() ) )
  {
    const QString expression = mLayerFilterExpression[vl->id()];
    expr.reset( new QgsExpression( expression ) );
    expr->prepare( &context.expressionContext() );

    requiredAttributes.unite( expr->referencedColumns() );
    request.combineFilterExpression( expression );
  }

  request.setSubsetOfAttributes( requiredAttributes, vl->fields() );

  std::unique_ptr< QgsGeometryEngine > polygonEngine;
  if ( !mOnlyExpressions )
  {
    if ( mPolygon.isNull() )
    {
      request.setFilterRect( context.extent() );
      request.setFlags( QgsFeatureRequest::ExactIntersect );
    }
    else
    {
      request.setFilterRect( transformedPolygon.boundingBox() );
      polygonEngine.reset( QgsGeometry::createGeometryEngine( transformedPolygon.constGet() ) );
      polygonEngine->prepareGeometry();
    }
  }
  QgsFeatureIterator fi = vl->getFeatures( request );

  usedSymbols.clear();
  usedSymbolsRuleKey.clear();

  QgsFeature f;
  while ( fi.nextFeature( f ) )
  {
    // filter out elements outside of the polygon
    if ( f.hasGeometry() && polygonEngine )
    {
      if ( !polygonEngine->intersects( f.geometry().constGet() ) )
      {
        continue;
      }
    }

    context.expressionContext().setFeature( f );

    //make sure we store string representation of symbol, not pointer
    //otherwise layer style override changes will delete original symbols and leave hanging pointers
    const auto constLegendKeysForFeature = r->legendKeysForFeature( f, context );
    for ( const QString &legendKey : constLegendKeysForFeature )
    {
      usedSymbolsRuleKey.insert( legendKey );
    }

    if ( moreSymbolsPerFeature )
    {
      const auto constOriginalSymbolsForFeature = r->originalSymbolsForFeature( f, context );
      for ( QgsSymbol *s : constOriginalSymbolsForFeature )
      {
        if ( s )
          usedSymbols.insert( QgsSymbolLayerUtils::symbolProperties( s ) );
      }
    }
    else
    {
      QgsSymbol *s = r->originalSymbolForFeature( f, context );
      if ( s )
        usedSymbols.insert( QgsSymbolLayerUtils::symbolProperties( s ) );
    }
  }
  r->stopRender( context );
}

bool QgsMapHitTest::layerVisible( QgsMapLayer *layer )
{
  QString mapId = layer->id();

  if ( ! layer->dataProvider() )
    return false;
  if ( layer->hasScaleBasedVisibility() )
  {
    if ( mSettings.scale() < layer->minimumScale() || mSettings.scale() > layer->maximumScale() )
      return false;
  }
  else if ( mMapContains.contains( mapId ) )
    return mMapContains.value( mapId );

  QgsRectangle footprint = layer->extent();
  if ( mSettings.destinationCrs() != layer->crs() )
  {
    try
    {
      QgsCoordinateTransform ct = QgsCoordinateTransform( layer->crs(), mSettings.destinationCrs(), mSettings.transformContext() );
      footprint = ct.transformBoundingBox( footprint );
    }
    catch ( QgsCsException & )
    {
      QgsMessageLog::logMessage( QObject::tr( "Could not transform map CRS to layer CRS" ) );
      return false;
    }
  }
  bool withinExt = false;
  if ( mSettings.visibleExtent().intersects( footprint ) )
  {
    withinExt = true;
  }
  mMapContains.insert( mapId, withinExt );
  return withinExt;
}
