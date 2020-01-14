/***************************************************************************
  qgslayertreelayer.cpp
  --------------------------------------
  Date                 : May 2014
  Copyright            : (C) 2014 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslayertreelayer.h"

#include "qgslayertreeutils.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsrulebasedrenderer.h"
#include "qgsvectorlayer.h"


QgsLayerTreeLayer::QgsLayerTreeLayer( QgsMapLayer *layer )
  : QgsLayerTreeNode( NodeLayer, true )
  , mRef( layer )
  , mLayerName( layer->name() )
{
  attachToLayer();
}

QgsLayerTreeLayer::QgsLayerTreeLayer( const QString &layerId, const QString &name, const QString &source, const QString &provider )
  : QgsLayerTreeNode( NodeLayer, true )
  , mRef( layerId, name, source, provider )
  , mLayerName( name.isEmpty() ? QStringLiteral( "(?)" ) : name )
{
}

QgsLayerTreeLayer::QgsLayerTreeLayer( const QgsLayerTreeLayer &other )
  : QgsLayerTreeNode( other )
  , mRef( other.mRef )
  , mLayerName( other.mLayerName )
{
  attachToLayer();
}

void QgsLayerTreeLayer::resolveReferences( const QgsProject *project, bool looseMatching )
{
  if ( mRef )
    return;  // already assigned

  if ( !looseMatching )
  {
    mRef.resolve( project );
  }
  else
  {
    mRef.resolveWeakly( project );
  }

  if ( !mRef )
    return;

  attachToLayer();
  emit layerLoaded();
}

void QgsLayerTreeLayer::attachToLayer()
{
  if ( !mRef )
    return;

  connect( mRef.layer, &QgsMapLayer::nameChanged, this, &QgsLayerTreeLayer::layerNameChanged );
  connect( mRef.layer, &QgsMapLayer::willBeDeleted, this, &QgsLayerTreeLayer::layerWillBeDeleted );
}


QString QgsLayerTreeLayer::name() const
{
  return ( mRef && mUseLayerName ) ? mRef->name() : mLayerName;
}

void QgsLayerTreeLayer::setName( const QString &n )
{
  if ( mRef && mUseLayerName )
  {
    if ( mRef->name() == n )
      return;
    mRef->setName( n );
    // no need to emit signal: we will be notified from layer's nameChanged() signal
  }
  else
  {
    if ( mLayerName == n )
      return;
    mLayerName = n;
    emit nameChanged( this, n );
  }
}

QgsLayerTreeLayer *QgsLayerTreeLayer::readXml( QDomElement &element, const QgsReadWriteContext &context )
{
  if ( element.tagName() != QLatin1String( "layer-tree-layer" ) )
    return nullptr;

  QString layerID = element.attribute( QStringLiteral( "id" ) );
  QString layerName = element.attribute( QStringLiteral( "name" ) );

  QString providerKey = element.attribute( QStringLiteral( "providerKey" ) );
  QString source = context.pathResolver().readPath( element.attribute( QStringLiteral( "source" ) ) );

  Qt::CheckState checked = QgsLayerTreeUtils::checkStateFromXml( element.attribute( QStringLiteral( "checked" ) ) );
  bool isExpanded = ( element.attribute( QStringLiteral( "expanded" ), QStringLiteral( "1" ) ) == QLatin1String( "1" ) );

  // needs to have the layer reference resolved later
  QgsLayerTreeLayer *nodeLayer = new QgsLayerTreeLayer( layerID, layerName, source, providerKey );

  nodeLayer->readCommonXml( element );

  nodeLayer->setItemVisibilityChecked( checked != Qt::Unchecked );
  nodeLayer->setExpanded( isExpanded );
  return nodeLayer;
}

QgsLayerTreeLayer *QgsLayerTreeLayer::readXml( QDomElement &element, const QgsProject *project, const QgsReadWriteContext &context )
{
  QgsLayerTreeLayer *node = readXml( element, context );
  if ( node )
    node->resolveReferences( project );
  return node;
}

void QgsLayerTreeLayer::writeXml( QDomElement &parentElement, const QgsReadWriteContext &context )
{
  QDomDocument doc = parentElement.ownerDocument();
  QDomElement elem = doc.createElement( QStringLiteral( "layer-tree-layer" ) );
  elem.setAttribute( QStringLiteral( "id" ), layerId() );
  elem.setAttribute( QStringLiteral( "name" ), name() );

  if ( mRef )
  {
    elem.setAttribute( QStringLiteral( "source" ), context.pathResolver().writePath( mRef->publicSource() ) );
    elem.setAttribute( QStringLiteral( "providerKey" ), mRef->dataProvider() ? mRef->dataProvider()->name() : QString() );
  }

  elem.setAttribute( QStringLiteral( "checked" ), mChecked ? QStringLiteral( "Qt::Checked" ) : QStringLiteral( "Qt::Unchecked" ) );
  elem.setAttribute( QStringLiteral( "expanded" ), mExpanded ? "1" : "0" );

  writeCommonXml( elem );

  parentElement.appendChild( elem );
}

QString QgsLayerTreeLayer::dump() const
{
  return QStringLiteral( "LAYER: %1 checked=%2 expanded=%3 id=%4\n" ).arg( name() ).arg( mChecked ).arg( mExpanded ).arg( layerId() );
}

QgsLayerTreeLayer *QgsLayerTreeLayer::clone() const
{
  return new QgsLayerTreeLayer( *this );
}

void QgsLayerTreeLayer::layerWillBeDeleted()
{
  Q_ASSERT( mRef );

  emit layerWillBeUnloaded();

  mLayerName = mRef->name();
  // in theory we do not even need to do this - the weak ref should clear itself
  mRef.layer.clear();
  // layerId stays in the reference

}

void QgsLayerTreeLayer::setUseLayerName( const bool use )
{
  mUseLayerName = use;
}

bool QgsLayerTreeLayer::useLayerName() const
{
  return mUseLayerName;
}

void QgsLayerTreeLayer::layerNameChanged()
{
  Q_ASSERT( mRef );
  emit nameChanged( this, mRef->name() );
}

void QgsLayerTreeLayer::setLabelExpression( const QString &expression )
{
  mLabelExpression = expression;
}

QString QgsLayerTreeLayer::symbolExpression( const QString &ruleKey )
{
  if ( mSymbolExpressions.isEmpty() )
    updateSymbolExpressions();

  return mSymbolExpressions.value( ruleKey, QString() );
}

void QgsLayerTreeLayer::updateSymbolExpressions()
{
  if ( ! mRef )
    return;

  QgsVectorLayer *layer = qobject_cast<QgsVectorLayer *>( mRef.get() );
  if ( ! layer )
    return;

  mSymbolExpressions.clear();
  if ( layer->renderer()->type() == QLatin1String( "singleSymbol" ) ) //no need to convert if only one symbol
  {
    mSymbolExpressions.insert( layer->renderer()->legendSymbolItems()[0].ruleKey(), "TRUE" );
  }
  else
  {
    QgsRuleBasedRenderer *renderer;
    if ( layer->renderer()->type() == QLatin1String( "RuleRenderer" ) )
    {
      renderer = dynamic_cast<QgsRuleBasedRenderer *>( layer->renderer() );
      mConvertRuleKeys = false;
    }
    else
    {
      renderer = QgsRuleBasedRenderer::convertFromRenderer( layer->renderer() );
      mConvertRuleKeys = true;
      mKeyIndexer = 0;
    }

    QgsRuleBasedRenderer::Rule *root = renderer->rootRule();
    QString rootExp = root->filterExpression();
    if ( ! mConvertRuleKeys )
      mSymbolExpressions.insert( root->ruleKey(), rootExp );
    ruleIterator( root, rootExp );
  }
}

void QgsLayerTreeLayer::ruleIterator( QgsRuleBasedRenderer::Rule *baseRule, const QString prefix )
{
  const QgsRuleBasedRenderer::RuleList childrens = baseRule->children();
  if ( childrens.size() == 0 )
    return;

  QString filterexp;
  QString rulestr;
  QgsRuleBasedRenderer::Rule *childRule;
  for ( QgsRuleBasedRenderer::RuleList::const_iterator it = childrens.constBegin(); it != childrens.constEnd(); ++it )
  {
    childRule = *it;

    filterexp = childRule->filterExpression();
    if ( filterexp.contains( "ELSE" ) )
      filterexp = elseFormatter( baseRule );
    else if ( filterexp.contains( " AND " ) || filterexp.contains( " OR " ) )
      filterexp = " ( " + filterexp + " ) ";
    if ( ! filterexp.isEmpty() && ! prefix.isEmpty() )
      rulestr = prefix  + " AND " + filterexp;
    else if ( filterexp.isEmpty() && prefix.isEmpty() )
      rulestr = "";
    else if ( filterexp.isEmpty() )
      rulestr = prefix;
    else
      rulestr = filterexp;

    if ( mConvertRuleKeys )
    {
      mSymbolExpressions.insert( QString::number( mKeyIndexer ), rulestr );
      mKeyIndexer ++;
    }
    else
      mSymbolExpressions.insert( childRule->ruleKey(), rulestr );

    ruleIterator( childRule, rulestr );
  }

}

QString QgsLayerTreeLayer::elseFormatter( QgsRuleBasedRenderer::Rule *baseRule )
{
  const QgsRuleBasedRenderer::RuleList childrens = baseRule->children();
  QString filterexp;
  QString rulestr;
  QgsRuleBasedRenderer::Rule *childRule;
  for ( QgsRuleBasedRenderer::RuleList::const_iterator it = childrens.constBegin(); it != childrens.constEnd(); ++it )
  {
    childRule = *it;

    filterexp = childRule->filterExpression();
    if ( filterexp.isEmpty() || filterexp.contains( "ELSE" ) )
      continue;
    else if ( filterexp.contains( " AND " ) || filterexp.contains( " OR " ) )
      filterexp = " ( " + filterexp + " ) ";
    if ( rulestr.isEmpty() )
      rulestr = filterexp;
    else
      rulestr = rulestr + " OR " + filterexp;
  }

  return "NOT (" + rulestr + ")";

}

