/***************************************************************************
  qgsthemeviewer.cpp
  --------------------------------------
  Date                 : April 2021
  Copyright            : (C) 2021 by Alex RL
  Email                : ping me on github
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsthemeviewer.h"
#include "qgsmaplayer.h"
#include "qgslayertreemodel.h"
#include "qgsmessagebar.h"
#include "qgsmaplayer.h"
#include "qgsmapstyle.h"
#include "qgslayertree.h"
#include "qgslayertreelayer.h"
#include "qgslayertreemodellegendnode.h"
#include "qgsmaplayerstylemanager.h"
#include <QDrag>
#include <QDragEnterEvent>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QWidget>
#include <QMimeData>
#include <QSignalBlocker>
#include <QCoreApplication>


QgsThemeModel::QgsThemeModel( QgsLayerTree *rootNode, QObject *parent)
  : QgsLayerTreeModel( rootNode, parent )
{
  setFlag( QgsLayerTreeModel::AllowLegendChangeState, false );
  setFlag( QgsLayerTreeModel::AllowNodeReorder, true );
  connect( this, &QgsThemeModel::dataChanged, this, &QgsThemeModel::refreshLegend );
}

QgsThemeModel::QgsThemeModel( QgsLayerTree *rootNode )
  : QgsLayerTreeModel( rootNode )
{
  setFlag( QgsLayerTreeModel::AllowLegendChangeState, false );
  setFlag( QgsLayerTreeModel::AllowNodeReorder, true );
  connect( this, &QgsThemeModel::dataChanged, this, &QgsThemeModel::refreshLegend );
}

QVariant QgsThemeModel::data( const QModelIndex &index, int role ) const
{
  // handle custom layer node labels

  QgsLayerTreeNode *node = index2node( index );
  QgsLayerTreeLayer *nodeLayer = QgsLayerTree::isLayer( node ) ? QgsLayerTree::toLayer( node ) : nullptr;
  if ( nodeLayer && ( role == Qt::DisplayRole || role == Qt::EditRole ) )
  {

    QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( nodeLayer->layer() );

  }
  return QgsLayerTreeModel::data( index, role );
}

Qt::ItemFlags QgsThemeModel::flags( const QModelIndex &index ) const
{
  // make the legend nodes selectable even if they are not by default
  if ( index2legendNode( index ) )
    return QgsLayerTreeModel::flags( index ) | Qt::ItemIsSelectable;

  return QgsLayerTreeModel::flags( index );
}

QList<QgsLayerTreeModelLegendNode *> QgsThemeModel::layerLegendNodes( QgsLayerTreeLayer *nodeLayer, bool skipNodeEmbeddedInParent ) const
{
  if ( !mLegend.contains( nodeLayer ) )
    return QList<QgsLayerTreeModelLegendNode *>();

  const LayerLegendData &data = mLegend[nodeLayer];
  QList<QgsLayerTreeModelLegendNode *> lst( data.activeNodes );
  if ( !skipNodeEmbeddedInParent && data.embeddedNodeInParent )
    lst.prepend( data.embeddedNodeInParent );
  return lst;
}

void QgsThemeModel::clearCachedData( QgsLayerTreeNode *node ) const
{
  node->removeCustomProperty( QStringLiteral( "cached_name" ) );
}

void QgsThemeModel::forceRefresh()
{
  emit refreshTheme();
}

void QgsThemeModel::validateTheme( QString theme )
{
  QMap<QgsMapLayer *, QgsMapStyle> themeStyle;
  QMap<QgsMapLayer *, QgsMapStyle>  currentStyle;
  for layer 
      if ( QgsMapLayer *layer = QgsLayerTree::toLayer( node->parent() )->layer() )
      {
        QString layerid = layer->id();
        const QgsMapThemeCollection::MapThemeLayerRecord lrecord = mTheme.getRecord( layerid );
        if ( lrecord.currentStyle != layer->styleManager()->currentStyle() )
        {
          themeStyle.insert( layer, lrecord.currentStyle );
          currentStyle.insert( layer, layer->styleManager()->currentStyle() );
        }
      }
  QMap<QString, QgsMapStyle>::const_iterator i;
  for ( i = themeStyle.constBegin(); i != themeStyle.constEnd() ; ++i )
  {
    QgsMapLayer *layer = i.key();
    if ( layer )
      layer->styleManager()->setCurrentStyle( i.value() );
  }

  forceRefresh();

  QMap<QString, QgsMapStyle>::const_iterator i;
  for ( i = currentStyle.constBegin(); i != currentStyle.constEnd() ; ++i )
  {
    QgsMapLayer *layer = i.key();
    if ( layer )
      layer->styleManager()->setCurrentStyle( i.value() );
  }
}

void QgsThemeModel::changeLayerStyle( QgsLayerTreeNode *node, QString theme )
{
  if ( !QgsLayerTree::isLayer( node ) )
    return;
    const QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( node );
  if ( !nodeLayer->layer() )
    return;
  const QgsMapLayerStyle lStyle = mTheme.getRecord( nodeLayer->layer()->id() ).currentStyle;

}

QgsThemeViewer::QgsThemeViewer( QWidget *parent )
  : QgsLayerTreeView( parent )
{
}

void QgsThemeViewer::setModel( QAbstractItemModel *model )
{
  QgsThemeModel *treeModel = qobject_cast<QgsThemeModel *>( model );
  if ( !treeModel )
    return;

  mProxyModel = new QgsThemeProxy( treeModel, this );

  //connect( mProxyModel, &QAbstractItemModel::rowsInserted, this, &QgsLayerTreeView::modelRowsInserted );
 // connect( mProxyModel, &QAbstractItemModel::rowsRemoved, this, &QgsLayerTreeView::modelRowsRemoved );

  mProxyModel->setShowPrivateLayers( true );
  QTreeView::setModel( mProxyModel );

  connect( treeModel->rootGroup(), &QgsLayerTreeNode::expandedChanged, this, &QgsThemeViewer::onExpandedChanged );
  //connect( treeModel->rootGroup(), &QgsLayerTreeNode::customPropertyChanged, this, &QgsThemeViewer::onCustomPropertyChanged );

  connect( selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsThemeViewer::onCurrentChanged );

  connect( treeModel, &QAbstractItemModel::modelReset, this, &QgsThemeViewer::onModelReset );

  connect( treeModel, &QAbstractItemModel::dataChanged, this, &QgsThemeViewer::onDataChanged );

  updateExpandedStateFromNode( treeModel->rootGroup() );

  //checkModel();
}

QStringList QgsThemeViewer::mimeTypes() const
{
  QStringList types;
  types << QStringLiteral( "application/qgis.thememanagerdata" );
  return types;
}

void QgsThemeViewer::dragEnterEvent( QDragEnterEvent *event )
{
  if ( event->mimeData()->hasFormat( QStringLiteral( "application/qgis.layertreemodeldata" ) ) )
    event->acceptProposedAction();
}

void QgsThemeViewer::dropEvent( QDropEvent *event )
{
  if ( event->mimeData()->hasFormat( QStringLiteral( "application/qgis.layertreemodeldata" ) ) )
    emit layersAdded();

}

Qt::DropActions QgsThemeViewer::supportedDropActions() const
{
  return Qt::CopyAction | Qt::LinkAction;
}


QMimeData *QgsThemeViewer::mimeData() const
{
  QMimeData *mimeData = new QMimeData();
  //QList<QgsLayerTreeNode *> &nodes = selectedTreeIndexes();
  //QList<QgsLayerTreeNode *> nodesFinal = indexes2nodes( sortedIndexes, true );

  //if ( layers.isEmpty() )
  //  return mimeData;

//  QStringList ids;
//  for ( QgsLayerTreeNode *node : std::as_const( nodes ) )
//  {
//    QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( parentNode );
//    if ( QgsMapLayer *layer = nodeLayer->layer() )
//      ids << layer->id().toUtf8();
//  }

//  mimeData->setData( QStringLiteral( "application/qgis.thememanagerdata" ), ids.join( "+" ).toUtf8() );
//  mimeData->setData( QStringLiteral( "application/qgis.application.pid" ), QString::number( QCoreApplication::applicationPid() ).toUtf8() );

  return mimeData;
}


void QgsThemeViewer::startDrag( Qt::DropActions )
{
  QMimeData *mimeDat = mimeData();
  QDrag *drag = new QDrag( this );
  drag->setMimeData( mimeDat );

  if ( !( drag->target() == this ) )
    emit layersDropped();

}

void QgsThemeViewer::contextMenuEvent( QContextMenuEvent *event )
{
  emit showMenu( event->pos() );
}


void QgsThemeViewer::showAllNodes( bool show )
{
  mProxyModel->setShowAllNodes( show );
}


void QgsThemeViewer::disconnectProxyModel()
{
  disconnect( mProxyModel, &QAbstractItemModel::rowsInserted, this, &QgsThemeViewer::modelRowsInserted );
  disconnect( mProxyModel, &QAbstractItemModel::rowsRemoved, this, &QgsThemeViewer::modelRowsRemoved );
}

void QgsThemeViewer::setProxyMapThemeRecord( const QgsMapThemeCollection::MapThemeRecord theme )
{
  mProxyModel->setMapTheme( theme );
}

QgsThemeProxy::QgsThemeProxy( QgsLayerTreeModel *treeModel, QObject *parent )
  : QgsLayerTreeProxyModel( treeModel, parent )
{
  mLayerTreeModel = treeModel;
}


void QgsThemeProxy::setShowAllNodes( bool show )
{
  mShowAllNodes = show;
  invalidateFilter();
}

void QgsThemeProxy::setMapTheme( const QgsMapThemeCollection::MapThemeRecord theme )
{
  mTheme = theme;
  invalidateFilter();
}

bool QgsThemeProxy::filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const
{
  if (QgsLayerTreeNode *node = mLayerTreeModel->index2node( mLayerTreeModel->index( sourceRow, 0, sourceParent ) ) )
    return nodeShown( node );
  else if ( mShowAllNodes )
  {
    if ( node->parent() && QgsLayerTree::isLayer( node->parent() ) )
    {
      if ( QgsMapLayer *mlayer = QgsLayerTree::toLayer( node->parent() )->layer() )
      {
        const QgsMapThemeCollection::MapThemeLayerRecord lrecord = mTheme.getRecord( mlayer->id() );
        return lrecord.currentStyle == mlayer->styleManager()->currentStyle();
      }
    }
    else
      return true;
  }
  else if ( QgsLayerTreeModelLegendNode *legendNode = mLayerTreeModel->index2legendNode( mLayerTreeModel->index( sourceRow, 0, sourceParent ) ) )
    return legendNodeShown( legendNode );
  else
    return false;

}

bool QgsThemeProxy::nodeShown( QgsLayerTreeNode *node ) const
{
  if ( !node ) //other node
    return mShowAllNodes;

  if ( node->nodeType() == QgsLayerTreeNode::NodeGroup )
  {
    if ( !mShowAllNodes && !mHasTheme ) //torework
    {
      QList <QgsLayerTreeNode *> children = node->children();
      QList <QgsLayerTreeNode *>::const_iterator i;
      for ( i = children.constBegin(); i != children.constEnd() ; ++i )
      {
        if ( nodeShown( *i ) )
          return true;
      }
      return false;
    }
    else
      return true;
  }
  else
  {
    QgsMapLayer *layer = QgsLayerTree::toLayer( node )->layer();
    if ( !layer )
      return mShowAllNodes;
    if ( !mHasTheme && !mTheme.hasLayer( layer->id() ) ) //torework
      return false;
    return true;
  }
}

bool QgsThemeProxy::legendNodeShown( QgsLayerTreeModelLegendNode *node ) const
{
  const QgsMapThemeCollection::MapThemeLayerRecord lrecord = mTheme.getRecord( node->layerNode()->layer()->id() );
  if( ! lrecord.usingLegendItems )
    return nodeShown( node->layerNode() );
  else if ( lrecord.checkedLegendItems.contains( node->data( QgsLayerTreeModelLegendNode::RuleKeyRole ).toString() ) ) //applyMapThemeCheckedLegendNodesToLayer
   return true;

  return mShowAllNodes;
}


