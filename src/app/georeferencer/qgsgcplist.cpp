/***************************************************************************
     qgsgeorefconfigdialog.h
     --------------------------------------
    Date                 : 14-Feb-2010
    Copyright            : (C) 2010 by Jack R, Maxim Dubinin (GIS-Lab)
    Email                : sim@gis-lab.info
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgspointxy.h"
#include "qgsgeorefdatapoint.h"
#include "qgscoordinatereferencesystem.h"
#include "qgscoordinatetransform.h"
#include "qgsproject.h"

#include "qgsgcplist.h"

void QgsGCPList::createGCPVectors( QVector<QgsPointXY> &sourceCoordinates, QVector<QgsPointXY> &destinationCoordinates, const QgsCoordinateReferenceSystem &targetCrs )
{
  const int targetSize = countEnabledPoints();
  sourceCoordinates.clear();
  sourceCoordinates.reserve( targetSize );
  destinationCoordinates.clear();
  destinationCoordinates.reserve( targetSize );

  for ( QgsGeorefDataPoint *pt : std::as_const( *this ) )
  {
    if ( !pt->isEnabled() )
      continue;

    sourceCoordinates.push_back( pt->sourcePoint() );
    if ( targetCrs.isValid() )
    {
      try
      {
        QgsPointXY transCoords = QgsCoordinateTransform( pt->destinationPointCrs(), targetCrs,
                                 QgsProject::instance() ).transform( pt->destinationPoint() );
        destinationCoordinates.push_back( transCoords );
        pt->setTransCoords( transCoords );
      }
      catch ( const QgsException & )
      {
        destinationCoordinates.push_back( pt->destinationPoint() );
      }
    }
    else
      destinationCoordinates.push_back( pt->destinationPoint() );
  }
}

int QgsGCPList::countEnabledPoints() const
{
  if ( isEmpty() )
    return 0;

  int s = 0;
  const_iterator it = begin();
  while ( it != end() )
  {
    if ( ( *it )->isEnabled() )
      s++;
    ++it;
  }
  return s;
}

QList<QgsGcpPoint> QgsGCPList::asPoints() const
{
  QList<QgsGcpPoint> res;
  res.reserve( size() );
  for ( QgsGeorefDataPoint *pt : *this )
  {
    res.append( QgsGcpPoint( pt->point() ) );
  }
  return res;
}
