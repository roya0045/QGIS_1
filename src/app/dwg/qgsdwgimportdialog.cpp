/***************************************************************************
                         qgsdwgimportdialog.cpp
                         ----------------------
    begin                : May 2016
    copyright            : (C) 2016 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdwgimportdialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

#include "qgssettings.h"
#include "qgisapp.h"
#include "qgsdwgimporter.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerlabeling.h"
#include "qgsvectordataprovider.h"
#include "qgsproject.h"
#include "qgsfeatureiterator.h"
#include "qgslayertreeview.h"
#include "qgslayertreemodel.h"
#include "qgslayertreegroup.h"
#include "qgsrenderer.h"
#include "qgsnullsymbolrenderer.h"
#include "qgssinglesymbolrenderer.h"
#include "qgsfillsymbollayer.h"
#include "qgslinesymbollayer.h"
#include "qgspallabeling.h"
#include "qgsmapcanvas.h"
#include "qgsprojectionselectiondialog.h"
#include "qgsmessagelog.h"
#include "qgslogger.h"
#include "qgsproperty.h"
#include "qgslayertree.h"
#include "qgsguiutils.h"
#include "qgsfilewidget.h"
#include "qgsmessagebar.h"
#include "qgsgui.h"
#include "qgsfillsymbol.h"
#include "qgslinesymbol.h"

QgsDwgImportDialog::QgsDwgImportDialog( QWidget *parent, Qt::WindowFlags f )
  : QDialog( parent, f )
{
  setupUi( this );
  QgsGui::enableAutoGeometryRestore( this );
  mDatabaseFileWidget->setStorageMode( QgsFileWidget::SaveFile );
  mDatabaseFileWidget->setConfirmOverwrite( false );

  connect( buttonBox, &QDialogButtonBox::accepted, this, &QgsDwgImportDialog::buttonBox_accepted );
  connect( mDatabaseFileWidget, &QgsFileWidget::fileChanged, this, &QgsDwgImportDialog::mDatabaseFileWidget_textChanged );
  connect( mDrawingFileWidget, &QgsFileWidget::fileChanged, this, &QgsDwgImportDialog::mDrawingFileWidget_textChanged );
  connect( pdImportDrawing, &QPushButton::clicked, this, &QgsDwgImportDialog::pbImportDrawing_clicked );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsDwgImportDialog::showHelp );

  const QgsSettings s;
  cbExpandInserts->setChecked( s.value( QStringLiteral( "/DwgImport/lastExpandInserts" ), true ).toBool() );
  cbUseCurves->setChecked( s.value( QStringLiteral( "/DwgImport/lastUseCurves" ), true ).toBool() );
  cbCadStyle->setChecked( s.value( QStringLiteral( "/DwgImport/lastCADStyling" ), true ).toBool() );
  mDatabaseFileWidget->setDefaultRoot( s.value( QStringLiteral( "/DwgImport/lastDirDatabase" ), QDir::homePath() ).toString() );

 lblMessage->setHidden( true );

  const int crsid = s.value( QStringLiteral( "/DwgImport/lastCrs" ), QString::number( QgsProject::instance()->crs().srsid() ) ).toInt();

  mCrsSelector->setShowAccuracyWarnings( true );
  QgsCoordinateReferenceSystem crs;
  crs.createFromSrsId( crsid );
  mCrsSelector->setCrs( crs );
  mCrsSelector->setLayerCrs( crs );
  mCrsSelector->setMessage( tr( "Select the coordinate reference system for the dxf file. "
                                "The data points will be transformed from the layer coordinate reference system." ) );


  if ( ! QgsVectorFileWriter::supportedFormatExtensions().contains( QStringLiteral( "gpkg" ) ) )
  {
    bar->pushMessage( tr( "GDAL/OGR not built with GPKG (sqlite3) support. You will not be able to export the DWG in a GPKG." ), Qgis::MessageLevel::Critical );
  }
  updateUI();
}

QgsDwgImportDialog::~QgsDwgImportDialog()
{
  QgsSettings s;
  s.setValue( QStringLiteral( "/DwgImport/lastExpandInserts" ), cbExpandInserts->isChecked() );
  s.setValue( QStringLiteral( "/DwgImport/lastUseCurves" ), cbUseCurves->isChecked() );
  s.setValue( QStringLiteral( "/DwgImport/lastCADStyling" ), cbCadStyle->isChecked() );
}

void QgsDwgImportDialog::updateUI()
{
  bool dbAvailable = false;
  bool dbReadable = false;
  bool dwgReadable = false;

  if ( !mDatabaseFileWidget->filePath().isEmpty() )
  {
    const QFileInfo fi( mDatabaseFileWidget->filePath() );
    dbAvailable = fi.exists() ? fi.isWritable() : QFileInfo( fi.path() ).isWritable();
    dbReadable = fi.exists() && fi.isReadable();
  }

  if ( !mDrawingFileWidget->filePath().isEmpty() )
  {
    const QFileInfo fi( mDrawingFileWidget->filePath() );
    dwgReadable = fi.exists() && fi.isReadable();
  }

}

void QgsDwgImportDialog::mDatabaseFileWidget_textChanged( const QString &filename )
{
  QgsSettings s;
  s.setValue( QStringLiteral( "/DwgImport/lastDirDatabase" ), QFileInfo( filename ).canonicalPath() );
  mImported = false;
  updateUI();
}

void QgsDwgImportDialog::mDrawingFileWidget_textChanged( const QString &filename )
{
  Q_UNUSED( filename );
  mImported = false;
  updateUI();
}

void QgsDwgImportDialog::pbImportDrawing_clicked()
{
  const QgsTemporaryCursorOverride waitCursor( Qt::BusyCursor );

  QgsDwgImporter importer( mDatabaseFileWidget->filePath(), mCrsSelector->crs() );

  lblMessage->setVisible( true );

  QString error;
  if ( importer.import( mDrawingFileWidget->filePath(), error, cbExpandInserts->isChecked(), cbUseCurves->isChecked(), lblMessage ) )
  {
    bar->pushMessage( tr( "Drawing import completed." ), Qgis::MessageLevel::Info );
  }
  else
  {
    bar->pushMessage( tr( "Drawing import failed (%1)" ).arg( error ), Qgis::MessageLevel::Critical );
  }
  if ( cbCadStyle->isChecked() )
    connect( QgsProject::instance() , &QgsProject::layersAdded, this , &QgsDwgImportDialog::styleImportedLayers );
  propose_layers();
  mImported = true;
}

void QgsDwgImportDialog::propose_layers()
{
  QString db = mDatabaseFileWidget->filePath();
  if (!QFileInfo::exists( db ) )
    return;

  QgisApp::instance()->openLayer( db, true );

}

void QgsDwgImportDialog::styleImportedLayers( const QList<QgsMapLayer *> &layers )
{
  QString style;
  QgsVectorLayer *vLayer;
  for ( QgsMapLayer *mapLayer : layers )
  {
    vLayer = qobject_cast<QgsVectorLayer *>( mapLayer );
    if ( vLayer )
      styleLayer( vLayer );
  }
  disconnect( QgsProject::instance() , &QgsProject::layersAdded, this , &QgsDwgImportDialog::styleImportedLayers );
}

void QgsDwgImportDialog::styleLayer( QgsVectorLayer *layer )
{
  if ( !layer )
    return;
  QgsSymbol *sym = nullptr;
  QString layerName = layer->name();

    if ( layerName == QStringLiteral( "hatches" ) )
    {
      QgsSimpleFillSymbolLayer *sfl = new QgsSimpleFillSymbolLayer();
      sfl->setDataDefinedProperty( QgsSymbolLayer::PropertyFillColor, QgsProperty::fromField( QStringLiteral( "color" ) ) );
      sfl->setStrokeStyle( Qt::NoPen );
      sym = new QgsFillSymbol();
      sym->changeSymbolLayer( 0, sfl );
      layer->setRenderer( new QgsSingleSymbolRenderer( sym ) );
    }

    else if ( layerName == QStringLiteral( "lines" ) )
    {
      QgsSimpleLineSymbolLayer *sll = new QgsSimpleLineSymbolLayer();
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeColor, QgsProperty::fromField( QStringLiteral( "color" ) ) );
      sll->setPenJoinStyle( Qt::MiterJoin );
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeWidth, QgsProperty::fromField( QStringLiteral( "linewidth" ) ) );
      // sll->setUseCustomDashPattern( true );
      // sll->setCustomDashPatternUnit( QgsSymbolV2::MapUnit );
      // sll->setDataDefinedProperty( QgsSymbolLayer::PropertyCustomDash, QgsProperty::fromField( "linetype" ) );
      sym = new QgsLineSymbol();
      sym->changeSymbolLayer( 0, sll );
      sym->setOutputUnit( QgsUnitTypes::RenderMillimeters );
      layer->setRenderer( new QgsSingleSymbolRenderer( sym ) );
    }

    else if ( layerName == QStringLiteral( "polylines" ) )
    {
      sym = new QgsLineSymbol();

      QgsSimpleLineSymbolLayer *sll = new QgsSimpleLineSymbolLayer();
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeColor, QgsProperty::fromField( QStringLiteral( "color" ) ) );
      sll->setPenJoinStyle( Qt::MiterJoin );
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeWidth, QgsProperty::fromField( QStringLiteral( "width" ) ) );
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyLayerEnabled, QgsProperty::fromExpression( QStringLiteral( "width>0" ) ) );
      sll->setOutputUnit( QgsUnitTypes::RenderMapUnits );
      // sll->setUseCustomDashPattern( true );
      // sll->setCustomDashPatternUnit( QgsSymbolV2::MapUnit );
      // sll->setDataDefinedProperty( QgsSymbolLayer::PropertyCustomDash, QgsProperty::fromField( "linetype" ) );
      sym->changeSymbolLayer( 0, sll );

      sll = new QgsSimpleLineSymbolLayer();
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeColor, QgsProperty::fromField( QStringLiteral( "color" ) ) );
      sll->setPenJoinStyle( Qt::MiterJoin );
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyStrokeWidth, QgsProperty::fromField( QStringLiteral( "linewidth" ) ) );
      sll->setDataDefinedProperty( QgsSymbolLayer::PropertyLayerEnabled, QgsProperty::fromExpression( QStringLiteral( "width=0" ) ) );
      sll->setOutputUnit( QgsUnitTypes::RenderMillimeters );
      sym->appendSymbolLayer( sll );

      layer->setRenderer( new QgsSingleSymbolRenderer( sym ) );
    }

    else if ( layerName == QStringLiteral( "texts" ) )
    {
      layer->setRenderer( new QgsNullSymbolRenderer() );

      QgsTextFormat tf;
      tf.setSizeUnit( QgsUnitTypes::RenderMapUnits );

      QgsPalLayerSettings pls;
      pls.setFormat( tf );

      pls.drawLabels = true;
      pls.fieldName = QStringLiteral( "text" );
      pls.wrapChar = QStringLiteral( "\\P" );

      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::Size, QgsProperty::fromField( QStringLiteral( "height" ) ) );
      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::Color, QgsProperty::fromField( QStringLiteral( "color" ) ) );
      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::MultiLineHeight, QgsProperty::fromExpression( QStringLiteral( "CASE WHEN interlin<0 THEN 1 ELSE interlin*1.5 END" ) ) );
      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::PositionX, QgsProperty::fromExpression( QStringLiteral( "$x" ) ) );
      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::PositionY, QgsProperty::fromExpression( QStringLiteral( "$y" ) ) );

      // DXF TEXT
      // vertical: 0 = Base, 1 = Bottom, 2 = Middle, 3 = Top,  default Base
      // horizontal: 0 = Left, 1 = Center, 2 = Right, 3 = Aligned (if Base), 4 = Middle (if Base), default Left

      // DXF MTEXT
      // 1 = Top left;    2 = Top center;    3 = Top right
      // 4 = Middle left; 5 = Middle center; 6 = Middle right
      // 7 = Bottom left; 8 = Bottom center; 9 = Bottom right

      // QGIS Quadrant
      // 0 QuadrantAboveLeft, 1 QuadrantAbove, 2 QuadrantAboveRight,
      // 3 QuadrantLeft,      4 QuadrantOver,  5 QuadrantRight,
      // 6 QuadrantBelowLeft, 7 QuadrantBelow, 8 QuadrantBelowRight,

      pls.dataDefinedProperties().setProperty(
        QgsPalLayerSettings::Hali,
        QgsProperty::fromExpression( QStringLiteral(
                                       "CASE"
                                       " WHEN etype=%1 THEN"
                                       " CASE"
                                       " WHEN textgen % 3=2 THEN 'Center'"
                                       " WHEN textgen % 3=0 THEN 'Right'"
                                       " ELSE 'Left'"
                                       " END"
                                       " ELSE"
                                       " CASE"
                                       " WHEN alignh=1 THEN 'Center'"
                                       " WHEN alignh=2 THEN 'Right'"
                                       " ELSE 'Left'"
                                       " END"
                                       " END"
                                     ).arg( DRW::MTEXT )
                                   )
      );

      pls.dataDefinedProperties().setProperty(
        QgsPalLayerSettings::Vali,
        QgsProperty::fromExpression( QStringLiteral(
                                       "CASE"
                                       " WHEN etype=%1 THEN"
                                       " CASE"
                                       " WHEN textgen<4 THEN 'Top'"
                                       " WHEN textgen<7 THEN 'Half'"
                                       " ELSE 'Bottom'"
                                       " END"
                                       " ELSE"
                                       " CASE"
                                       " WHEN alignv=1 THEN 'Bottom'"
                                       " WHEN alignv=2 THEN 'Half'"
                                       " WHEN alignv=3 THEN 'Top'"
                                       " ELSE 'Base'"
                                       " END"
                                       " END"
                                     ).arg( DRW::MTEXT )
                                   )
      );

      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::LabelRotation, QgsProperty::fromExpression( QStringLiteral( "360-angle" ) ) );
      pls.dataDefinedProperties().setProperty( QgsPalLayerSettings::AlwaysShow, QgsProperty::fromExpression( QStringLiteral( "1" ) ) );

      layer->setLabeling( new QgsVectorLayerSimpleLabeling( pls ) );
      layer->setLabelsEnabled( true );
    }

    else if ( layerName == QStringLiteral( "points" ) )
    {
      // FIXME: use PDMODE?
      layer->setRenderer( new QgsNullSymbolRenderer() );
    }

    else if ( layerName == QStringLiteral( "inserts" ) )
    {
      if ( !cbExpandInserts->isChecked() )
      {
        if ( layer && layer->renderer() )
        {
          QgsSingleSymbolRenderer *ssr = dynamic_cast<QgsSingleSymbolRenderer *>( layer->renderer() );
          if ( ssr && ssr->symbol() && ssr->symbol()->symbolLayer( 0 ) )
            ssr->symbol()->symbolLayer( 0 )->setDataDefinedProperty( QgsSymbolLayer::PropertyAngle, QgsProperty::fromExpression( QStringLiteral( "180-angle*180.0/pi()" ) ) );
        }
      }
    }
}


void QgsDwgImportDialog::buttonBox_accepted()
{
  if ( !mImported && QFileInfo::exists( mDatabaseFileWidget->filePath() ) && QFileInfo::exists( mDrawingFileWidget->filePath() )  )
      pbImportDrawing_clicked();
}

void QgsDwgImportDialog::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/opening_data.html#importing-a-dxf-or-dwg-file" ) );
}
