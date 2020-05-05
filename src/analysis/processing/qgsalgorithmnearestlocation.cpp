/***************************************************************************
                         qgsalgorithmnearestlocation.cpp
                         ---------------------
    begin                : January 2020
    copyright            : (C) 2020 by Alexis Roy-Lizotte
    email                : roya2 at premiertech dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsalgorithmNearestLocation.h"
#include "qgsprocessing.h"
#include "qgsgeometryengine.h"
#include "qgsvectorlayer.h"
#include "qgsapplication.h"
#include "qgsfeature.h"
#include "qgsfeaturesource.h"

///@cond PRIVATE


void QgsNearestLocationAlgorithm::initAlgorithm( const QVariantMap & )
{
  addParameter( new QgsProcessingParameterFeatureSource( QStringLiteral( "INPUT" ),
                QObject::tr( "Base Layer" ), QList< int > () << QgsProcessing::QgsProcessing::TypeVectorAnyGeometry ) );
  addParameter( new QgsProcessingParameterFeatureSource( QStringLiteral( "JOIN" ),
                QObject::tr( "Target Layer" ), QList< int > () << QgsProcessing::QgsProcessing::TypeVectorAnyGeometry ) );


  predicateParam->setMetadata( predicateMetadata );
  addParameter( predicateParam.release() );
  addParameter( new QgsProcessingParameterField( QStringLiteral( "JOIN_FIELDS" ),
                QObject::tr( "Fields to add (leave empty to use all fields)" ),
                QVariant(), QStringLiteral( "JOIN" ), QgsProcessingParameterField::Any, true, true ) );

  QStringList joinMethods;
  joinMethods << QObject::tr( "Make a feature for each matching feature (one-to-many)" )
              << QObject::tr( "Make a feature for the nearest matching feature only (one-to-one)" );
  addParameter( new QgsProcessingParameterEnum( QStringLiteral( "METHOD" ),
                QObject::tr( "Join type" ),
                joinMethods, false, static_cast< int >( Nearest ) ) );

  QStringList OutputType;
  outputType << QObject::tr( "Point" )
             << QObject::tr( "Line to center" )
             << QObject::tr( "Line to closest point" );
  addParameter( new QgsProcessingParameterEnum( QStringLiteral( "OUTPUTTYPE" ),
                QObject::tr( "Output type" ),
                outputType, false, static_cast< int >( Point ) ) );

  addParameter( new QgsProcessingParameterNumber( QStringLiteral( "NEIGHBORS" ),
                QObject::tr( "Maximum nearest neighbors" ), QgsProcessingParameterNumber::Integer, 1, false, 1 ) );

  addParameter( new QgsProcessingParameterDistance( QStringLiteral( "MAX_DISTANCE" ),
                QObject::tr( "Maximum distance" ), QVariant(), QStringLiteral( "INPUT" ), true, 0 ) );



  addParameter( new QgsProcessingParameterString( QStringLiteral( "PREFIX" ),
                QObject::tr( "Joined field prefix" ), QVariant(), false, true ) );
  addParameter( new QgsProcessingParameterFeatureSink( QStringLiteral( "OUTPUT" ), QObject::tr( "Joined layer" ), QgsProcessing::TypeVectorAnyGeometry, QVariant(), true, true ) );
  addOutput( new QgsProcessingOutputNumber( QStringLiteral( "JOINED_COUNT" ), QObject::tr( "Number of joined features from input table" ) ) );
}

QString QgsNearestLocationAlgorithm::name() const
{
  return QStringLiteral( "joinattributesbylocation" );
}

QString QgsNearestLocationAlgorithm::displayName() const
{
  return QObject::tr( "Join attributes by location" );
}

QStringList QgsNearestLocationAlgorithm::tags() const
{
  return QObject::tr( "join,intersects,intersecting,touching,within,contains,overlaps,relation,spatial" ).split( ',' );
}

QString QgsNearestLocationAlgorithm::group() const
{
  return QObject::tr( "Vector general" );
}

QString QgsNearestLocationAlgorithm::groupId() const
{
  return QStringLiteral( "vectorgeneral" );
}

QString QgsNearestLocationAlgorithm::shortHelpString() const
{
  return QObject::tr( "This algorithm takes an input vector layer and creates a new vector layer "
                      "that is an extended version of the input one, with additional attributes in its attribute table.\n\n"
                      "The additional attributes and their values are taken from a second vector layer. "
                      "A spatial criteria is applied to select the values from the second layer that are added "
                      "to each feature from the first layer in the resulting one." );
}

QString QgsNearestLocationAlgorithm::shortDescription() const
{
  return QObject::tr( "Join attributes from one vector layer to another by location." );
}

QgsNearestLocationAlgorithm *QgsNearestLocationAlgorithm::createInstance() const
{
  return new QgsNearestLocationAlgorithm();
}


QVariantMap QgsNearestLocationAlgorithm::processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  mBaseSource.reset( parameterAsSource( parameters, QStringLiteral( "INPUT" ), context ) );
  if ( !mBaseSource )
    throw QgsProcessingException( invalidSourceError( parameters, QStringLiteral( "INPUT" ) ) );

  mJoinSource.reset( parameterAsSource( parameters, QStringLiteral( "TARGET" ), context ) );
  if ( !mJoinSource )
    throw QgsProcessingException( invalidSourceError( parameters, QStringLiteral( "TARGET" ) ) );

  mJoinMethod = static_cast< JoinMethod >( parameterAsEnum( parameters, QStringLiteral( "METHOD" ), context ) );
  mOutputType = static_cast< OutputType >( parameterAsEnum( parameters, QStringLiteral( "OUTPUTTYPE" ), context ) );

  const QStringList joinedFieldNames = parameterAsFields( parameters, QStringLiteral( "JOIN_FIELDS" ), context );

  mPredicates = parameterAsEnums( parameters, QStringLiteral( "PREDICATE" ), context );
  sortPredicates( mPredicates );

  QString prefix = parameterAsString( parameters, QStringLiteral( "PREFIX" ), context );

  QgsFields joinFields;
  if ( joinedFieldNames.empty() )
  {
    joinFields = mJoinSource->fields();
    mJoinedFieldIndices = joinFields.allAttributesList();
  }
  else
  {
    mJoinedFieldIndices.reserve( joinedFieldNames.count() );
    for ( const QString &field : joinedFieldNames )
    {
      int index = mJoinSource->fields().lookupField( field );
      if ( index >= 0 )
      {
        mJoinedFieldIndices << index;
        joinFields.append( mJoinSource->fields().at( index ) );
      }
    }
  }

  if ( !prefix.isEmpty() )
  {
    for ( int i = 0; i < joinFields.count(); ++i )
    {
      joinFields.rename( i, prefix + joinFields[ i ].name() );
    }
  }

  const QgsFields outputFields = QgsProcessingUtils::combineFields( mBaseSource->fields(), joinFields );

  QString joinedSinkId;
  mJoinedFeatures.reset( parameterAsSink( parameters, QStringLiteral( "OUTPUT" ), context, joinedSinkId, outputFields,
                                          mBaseSource->wkbType(), mBaseSource->sourceCrs(), QgsFeatureSink::RegeneratePrimaryKey ) );

  if ( parameters.value( QStringLiteral( "OUTPUT" ) ).isValid() && !mJoinedFeatures )
    throw QgsProcessingException( invalidSinkError( parameters, QStringLiteral( "OUTPUT" ) ) );

  const double maxDistance = parameters.value( QStringLiteral( "MAX_DISTANCE" ) ).isValid() ? parameterAsDouble( parameters, QStringLiteral( "MAX_DISTANCE" ), context ) : std::numeric_limits< double >::quiet_NaN();


  // make spatial index
  QgsFeatureIterator f2 = input2->getFeatures( QgsFeatureRequest().setDestinationCrs( input->sourceCrs(), context.transformContext() ).setSubsetOfAttributes( fields2Fetch ) );
  QHash< QgsFeatureId, QgsAttributes > input2AttributeCache;
  double step = input2->featureCount() > 0 ? 50.0 / input2->featureCount() : 1;
  int i = 0;
  QgsSpatialIndex index( f2, [&]( const QgsFeature & f )->bool
  {
    i++;
    if ( feedback->isCanceled() )
      return false;

    feedback->setProgress( i * step );

    if ( !f.hasGeometry() )
      return true;

    // only keep selected attributes
    QgsAttributes attributes;
    for ( int j = 0; j < f.attributes().count(); ++j )
    {
      if ( ! fields2Indices.contains( j ) )
        continue;
      attributes << f.attribute( j );
    }
    input2AttributeCache.insert( f.id(), attributes );

    return true;
  }
  , QgsSpatialIndex::FlagStoreFeatureGeometries );

  switch ( mJoinMethod )
  {
    case OneToMany:
    case JoinToFirst:
    {
      if ( mBaseSource->featureCount() > 0 && mJoinSource->featureCount() > 0 && mBaseSource->featureCount() < mJoinSource->featureCount() )
      {
        // joining FEWER features to a layer with MORE features. So we iterate over the FEW features and find matches from the MANY
        processAlgorithmByIteratingOverInputSource( context, feedback );
      }
      else
      {
        // default -- iterate over the join source and match back to the base source. We do this on the assumption that the most common
        // use case is joining a points layer to a polygon layer (taking polygon attributes and adding them to the points), so by iterating
        // over the polygons we can take advantage of prepared geometries for the spatial relationship test.

        // TODO - consider using more heuristics to determine whether it's always best to iterate over the join
        // source.
        processAlgorithmByIteratingOverJoinedSource( context, feedback );
      }
      break;
    }

  }

  QVariantMap outputs;
  if ( mJoinedFeatures )
  {
    outputs.insert( QStringLiteral( "OUTPUT" ), joinedSinkId );
  }


  // need to release sinks to finalize writing
  mJoinedFeatures.reset();

  outputs.insert( QStringLiteral( "JOINED_COUNT" ), static_cast< long long >( mJoinedCount ) );
  return outputs;
}

void QgsNearestLocationAlgorithm::processAlgorithmByIteratingOverJoinedSource( QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  if ( mBaseSource->hasSpatialIndex() == QgsFeatureSource::SpatialIndexNotPresent )
    feedback->reportError( QObject::tr( "No spatial index exists for input layer, performance will be severely degraded" ) );

  QgsFeatureIterator joinIter = mJoinSource->getFeatures( QgsFeatureRequest().setDestinationCrs( mBaseSource->sourceCrs(), context.transformContext() ).setSubsetOfAttributes( mJoinedFieldIndices ) );
  QgsFeature f;

  // Create output vector layer with additional attributes
  const double step = mJoinSource->featureCount() > 0 ? 100.0 / mJoinSource->featureCount() : 1;
  long i = 0;
  while ( joinIter.nextFeature( f ) )
  {
    if ( feedback->isCanceled() )
      break;

    processFeatureFromJoinSource( f, feedback );

    i++;
    feedback->setProgress( i * step );
  }

}

void QgsNearestLocationAlgorithm::processAlgorithmByIteratingOverInputSource( QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  if ( mJoinSource->hasSpatialIndex() == QgsFeatureSource::SpatialIndexNotPresent )
    feedback->reportError( QObject::tr( "No spatial index exists for join layer, performance will be severely degraded" ) );

  QgsFeatureIterator it = mBaseSource->getFeatures();
  QgsFeature f;

  const double step = mBaseSource->featureCount() > 0 ? 100.0 / mBaseSource->featureCount() : 1;
  long i = 0;
  while ( it .nextFeature( f ) )
  {
    if ( feedback->isCanceled() )
      break;

    processFeatureFromInputSource( f, context, feedback );

    i++;
    feedback->setProgress( i * step );
  }
}

bool QgsNearestLocationAlgorithm::processFeatureFromJoinSource( QgsFeature &joinFeature, QgsProcessingFeedback *feedback )
{
  if ( !joinFeature.hasGeometry() )
    return false;

  const QgsGeometry featGeom = joinFeature.geometry();
  std::unique_ptr< QgsGeometryEngine > engine;
  QgsFeatureRequest req = QgsFeatureRequest().setFilterRect( featGeom.boundingBox() );
  QgsFeatureIterator it = mBaseSource->getFeatures( req );
  QList<QgsFeature> filtered;
  QgsFeature baseFeature;
  bool ok = false;
  QgsAttributes joinAttributes;

  while ( it.nextFeature( baseFeature ) )
  {
    if ( feedback->isCanceled() )
      break;

    switch ( mJoinMethod )
    {
      case JoinToFirst:
        if ( mAddedIds.contains( baseFeature.id() ) )
        {
          //  already added this feature, and user has opted to only output first match
          continue;
        }
        break;

      case OneToMany:
        break;

      case JoinToLargestOverlap:
        Q_ASSERT_X( false, "QgsNearestLocationAlgorithm::processFeatureFromJoinSource", "processFeatureFromJoinSource should not be used with join to largest overlap method" );
    }

    if ( !engine )
    {
      engine.reset( QgsGeometry::createGeometryEngine( featGeom.constGet() ) );
      engine->prepareGeometry();
      for ( int ix : qgis::as_const( mJoinedFieldIndices ) )
      {
        joinAttributes.append( joinFeature.attribute( ix ) );
      }
    }
    if ( featureFilter( baseFeature, engine.get(), false ) )
    {
      if ( mJoinedFeatures )
      {
        QgsFeature outputFeature( baseFeature );
        outputFeature.setAttributes( baseFeature.attributes() + joinAttributes );
        mJoinedFeatures->addFeature( outputFeature, QgsFeatureSink::FastInsert );
      }
      if ( !ok )
        ok = true;

      mAddedIds.insert( baseFeature.id() );
      mJoinedCount++;
    }
  }
  return ok;
}

bool QgsNearestLocationAlgorithm::processFeatureFromInputSource( QgsFeature &baseFeature, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  if ( !baseFeature.hasGeometry() )
  {
    // no geometry, treat as if we didn't find a match...
    if ( mJoinedFeatures && !mDiscardNonMatching )
    {
      QgsAttributes emptyAttributes;
      emptyAttributes.reserve( mJoinedFieldIndices.count() );
      for ( int i = 0; i < mJoinedFieldIndices.count(); ++i )
        emptyAttributes << QVariant();

      QgsAttributes attributes = baseFeature.attributes();
      attributes.append( emptyAttributes );
      QgsFeature outputFeature( baseFeature );
      outputFeature.setAttributes( attributes );
      mJoinedFeatures->addFeature( outputFeature, QgsFeatureSink::FastInsert );
    }

    if ( mUnjoinedFeatures )
      mUnjoinedFeatures->addFeature( baseFeature, QgsFeatureSink::FastInsert );

    return false;
  }

  const QgsGeometry featGeom = baseFeature.geometry();
  std::unique_ptr< QgsGeometryEngine > engine;
  QgsFeatureRequest req = QgsFeatureRequest().setDestinationCrs( mBaseSource->sourceCrs(), context.transformContext() ).setFilterRect( featGeom.boundingBox() ).setSubsetOfAttributes( mJoinedFieldIndices );

  QgsFeatureIterator it = mJoinSource->getFeatures( req );
  QList<QgsFeature> filtered;
  QgsFeature joinFeature;
  bool ok = false;

  double largestOverlap  = std::numeric_limits< double >::lowest();
  QgsFeature bestMatch;

  while ( it.nextFeature( joinFeature ) )
  {
    if ( feedback->isCanceled() )
      break;

    if ( !engine )
    {
      engine.reset( QgsGeometry::createGeometryEngine( featGeom.constGet() ) );
      engine->prepareGeometry();
    }

    if ( featureFilter( joinFeature, engine.get(), true ) )
    {
      switch ( mJoinMethod )
      {
        case JoinToFirst:
        case OneToMany:
          if ( mJoinedFeatures )
          {
            QgsAttributes joinAttributes = baseFeature.attributes();
            joinAttributes.reserve( joinAttributes.size() + mJoinedFieldIndices.size() );
            for ( int ix : qgis::as_const( mJoinedFieldIndices ) )
            {
              joinAttributes.append( joinFeature.attribute( ix ) );
            }

            QgsFeature outputFeature( baseFeature );
            outputFeature.setAttributes( joinAttributes );
            mJoinedFeatures->addFeature( outputFeature, QgsFeatureSink::FastInsert );
          }
          break;

      }

      ok = true;

      if ( mJoinMethod == JoinToFirst )
        break;
    }
  }

  mJoinedCount++;

  return ok;
}


///@endcond



