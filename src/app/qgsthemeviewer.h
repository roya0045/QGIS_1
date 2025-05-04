/***************************************************************************
  qgsthemeviewer.h
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

#ifndef QGSTHEMEVIEWER_H
#define QGSTHEMEVIEWER_H

#include <QObject>
#include <QWidget>
#include "qgslayertreeview.h"
#include "qgslayertreemodel.h"
#include "qgsmapthemecollection.h"

class QMimeData;
class QContextMenuEvent;


class QgsThemeModel : public QgsLayerTreeModel
{
    Q_OBJECT

  public:
    //! Construct the model based on the given layer tree
    QgsThemeModel( QgsLayerTree *rootNode, QObject *parent SIP_TRANSFERTHIS = nullptr );

    //! Alternative constructor.
    QgsThemeModel( QgsLayerTree *rootNode );

    QVariant data( const QModelIndex &index, int role ) const override;

    Qt::ItemFlags flags( const QModelIndex &index ) const override;

    /**
     * Returns filtered list of active legend nodes attached to a particular layer node
     * (by default it returns also legend node embedded in parent layer node (if any) unless skipNodeEmbeddedInParent is TRUE)
     * \note Parameter skipNodeEmbeddedInParent added in QGIS 2.18
     * \note Not available in Python bindings
     * \see layerOriginalLegendNodes()
     * \since QGIS 3.10
     */
    QList<QgsLayerTreeModelLegendNode *> layerLegendNodes( QgsLayerTreeLayer *nodeLayer, bool skipNodeEmbeddedInParent = false ) const SIP_SKIP;

    /**
     * Clears any previously cached data for the specified \a node.
     * \since QGIS 3.14
     */
    void clearCachedData( QgsLayerTreeNode *node ) const;

    void loadSymbols( QMap< QString, QString > styles );

  signals:

    /**
     * Emitted to refresh the legend.
     * \since QGIS 3.10
     */
    void refreshTheme();

  private slots:

    /**
     * Handle incoming signal to refresh the legend.
     * \since QGIS 3.10
     */
    void resyncTheme();

};


class QgsThemeProxy :  public QgsLayerTreeProxyModel
{
    Q_OBJECT
  public:

    /**
     * Constructs QgsThemeProxy with source model \a treeModel and a \a parent
     */
    QgsThemeProxy( QgsThemeModel *treeModel, QgsMapThemeCollection * collection, QObject *parent );

    /**
     * Allow non-spatial layers and empty groups to be show.
     * \param show If TRUE (default behavior), non-spatial layers and groups will be shown.
     * \since QGIS 3.26
     */
    void setShowAllNodes( bool show );

    /**
     * Used to bypass the filtering.
     * \since QGIS 3.26
     */
    void removeTheme();//{ mTheme = QgsMapThemeCollection::MapThemeRecord(); }

    void setMapTheme( QString themeName, const QMap<QString, QString> styles );

    QModelIndex mapToSource( const QModelIndex idx ) const;

    QString themeName(){ return mThemeName; }

  public slots:
    void evalSafe(){ mEvalSafe = true;}
    void evalUnsafe(){ mEvalSafe = false; }

  protected:

    bool filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const override;

  private:

    bool nodeShown( QgsLayerTreeNode *node ) const;
    bool legendNodeShown( QgsLayerTreeModelLegendNode *node ) const;
    QString mThemeName;
    //QgsMapThemeCollection::MapThemeRecord mTheme;// = nullptr;
    QgsMapThemeCollection *mThemeHolder = nullptr;
    bool mShowAllNodes = true;
    QgsThemeModel *mLayerTreeModel = nullptr;
    QgsMapSettings *mapSetting = nullptr;
    bool mEvalSafe = true;
};

/**
 * QgsThemeViewe class: QgsLayerTreeView with custom drag & drop handling to interact with QgsThemeManagerWidget
 * \since QGIS 3.20
 */

class QgsThemeViewer :  public QgsLayerTreeView
{
    Q_OBJECT
  public:
    explicit QgsThemeViewer( QWidget *parent = nullptr );

    //! List supported drop actions
    Qt::DropActions supportedDropActions() const;

    /**
     * Overridden setModel() from base class.
     * \param model Model used to populate the view. Only QgsLayerTreeModel models are accepted.
     */
    //void setModel( QAbstractItemModel *model ) override;

    /**
     * Disconnects the Proxy Model to prevent crash when using a second view on the same model.
     * \since QGIS3.24
     */
    void disconnectProxyModel();
    //! Overridden setModel() from base class. Only QgsLayerTreeModel is an acceptable model.
    void setModel( QgsLayerTreeModel *model, QgsMapThemeCollection *collection );

    /**
     * Allow non-spatial layers and empty groups to be show.
     * \param show If TRUE (default behavior), non-spatial layers and groups will be shown.
     * \since QGIS 3.20
     */
    void showAllNodes( bool show );


    /**
     * Sets the MapThemeRecord to filter the proxy.
     * \since QGIS 3.26
     */
    void setProxyMapThemeRecord( const QgsMapThemeCollection::MapThemeRecord theme );

    /**
     * Returns a pointer to the QgsThemeProxy used in the view.
     * \since QGIS 3.26
     */
    QgsThemeProxy *proxyModel(){ return mProxyModel; }

    void setProxyMapTheme( QString themeName, const QMap<QString, QString> styles );
    //QgsLayerTreeNode *index2node( const QModelIndex &index ) const;
    QgsLayerTreeNode *index2node( const QModelIndex &index ) const;
  signals:

    //! Used by QgsThemeManagerWidget to trigger the import of layers
    void layersAdded();

    //! Used by QgsThemeManagerWidget to trigger the removal of layers
    void layersDropped();

    void showMenu( const QPoint &pos );

  protected:
    void contextMenuEvent( QContextMenuEvent *event ) override;

  protected slots:
    void onExpandedChanged( QgsLayerTreeNode *node, bool expanded );

  private:

    QStringList mimeTypes() const;

    QMimeData *mimeData() const;

    void dragEnterEvent( QDragEnterEvent *event ) override;

    //! Call emit layersAdded if drop incoming from the layers widget
    void dropEvent( QDropEvent *event ) override;

    //! Prevent any outdrag and loss of layers when attemptint to move or select them.
    void startDrag( Qt::DropActions ) override;
    QgsThemeModel *mModel = nullptr;
    QgsThemeProxy *mProxyModel = nullptr;

};

class QgsThemeViewerDelegate : public QStyledItemDelegate
{
    Q_OBJECT
  public:
    explicit QgsThemeViewerDelegate( QgsThemeViewer *parent );

    void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const override;

    bool helpEvent( QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index ) override;

  private slots:
    void onClicked( const QModelIndex &index );

  private:
    QgsThemeViewer *mThemeViewer = nullptr;
};


#endif // QGSTHEMEVIEWER_H
