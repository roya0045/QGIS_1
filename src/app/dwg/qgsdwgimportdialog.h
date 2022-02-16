/***************************************************************************
                         qgsdwgimportdialog.h
                         --------------------
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

#ifndef QGSDWGIMPORTDIALOG_H
#define QGSDWGIMPORTDIALOG_H

#include "ui_qgsdwgimportbase.h"
#include "qgshelp.h"

class QgsVectorLayer;
class QgsLayerTreeGroup;

class QgsDwgImportDialog : public QDialog, private Ui::QgsDwgImportBase
{
    Q_OBJECT
  public:
    QgsDwgImportDialog( QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags() );
    ~QgsDwgImportDialog() override;

  private slots:
    void buttonBox_accepted();
    void pbImportDrawing_clicked();
    void mDatabaseFileWidget_textChanged( const QString &filename );
    void mDrawingFileWidget_textChanged( const QString &filename );
    void showHelp();
    void styleImportedLayers( const QList<QgsMapLayer *> &layers );

  private:
    void styleLayer( QgsVectorLayer *layer );
    void updateUI();
    void expandInserts();
    void propose_layers();
    void updateCheckState( Qt::CheckState state );
    bool mImported = false;
};

#endif // QGSDWGIMPORTDIALOG_H
