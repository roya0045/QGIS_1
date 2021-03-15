
#ifndef QGSTHEMEMANAGERWIDGET_H
#define QGSTHEMEMANAGERWIDGET_H


/**
 * QgsThemeManagerWidget class: Used to display layers of selected themes in order to easily modify existing themes.
 * \since QGIS 3.20
 */

class QgsThemeManagerWidget : public QWidget //QgsLayerTreeProxyModel
{
  Q_OBJECT
	public:

		QgsThemeManagerWidget( QWidget *parent );

	signal:
		/**
		 * Used to call viewCurrentTheme
		 */
		void themeChanged();

	slots:
		/**
		 * Used to catche the ComboBox signal
		 */
		void setTheme( QString &themename );

		/**
		 * Triggered by the previous theme button
		 */
		void previousTheme();

		/**
		 * Triggered by the next theme button
		 */
		void nextTheme();

		/**
		 * Populate the ComboBox to keep the list synched
		 */
		void populateCombo();

		/**
		 * Populate the ThemeViewer
		 */
		void viewCurrentTheme() const;

		/**
		 * Triggered by the all layer button
		 */
		void addSelectedLayers();

		/**
		 * Triggered by the remove layer button
		 */
		void removeSelectedLayers();

	private:

		/**
		 * Used by the tool or drag & drop from the main layertree
		 */
		void appendLayers( QList<QgsMapLayer *> &layers );

		/**
		 * Used by the tool or drag & drop
		 */
		void removeThemeLayers( QList<QgsMapLayer *> &layers );

		QString mCurrentTheme;
		QToolButton mThemePrev;
		QToolButton mThemeNext;
		QToolButton mAddThemeLayer;
		QToolButton mRemoveThemeLayer;
		QToolButton mRemoveThemeLayer;
		QComboBox mThemeList;
		QgsThemeViewer mThemeViewer;
		QgsProject *mProject;
		QgsMapThemeCollection *mThemeCollection;
		QgsLayerTreeModel *mModel;

}

/**
 * Specialised QgsLayerTreeView to display theme specific layers. With specific handling for incoming and outgoing drag&drop.
 * \since QGIS 3.20
 */
Class QgsThemeViewer : QgsLayerTreeView
{
public:
	Qt::DropActions supportedDropActions() const;
	bool dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent );

signal:
	void addLayerTreeLayers();

	void droppedLayers();
private:
	QStringList mimeTypes() const;

	void dragEnterEvent(QDragEnterEvent *event);

	void dropEvent(QDropEvent *event);

	QMimeData *mimeData( const QModelIndexList &indexes ) const;

	QWidget *parent;

}
