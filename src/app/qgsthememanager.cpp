#include <qgsdockwidget.h>
#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <qgsmapthemecollection.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>


class QgsThemeManagerWidget : public QgsDockWidget //QgsLayerTreeProxyModel
{
    Q_OBJECT
QgsThemeManagerWidget::QgsThemeManagerWidget( QWidget *parent )
{
  setupUi( this );
  //comboBox.setInsertPolicy( QComboBox::InsertAlphabetically );
  QgsProject *mProject = QgsProject::instance();
  QgsMapThemeCollection *mThemeCollection = mProject->mapThemeCollection();
  QgsLayerTreeModel *mModel = QgisApp::instance()->layerTreeView()->layerTreeModel();
  mCurrentTheme = mThemeCollection->mapThemes()[0];
  mThemeViewer.setModel( mModel );

  connect( mThemePrev, &QToolButton::clicked, this, &QgsThemeManagerWidget::previousTheme );
  connect( mThemeNext, &QToolButton::clicked, this, &QgsThemeManagerWidget::nextTheme );
  connect( mAddThemeLayer, &QToolButton::clicked, this, &QgsThemeManagerWidget::addSelectedLayers );
  connect( mRemoveThemeLayer, &QToolButton::clicked, this, &QgsThemeManagerWidget::removeSelectedLayers );
  connect( mThemeList, &QComboBox::currentTextChanged, this, &QgsThemeManagerWidget::setTheme );
  connect( mThemeViewer, &QgsThemeViewer::droppedLayers, this , &QgsThemeManagerWidget::removeSelectedLayers)
  connect( mThemeViewer, &QgsThemeViewer::addLayerTreeLayers, this , &QgsThemeManagerWidget::addSelectedLayers)
  
}


void QgsThemeManagerWidget::setTheme( QString &themename )
{
  if ( !mThemeCollection.hasTheme( themename ) )
    return;
  mCurrentTheme = themename;
  emit themeChanged();
}

void QgsThemeManagerWidget::previousTheme()
{
  QStringList themes = mThemeCollection.mapThemes();
  idx = themes.indexOf( mCurrentTheme );
  if ( idx > 0 )
  {
    mCurrentTheme = themes.at( idx - 1 );
    emit themeChanged();
  }
}

void QgsThemeManagerWidget::nextTheme()
{
  QStringList themes = mThemeCollection.mapThemes();
  idx = 1 + themes.indexOf( mCurrentTheme );
  if ( idx < themes.size() )
  {
    mCurrentTheme = themes.at( idx );
    emit themeChanged();
  }
}

void QgsThemeManagerWidget::populateCombo()
{
  comboBox.clear();
  comboBox.addItems( mThemeCollection.mapThemes() );
}

void QgsThemeManagerWidget::themeChanged()
{
  comboBox.setCurrentText( mCurrentTheme );
}

void QgsThemeManagerWidget::viewCurrentTheme() const
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;
  QList<QgsMapLayer *> themeLayers = mThemeCollection.mapThemeVisibleLayers( mCurrentTheme );
  QStringList themeIds = mThemeCollection.mapThemeVisibleLayerIds( mCurrentTheme );
  mThemeViewer.proxyModel()->setApprovedIds( themeIds );
}


void QgsThemeManagerWidget::appendLayers( QList<QgsMapLayer *> &layers )
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;

  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection.mapThemeState( mCurrentTheme );
  for ( QgsMapLayer *layer : qgis::as_const( layers ) )
  {
    MapThemeLayerRecord newRecord( layer );
    theme.addLayerRecord( newRecord );
  }
  mThemeCollection.update( mCurrentTheme, theme );
}

void QgsThemeManagerWidget::addSelectedLayers()
{
  QList<QgsMapLayer *> &selectedLayers = QgisApp::instance()->layerTreeView()->selectedLayers();
  removeThemeLayers( selectedLayers );
}

void QgsThemeManagerWidget::removeSelectedLayers()
{
  QList<QgsMapLayer *> &selectedLayers = selectedLayers();
  removeThemeLayers( selectedLayers );
}

void QgsThemeManagerWidget::removeThemeLayers( QList<QgsMapLayer *> &layers )
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;
  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection.mapThemeState( mCurrentTheme );
  for ( QgsMapLayer *layer : qgis::as_const( layers ) )
  {
    theme.removeLayerRecord( newRecord );
  }
  mThemeCollection.update( mCurrentTheme, theme );
}




Class QgsThemeViewer : QgsLayerTreeView

 QgsThemeViewer::QgsThemeViewer( QWidget *parent )
 :QgsLayerTreeView( parent )
 {

 }

QStringList QgsThemeViewer::mimeTypes() const
{
  QStringList types;
  types << QStringLiteral( "application/qgis.thememanagerdata" );
  return types;
}

void QgsThemeViewer::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat( QStringLiteral( "application/qgis.layertreemodeldata" ) ))
        event->acceptProposedAction();
}

void QgsThemeViewer::dropEvent(QDropEvent *event)
{
    textBrowser->setPlainText(event->mimeData()->text());
    mimeTypeCombo->clear();
    mimeTypeCombo->addItems(event->mimeData()->formats());

    event->acceptProposedAction();
    ..remove layer
}

Qt::DropActions QgsThemeViewer::supportedDropActions() const
{
  return Qt::CopyAction | Qt::LinkAction | Qt::MoveAction;
}


QMimeData *QgsThemeViewer::mimeData( const QModelIndexList &indexes ) const
{

  QList<QgsMapLayer *> layers = selectedLayers();
  //QList<QgsLayerTreeNode *> nodesFinal = indexes2nodes( sortedIndexes, true );

  if ( nodesFinal.isEmpty() )
    return nullptr;

  QMimeData *mimeData = new QMimeData();
  QStringList ids;
  for ( QgsLayerTreeNode *layer : qgis::as_const(layers) )
  {
    ids << layer->id().toUtf8();
  }

  mimeData->setData( QStringLiteral( "application/qgis.thememanagerdata" ), ids.join( "+" ) );
  mimeData->setData( QStringLiteral( "application/qgis.application.pid" ), QString::number( QCoreApplication::applicationPid() ).toUtf8() );

  return mimeData;
}


bool QgsThemeViewer::dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent )
{
  if ( action == Qt::IgnoreAction )
    return true;

  if ( data->hasFormat( QStringLiteral( "application/qgis.layertreemodeldata" ) ) )
    emit addLayerTreeLayers();

}

void QgsThemeViewer::startDrag(Qt::DropActions /*supportedActions*/)
{
    QMimeData *mimeData = new QMimeData;
    mimeData->setData(PiecesList::puzzleMimeType(), itemData);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    if ( !( drag.target() == parent || drag->target() == this ) )
      emit droppedLayers();
}


