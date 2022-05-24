/***************************************************************************
  qgsthememanagerwidget.cpp
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

#include "qgsthememanagerwidget.h"
#include "qgsdockwidget.h"
#include "qgsproject.h"
#include "qgsmaplayer.h"
#include "qgsmapthemecollection.h"
#include "qgslayertreemodel.h"
#include "qgslayertree.h"
#include "qgslayertreeview.h"
#include "qgslayertreenode.h"
#include "qgslayertreegroup.h"
#include "qgsthemeviewer.h"
#include "qgsmapthemes.h"
#include <QMessageBox>
#include <QWidget>
#include <QToolButton>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include "qgisapp.h"

static QString _groupId( QgsLayerTreeNode *node )
{
  QStringList lst;
  while ( node->parent() )
  {
    lst.prepend( node->name() );
    node = node->parent();
  }
  return lst.join( '/' );
}


QgsThemeManagerWidget::QgsThemeManagerWidget( QWidget *parent )
  : QgsDockWidget( parent )
{
  setupUi( this );
  connect( this, &QgsDockWidget::opened, this, &QgsThemeManagerWidget::showWidget );
  connect( QgsProject::instance(), &QgsProject::readProject, this, &QgsThemeManagerWidget::projectLoaded );
  connect( mThemePrev, &QToolButton::clicked, this, &QgsThemeManagerWidget::previousTheme );
  connect( mThemeNext, &QToolButton::clicked, this, &QgsThemeManagerWidget::nextTheme );
  connect( mCreateTheme, &QToolButton::clicked, this, &QgsThemeManagerWidget::createTheme );
  connect( mDeleteTheme, &QToolButton::clicked, this, &QgsThemeManagerWidget::removeTheme );
  connect( mAddThemeLayer, &QToolButton::clicked, this, &QgsThemeManagerWidget::addSelectedNodes );
  connect( mRemoveThemeLayer, &QToolButton::clicked, this, &QgsThemeManagerWidget::removeSelectedNodes );
  connect( mThemeList, qOverload<int>( &QComboBox::activated ), [ = ]( int index ) { setTheme( index ); } );
  connect( mThemeViewer, &QgsThemeViewer::layersAdded, this, &QgsThemeManagerWidget::addSelectedNodes );
  connect( mThemeViewer, &QgsThemeViewer::showMenu, this, &QgsThemeManagerWidget::showContextMenu );
  //connect( mThemeViewer, &QgsThemeViewer::layersDropped, this, &QgsThemeManagerWidget::removeSelectedNodes );
  connect( this, &QgsThemeManagerWidget::droppedLayers, this, &QgsThemeManagerWidget::removeSelectedNodes );
  connect( this, &QgsThemeManagerWidget::addLayerTreeLayers, this, &QgsThemeManagerWidget::addSelectedNodes );
}

void QgsThemeManagerWidget::projectLoaded()
{
  mThemeCollection = QgsProject::instance()->mapThemeCollection();
  connect( mThemeCollection, &QgsMapThemeCollection::mapThemesChanged, this, &QgsThemeManagerWidget::populateCombo, Qt::UniqueConnection );
  QgsLayerTreeModel *mModel = QgisApp::instance()->layerTreeView()->layerTreeModel();
  if ( mThemeViewer && mModel )
  {
    mThemeViewer->setModel( mModel );
    mThemeViewer->disconnectProxyModel();
    mThemeViewer->showAllNodes( mShowAllLayers );
  }
  if ( mThemeCollection && mThemeCollection->mapThemes().length() > 0 )
  {
    if ( !mThemeCollection->hasMapTheme( mCurrentTheme ) )
    {
      mCurrentTheme = mThemeCollection->mapThemes()[0];
      mThemeList->setCurrentText( mCurrentTheme );
    }
    populateCombo();
    viewCurrentTheme();
  }
}

void QgsThemeManagerWidget::showWidget()
{
  if ( mCurrentTheme.isEmpty() )
    projectLoaded();
}

void QgsThemeManagerWidget::setTheme( const int index )
{
  QString themename;
  if ( index > -1 && mThemeCollection->mapThemes().size() > index )
  {
    QString themename = mThemeCollection->mapThemes().at( index );
    if ( !mThemeCollection->hasMapTheme( themename ) )
      return;
    mCurrentTheme = themename;
    mThemeViewer->setProxyMapThemeRecord( mThemeCollection->mapThemeState( themename ) );
    emit updateComboBox();
  }
}

void QgsThemeManagerWidget::createTheme()
{
  QgsMapThemes::instance()->addPreset();
  emit updateComboBox();
}

void QgsThemeManagerWidget::removeTheme()
{
  int res = QMessageBox::question( QgisApp::instance(), tr( "Remove Theme" ),
                                   tr( "Are you sure you want to remove the existing theme “%1”?" ).arg( mCurrentTheme ),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No );  if ( res == QMessageBox::Yes )
  {
    mThemeCollection->removeMapTheme( mCurrentTheme );
    mCurrentTheme = mThemeCollection->mapThemes()[0];
    emit updateComboBox();
  }

}

void QgsThemeManagerWidget::previousTheme()
{
  QStringList themes = mThemeCollection->mapThemes();
  int idx = themes.indexOf( mCurrentTheme );
  if ( idx > 0 )
  {
    mCurrentTheme = themes.at( idx - 1 );
    emit updateComboBox();
  }
}

void QgsThemeManagerWidget::nextTheme()
{
  QStringList themes = mThemeCollection->mapThemes();
  int idx = 1 + themes.indexOf( mCurrentTheme );
  if ( idx < themes.size() )
  {
    mCurrentTheme = themes.at( idx );
    emit updateComboBox();
  }
}

void QgsThemeManagerWidget::populateCombo()
{
  mThemeList->clear();
  QStringList themes = QgsProject::instance()->mapThemeCollection()->mapThemes();
  if ( !themes.isEmpty() )
  {
    mThemeList->addItems( themes );
    if ( themes.contains( mCurrentTheme ) )
      mThemeList->setCurrentText( mCurrentTheme );
  }

}

void QgsThemeManagerWidget::updateComboBox()
{
  mThemeList->setCurrentText( mCurrentTheme );
  viewCurrentTheme();
}

void QgsThemeManagerWidget::viewCurrentTheme() const
{
  if ( !mThemeCollection->hasMapTheme( mCurrentTheme ) )
    return;
  mThemeViewer->proxyModel()->setMapTheme( mThemeCollection->mapThemeState( mCurrentTheme ) );
}


void QgsThemeManagerWidget::appendNodes( const QModelIndexList indexes )
{
  if ( !mThemeCollection->hasMapTheme( mCurrentTheme ) )
    return;

  QgsLayerTreeView *appTreeView = QgisApp::instance()->layerTreeView();
  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection->mapThemeState( mCurrentTheme );
  for ( QModelIndex index : std::as_const( indexes ) )
  {
    if ( QgsLayerTreeNode *node = appTreeView->index2node( index ) )
    {
      if ( QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( node ) )
      {
        if ( QgsMapLayer *layer = nodeLayer->layer() )
        {
          QgsMapThemeCollection::MapThemeLayerRecord newRecord( layer );
          theme.addLayerRecord( newRecord );
        }
      }
    }
    else if ( QgsLayerTreeModelLegendNode *legendNode = appTreeView->index2legendNode( index ) )
    {
      if ( legendNode->layerNode() &&
           ! indexes.contains( appTreeView->node2index( legendNode->layerNode() ) ) &&
           QgsLayerTree::isLayer( legendNode->layerNode() ) )
      {
        if ( QgsMapLayer *mapLayer = QgsLayerTree::toLayer( legendNode->layerNode() )->layer() )
        {
          QgsMapThemeCollection::MapThemeLayerRecord modRecord;
          QString layerId = mapLayer->id();
          if ( theme.hasLayer( layerId ) )
          {
            const QgsMapThemeCollection::MapThemeLayerRecord lrecord = theme.getRecord( layerId );
            modRecord = lrecord;
          }
          else
          {
            modRecord = QgsMapThemeCollection::createThemeLayerRecord( legendNode->layerNode(), QgisApp::instance()->layerTreeView()->layerTreeModel() );
            modRecord.usingLegendItems = true;
            modRecord.checkedLegendItems.clear();
          }
          QString legendId= legendNode->data( QgsLayerTreeModelLegendNode::RuleKeyRole ).toString();
          // remove node to registry
          modRecord.checkedLegendItems.insert( legendId );
          theme.addLayerRecord( modRecord );
        }
      }
    }
    else if ( QgsLayerTree::isGroup( node ) )
    {
      QSet<QString> checkedGroups = theme.checkedGroupNodes();
      checkedGroups.insert( _groupId( node ) );
      theme.setCheckedGroupNodes( checkedGroups );
    }
  }
  mThemeCollection->update( mCurrentTheme, theme );
  viewCurrentTheme();
}

void QgsThemeManagerWidget::addSelectedNodes()
{
  const QModelIndexList indexes = QgisApp::instance()->layerTreeView()->selectedTreeIndexes();
  appendNodes( indexes );
}

void QgsThemeManagerWidget::removeSelectedNodes()
{
  const QModelIndexList indexes = mThemeViewer->selectedTreeIndexes();
  removeThemeNodes( indexes );
}

void QgsThemeManagerWidget::removeThemeNodes(  const QModelIndexList indexes )
{
  if ( !mThemeCollection->hasMapTheme( mCurrentTheme ) )
    return;
  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection->mapThemeState( mCurrentTheme );

  for ( QModelIndex index : std::as_const( indexes ) )
  {
    if ( QgsLayerTreeNode *node = mThemeViewer->index2node( index ) )
    {
      if ( QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( node ) )
      {
        if ( QgsMapLayer *layer = nodeLayer->layer() )
        {
          theme.removeLayerRecord( layer );
        }
      }
    }
    else if ( QgsLayerTreeModelLegendNode *legendNode = mThemeViewer->index2legendNode( index ) )
    {
      if ( legendNode->layerNode() &&
           ! indexes.contains( mThemeViewer->node2index( legendNode->layerNode() ) ) &&
           QgsLayerTree::isLayer( legendNode->layerNode() ) )
      {
        if ( QgsMapLayer *mapLayer = QgsLayerTree::toLayer( legendNode->layerNode() )->layer() )
        {
          if ( !theme.hasLayer( mapLayer->id() ) )
            continue;
          const QgsMapThemeCollection::MapThemeLayerRecord lrecord = theme.getRecord( mapLayer->id() );
          QgsMapThemeCollection::MapThemeLayerRecord modRecord = lrecord;
          QString legendId= legendNode->data( QgsLayerTreeModelLegendNode::RuleKeyRole ).toString();
          // remove node to registry
          if ( modRecord.checkedLegendItems.remove( legendId ) )
            theme.addLayerRecord( modRecord );
        }
      }
    }
    else if ( QgsLayerTree::isGroup( node ) )
    {
      QSet<QString> checkedGroups = theme.checkedGroupNodes();
      checkedGroups.remove( _groupId( node ) );
      theme.setCheckedGroupNodes( checkedGroups );
    }
  }
  mThemeCollection->update( mCurrentTheme, theme );
  viewCurrentTheme();
}

void QgsThemeManagerWidget::changeVisibility()
{
  mShowAllLayers = !mShowAllLayers;
  if ( mThemeViewer )
    mThemeViewer->showAllNodes( mShowAllLayers );
}

void QgsThemeManagerWidget::showContextMenu( const QPoint &pos )
{
  QMenu *menu = new QMenu();
  QAction *hideToggle = new QAction( tr( "Display layers only" ), this );
  hideToggle->setCheckable( true );
  hideToggle->setChecked( !mShowAllLayers );
  menu->addAction( hideToggle );
  connect( hideToggle, &QAction::triggered, this, [ = ]() { changeVisibility(); } );
  menu->exec( mapToGlobal( pos ) );
  delete menu;
}



