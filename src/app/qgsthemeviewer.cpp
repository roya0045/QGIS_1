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
#include "qgsmaplayerstyle.h"
#include "qgslayertree.h"
#include "qgslayertreelayer.h"
#include "qgslayertreemodellegendnode.h"
#include "qgsmaplayerstylemanager.h"
#include "qstyleditemdelegate.h"
#include "qgsvectorlayer.h"
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
  connect( this, &QgsThemeModel::dataChanged, this, &QgsThemeModel::resyncTheme );
}

QgsThemeModel::QgsThemeModel( QgsLayerTree *rootNode )
  : QgsLayerTreeModel( rootNode )
{
  setFlag( QgsLayerTreeModel::AllowLegendChangeState, false );
  setFlag( QgsLayerTreeModel::AllowNodeReorder, true );
  connect( this, &QgsThemeModel::dataChanged, this, &QgsThemeModel::resyncTheme );
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

void QgsThemeModel::resyncTheme()
{
  
}

QgsThemeViewer::QgsThemeViewer( QWidget *parent )
  : QgsLayerTreeView( parent )
{

}

void QgsThemeViewer::setModel( QgsLayerTreeModel *model )
{
  mModel = new QgsThemeModel( model->rootGroup(), this );
  if ( !mModel )
    return;

  mProxyModel = new QgsThemeProxy( mModel, this );
  disconnectProxyModel();
  //connect( mProxyModel, &QAbstractItemModel::rowsInserted, this, &QgsLayerTreeView::modelRowsInserted );
 // connect( mProxyModel, &QAbstractItemModel::rowsRemoved, this, &QgsLayerTreeView::modelRowsRemoved );

  mProxyModel->setShowPrivateLayers( true );
  QTreeView::setModel( mProxyModel );
  setItemDelegate( new QgsThemeViewerDelegate( this ) );

  //connect( mModel->rootGroup(), &QgsLayerTreeNode::expandedChanged, this, &QgsThemeViewer::onExpandedChanged );
  //connect( treeModel->rootGroup(), &QgsLayerTreeNode::customPropertyChanged, this, &QgsThemeViewer::onCustomPropertyChanged );

  connect( selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsThemeViewer::onCurrentChanged );

  connect( mModel, &QAbstractItemModel::modelReset, this, &QgsThemeViewer::onModelReset );

  connect( mModel, &QAbstractItemModel::dataChanged, this, &QgsThemeViewer::onDataChanged );

  //updateExpandedStateFromNode( mModel->rootGroup() );

  //checkModel();
}

QgsLayerTreeNode *QgsThemeViewer::index2node( const QModelIndex &index ) const
{
  QModelIndex idx= mProxyModel->mapToSource( index );
  if ( idx.isValid() )
    return layerTreeModel()->index2node( idx );
  return nullptr;
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

void QgsThemeViewer::setProxyMapTheme( QgsMapThemeCollection::MapThemeRecord *theme, const QMap<QString, QString> styles )
{
  if ( mProxyModel )
    mProxyModel->setMapTheme( theme, styles );
}


void QgsThemeViewer::showAllNodes( bool show )
{
  if ( mProxyModel )
    mProxyModel->setShowAllNodes( show );
}


void QgsThemeViewer::disconnectProxyModel()
{
  if ( mProxyModel )
  {
    disconnect( mProxyModel, &QAbstractItemModel::rowsInserted, this, &QgsThemeViewer::modelRowsInserted );
    disconnect( mProxyModel, &QAbstractItemModel::rowsRemoved, this, &QgsThemeViewer::modelRowsRemoved );
  }
}

void QgsThemeViewer::onExpandedChanged( QgsLayerTreeNode *node, bool expanded )
{

}


QgsThemeProxy::QgsThemeProxy( QgsThemeModel *treeModel, QObject *parent )
  : QgsLayerTreeProxyModel( treeModel, parent )
{
  mLayerTreeModel = treeModel;
}


void QgsThemeProxy::setShowAllNodes( bool show )
{
  mShowAllNodes = show;
  // invalidateFilter();
}

void QgsThemeProxy::setMapTheme( QgsMapThemeCollection::MapThemeRecord *theme, const QMap<QString, QString> styles )
{
  if ( theme )
    mTheme = theme;
  // mLayerTreeModel->setLayerStyleOverrides( styles );
  invalidateFilter();
}

bool QgsThemeProxy::filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const
{
  return true;
  if (QgsLayerTreeNode *node = mLayerTreeModel->index2node( mLayerTreeModel->index( sourceRow, 0, sourceParent ) ) )
    return nodeShown( node );
  else if ( mShowAllNodes )
  {
    if ( node->parent() && QgsLayerTree::isLayer( node->parent() ) )
    {
      if ( QgsMapLayer *mlayer = QgsLayerTree::toLayer( node->parent() )->layer() )
      {
        const QgsMapThemeCollection::MapThemeLayerRecord lrecord = mTheme->getRecord( mlayer->id() );
        return lrecord.currentStyle == mlayer->styleManager()->currentStyle();
      }
    }
    else
      return true;
  }
  else if ( QgsLayerTreeModelLegendNode *legendNode = mLayerTreeModel->index2legendNode( mLayerTreeModel->index( sourceRow, 0, sourceParent ) ) )
    return legendNodeShown( legendNode );
  return false;

}

bool QgsThemeProxy::nodeShown( QgsLayerTreeNode *node ) const
{
  if ( !node ) //other node
    return mShowAllNodes;

  if ( node->nodeType() == QgsLayerTreeNode::NodeGroup )
  {
    if (  mTheme && !mShowAllNodes ) //torework
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
    if ( !mTheme || !mTheme->hasLayer( layer->id() ) ) //torework
      return false;
    return true;
  }
}

bool QgsThemeProxy::legendNodeShown( QgsLayerTreeModelLegendNode *node ) const
{
  if ( !mTheme || !mTheme->hasLayer( node->layerNode()->layer()->id() ) ) //torework
    return false;
  const QgsMapThemeCollection::MapThemeLayerRecord lrecord = mTheme->getRecord( node->layerNode()->layer()->id() );
  if( ! lrecord.usingLegendItems )
    return nodeShown( node->layerNode() );
  else if ( lrecord.checkedLegendItems.contains( node->data( static_cast< int >(QgsLayerTreeModelLegendNode::CustomRole::RuleKey) ).toString() ) ) //applyMapThemeCheckedLegendNodesToLayer
   return true;

  return mShowAllNodes;
}

QModelIndex QgsThemeProxy::mapToSource( const QModelIndex idx ) const
{
    return QModelIndex();
}

QgsThemeViewerDelegate::QgsThemeViewerDelegate( QgsThemeViewer *parent )
  : QStyledItemDelegate( parent )
  ,  mThemeViewer( parent )
{
  connect( mThemeViewer, &QgsLayerTreeView::clicked, this, &QgsThemeViewerDelegate::onClicked );
}


void QgsThemeViewerDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  QStyledItemDelegate::paint( painter, option, index );

  QgsLayerTreeNode *node = mThemeViewer->index2node( index );
  if ( !node )
    return;

  QStyleOptionViewItem opt = option;
  initStyleOption( &opt, index );

  const QColor baseColor = opt.palette.base().color();
  const QRect tRect = mThemeViewer->style()->subElementRect( QStyle::SE_ItemViewItemText, &opt, mThemeViewer );

  const bool shouldShowLayerMark = tRect.left() < 0;  // Layer/group node icon not visible anymore?
  if ( shouldShowLayerMark )
  {
    const int tPadding = tRect.height() / 10;
    const QRect mRect( mThemeViewer->viewport()->rect().right() - mThemeViewer->layerMarkWidth(), tRect.top() + tPadding, mThemeViewer->layerMarkWidth(), tRect.height() - tPadding * 2 );
    const QBrush pb = painter->brush();
    const QPen pp = painter->pen();
    painter->setPen( QPen( Qt::NoPen ) );
    QBrush b = QBrush( opt.palette.mid() );
    QColor bc = b.color();
    // mix mid color with base color for a less dominant, yet still opaque, version of the color
    bc.setRed( static_cast< int >( bc.red() * 0.3 + baseColor.red() * 0.7 ) );
    bc.setGreen( static_cast< int >( bc.green() * 0.3 + baseColor.green() * 0.7 ) );
    bc.setBlue( static_cast< int >( bc.blue() * 0.3 + baseColor.blue() * 0.7 ) );
    b.setColor( bc );
    painter->setBrush( b );
    painter->drawRect( mRect );
    painter->setBrush( pb );
    painter->setPen( pp );
  }
}

static void _fixStyleOption( QStyleOptionViewItem &opt )
{
  // This makes sure our delegate behaves correctly across different styles. Unfortunately there is inconsistency
  // in how QStyleOptionViewItem::showDecorationSelected is prepared for paint() vs what is returned from view's viewOptions():
  // - viewOptions() returns it based on style's SH_ItemView_ShowDecorationSelected hint
  // - for paint() there is extra call to QTreeViewPrivate::adjustViewOptionsForIndex() which makes it
  //   always true if view's selection behavior is SelectRows (which is the default and our case with layer tree view)
  // So for consistency between different calls we override it to what we get in paint() method ... phew!
  opt.showDecorationSelected = true;
}

bool QgsThemeViewerDelegate::helpEvent( QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index )
{
  return QStyledItemDelegate::helpEvent( event, view, option, index );
}


void QgsThemeViewerDelegate::onClicked( const QModelIndex &index )
{

}

/// @endcond

