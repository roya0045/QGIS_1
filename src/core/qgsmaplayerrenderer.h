/***************************************************************************
  qgsmaplayerrenderer.h
  --------------------------------------
  Date                 : December 2013
  Copyright            : (C) 2013 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPLAYERRENDERER_H
#define QGSMAPLAYERRENDERER_H

#include <QStringList>

#include "qgis_core.h"
#include "qgis_sip.h"

class QgsFeedback;
class QgsRenderContext;

/**
 * \ingroup core
 * Base class for utility classes that encapsulate information necessary
 * for rendering of map layers. The rendering is typically done in a background
 * thread, so it is necessary to keep all structures required for rendering away
 * from the original map layer because it may change any time.
 *
 * Because the data needs to be copied (to avoid the need for locking),
 * it is highly desirable to use copy-on-write where possible. This way,
 * the overhead of copying (both memory and CPU) will be kept low.
 * Qt containers and various Qt classes use implicit sharing.
 *
 * The scenario will be:
 *
 * # renderer job (doing preparation in the GUI thread) calls
 *   QgsMapLayer::createMapRenderer() and gets instance of this class.
 *   The instance is initialized at that point and should not need
 *   additional calls to QgsVectorLayer.
 * # renderer job (still in GUI thread) stores the renderer for later use.
 * # renderer job (in worker thread) calls QgsMapLayerRenderer::render()
 * # renderer job (again in GUI thread) will check errors() and report them
 *
 * \since QGIS 2.4
 */
class CORE_EXPORT QgsMapLayerRenderer
{
  public:

    /**
     * Constructor for QgsMapLayerRenderer, with the associated \a layerID and render \a context.
     */
    QgsMapLayerRenderer( const QString &layerID, QgsRenderContext *context = nullptr )
      : mLayerID( layerID )
      , mContext( context )
    {}

    virtual ~QgsMapLayerRenderer() = default;

    //! Do the rendering (based on data stored in the class)
    virtual bool render() = 0;

    /**
     * Returns TRUE if the renderer must be rendered to a raster paint device (e.g. QImage).
     *
     * Some layer settings require layers to be effectively "flattened" while rendering maps,
     * which is achieved by first rendering the layer onto a raster paint device and then compositing
     * the resultant image onto the final map render.
     *
     * E.g. if a layer contains features with transparency or alternative blending modes, and
     * the effects of these opacity or blending modes should be restricted to only affect other
     * features within the SAME layer, then a flattened raster based render is required.
     *
     * Subclasses should return TRUE whenever their corresponding layer settings require the
     * layer to always be rendered using a raster paint device.
     *
     * \since QGIS 3.18
     */
    virtual bool forceRasterRender() const { return false; }

    /**
     * Access to feedback object of the layer renderer (may be NULLPTR)
     * \since QGIS 3.0
     */
    virtual QgsFeedback *feedback() const { return nullptr; }

    //! Returns list of errors (problems) that happened during the rendering
    QStringList errors() const { return mErrors; }

    //! Gets access to the ID of the layer rendered by this class
    QString layerId() const { return mLayerID; }

    /**
     * Returns the render context associated with the renderer.
     *
     * \since QGIS 3.10
     */
    QgsRenderContext *renderContext() { return mContext; }

    /**
     * Returns the render context associated with the renderer.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.18
     */
    const QgsRenderContext *renderContext() const SIP_SKIP { return mContext; }

  protected:
    QStringList mErrors;
    QString mLayerID;

  private:

    // TODO QGIS 4.0 - make reference instead of pointer!

    /**
     * Associated render context.
     *
     * \since QGIS 3.10
     */
    QgsRenderContext *mContext = nullptr;
};

#endif // QGSMAPLAYERRENDERER_H
