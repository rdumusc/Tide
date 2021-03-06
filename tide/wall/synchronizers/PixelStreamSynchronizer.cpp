/*********************************************************************/
/* Copyright (c) 2015-2018, EPFL/Blue Brain Project                  */
/*                          Raphael Dumusc <raphael.dumusc@epfl.ch>  */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE  IS  PROVIDED  BY  THE  ECOLE  POLYTECHNIQUE    */
/*    FEDERALE DE LAUSANNE  ''AS IS''  AND ANY EXPRESS OR IMPLIED    */
/*    WARRANTIES, INCLUDING, BUT  NOT  LIMITED  TO,  THE  IMPLIED    */
/*    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR  A PARTICULAR    */
/*    PURPOSE  ARE  DISCLAIMED.  IN  NO  EVENT  SHALL  THE  ECOLE    */
/*    POLYTECHNIQUE  FEDERALE  DE  LAUSANNE  OR  CONTRIBUTORS  BE    */
/*    LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,    */
/*    EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING, BUT NOT    */
/*    LIMITED TO,  PROCUREMENT  OF  SUBSTITUTE GOODS OR SERVICES;    */
/*    LOSS OF USE, DATA, OR  PROFITS;  OR  BUSINESS INTERRUPTION)    */
/*    HOWEVER CAUSED AND  ON ANY THEORY OF LIABILITY,  WHETHER IN    */
/*    CONTRACT, STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE    */
/*    OR OTHERWISE) ARISING  IN ANY WAY  OUT OF  THE USE OF  THIS    */
/*    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.   */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of Ecole polytechnique federale de Lausanne.          */
/*********************************************************************/

#include "PixelStreamSynchronizer.h"

#include "datasources/PixelStreamUpdater.h"
#include "qml/Tile.h"
#include "scene/Window.h"
#include "scene/ZoomHelper.h"

PixelStreamSynchronizer::PixelStreamSynchronizer(
    std::shared_ptr<PixelStreamUpdater> updater, const deflect::View view,
    const uint channel)
    : TiledSynchronizer{TileSwapPolicy::SwapTilesSynchronously}
    , _updater{std::move(updater)}
    , _view{view}
    , _channel{channel}
{
    _updater->synchronizers.register_(this);

    connect(_updater.get(), &PixelStreamUpdater::pictureUpdated, this,
            &PixelStreamSynchronizer::_onPictureUpdated);
}

PixelStreamSynchronizer::~PixelStreamSynchronizer()
{
    _updater->synchronizers.deregister(this);
}

void PixelStreamSynchronizer::update(const Window& window,
                                     const QRectF& visibleArea)
{
    // Tiles area corresponds to Content dimensions for PixelStreams
    const auto tilesSurface = window.getContent().getDimensions();
    const auto visibleTilesArea =
        ZoomHelper{window}.toTilesArea(visibleArea, tilesSurface);

    if (_visibleTilesArea == visibleTilesArea)
        return;

    _visibleTilesArea = visibleTilesArea;
    markTilesDirty();
}

void PixelStreamSynchronizer::swapTiles()
{
    TiledSynchronizer::swapTiles();

    _fpsCounter.tick();
    emit statisticsChanged();

    _setTilesArea(_updater->getTilesArea(0, getChannel()));
}

QString PixelStreamSynchronizer::getStatistics() const
{
    return _fpsCounter.toString() + " fps";
}

deflect::View PixelStreamSynchronizer::getView() const
{
    return _view;
}

const DataSource& PixelStreamSynchronizer::getDataSource() const
{
    return *_updater;
}

uint PixelStreamSynchronizer::getChannel() const
{
    return _channel;
}

QRectF PixelStreamSynchronizer::getVisibleTilesArea(const uint lod) const
{
    Q_UNUSED(lod);
    return _visibleTilesArea;
}

QSize PixelStreamSynchronizer::_getTilesArea(const uint lod) const
{
    Q_UNUSED(lod);
    return _tilesArea;
}

void PixelStreamSynchronizer::_setTilesArea(const QSize& tilesArea)
{
    if (tilesArea == _tilesArea)
        return;

    _tilesArea = tilesArea;
    emit tilesAreasChanged();
}

void PixelStreamSynchronizer::_onPictureUpdated()
{
    markTilesDirty();
    markExistingTilesDirty();
}
