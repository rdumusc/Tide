/*********************************************************************/
/* Copyright (c) 2016-2017, EPFL/Blue Brain Project                  */
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
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of The University of Texas at Austin.                 */
/*********************************************************************/

#include "RestInterface.h"

#include "FileBrowser.h"
#include "FileReceiver.h"
#include "HtmlContent.h"
#include "LoggingUtility.h"
#include "MasterConfiguration.h"
#include "RestServer.h"
#include "SceneController.h"
#include "ThumbnailCache.h"
#include "json.h"
#include "scene/ContentFactory.h"
#include "serialization.h"

#include <tide/master/version.h>

#include <zerozero/http/helpers.h>

#include <QDir>

using namespace std::placeholders;
using namespace zerozero;

namespace
{
QString _makeAbsPath(const QString& baseDir, const QString& uri)
{
    return QDir::isRelativePath(uri) ? baseDir + "/" + uri : uri;
}

template <typename T>
std::future<http::Response> processJsonRpc(T* jsonRpc,
                                           const http::Request& request)
{
    const auto body = jsonRpc->process(request.body);
    if (body.empty())
        return http::make_ready_response(http::Code::OK);
    return http::make_ready_response(http::Code::OK, body, "application/json");
}

using AsyncAction = std::function<void(QString, promisePtr)>;

std::future<http::Response> handleAsyncRpc(const http::Request& request,
                                           AsyncAction action)
{
    const auto obj = json::toObject(request.body);
    if (obj.empty() || !obj["uri"].isString())
        return http::make_ready_response(http::Code::BAD_REQUEST);

    const auto uri = obj["uri"].toString();
    return std::async(std::launch::deferred, [action, uri]() {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        action(uri, std::move(promise));

        if (!future.get())
            return http::Response{http::Code::INTERNAL_SERVER_ERROR};
        return http::Response{http::Code::OK};
    });
}
}

/** Overload to serialize QSize as an array instead of an object. */
std::string to_json(const QSize& size)
{
    return json::toString(QJsonArray{{size.width(), size.height()}});
}

class RestInterface::Impl
{
public:
    Impl(const int port, OptionsPtr options_, DisplayGroup& group,
         const MasterConfiguration& config)
        : server{port}
        , options{options_}
        , size{config.getTotalSize()}
        , thumbnailCache{group}
        , appController{config}
        , sceneController{group}
        , contentBrowser{config.getContentDir(),
                         ContentFactory::getSupportedFilesFilter()}
        , sessionBrowser{config.getSessionsDir(), QStringList{"*.dcx"}}
        , htmlContent{new HtmlContent(server)}
    {
    }

    std::future<http::Response> getWindowInfo(
        const http::Request& request) const
    {
        const auto path = QString::fromStdString(request.path);
        if (path.endsWith("/thumbnail"))
        {
            const auto pathSplit = path.split("/");
            if (pathSplit.size() == 2 && pathSplit[1] == "thumbnail")
                return thumbnailCache.getThumbnail(url_decode(pathSplit[0]));
        }
        return make_ready_response(http::Code::BAD_REQUEST);
    }

    RestServer server;
    OptionsPtr options;
    QSize size;
    ThumbnailCache thumbnailCache;
    AppController appController;
    SceneController sceneController;
    FileBrowser contentBrowser;
    FileBrowser sessionBrowser;
    FileReceiver fileReceiver;
    std::unique_ptr<HtmlContent> htmlContent; // HTML interface (optional)
};

RestInterface::RestInterface(const int port, OptionsPtr options,
                             DisplayGroup& group,
                             const MasterConfiguration& config)
    : _impl(new Impl(port, options, group, config))
{
    // Note: using same formatting as TUIO instead of put_flog() here
    std::cout << "listening to REST messages on TCP port "
              << _impl->server.getPort() << std::endl;

    QObject::connect(&_impl->fileReceiver, &FileReceiver::open,
                     &_impl->appController, &AppController::open);

    auto& server = _impl->server;

    server.handle(http::Method::GET, "tide/version", [](const http::Request&) {
        return http::make_ready_response(http::Code::OK,
                                         tide::Version::toJSON());
    });
    server.expose(config, "tide/config");
    server.expose(_impl->size, "tide/size");
    server.expose(*_impl->options, "tide/options");
    server.subscribe(*_impl->options, "tide/options");
    server.expose(group, "tide/windows");

    server.handle(http::Method::GET, "tide/windows/",
                  std::bind(&Impl::getWindowInfo, _impl.get(), _1));

    server.handle(http::Method::POST, "tide/application",
                  std::bind(processJsonRpc<AppController>,
                            &_impl->appController, _1));

    server.handle(http::Method::POST, "tide/controller",
                  std::bind(processJsonRpc<SceneController>,
                            &_impl->sceneController, _1));

    server.handle(http::Method::POST, "tide/upload",
                  std::bind(&FileReceiver::prepareUpload, &_impl->fileReceiver,
                            _1));

    server.handle(http::Method::PUT, "tide/upload/",
                  std::bind(&FileReceiver::handleUpload, &_impl->fileReceiver,
                            _1));

    server.handle(http::Method::GET, "tide/files/",
                  std::bind(&FileBrowser::list, &_impl->contentBrowser, _1));

    server.handle(http::Method::GET, "tide/sessions/",
                  std::bind(&FileBrowser::list, &_impl->sessionBrowser, _1));

    // TODO See how to make this async also through JSON-RPC

    const auto contentDir = config.getContentDir();
    const auto sessionDir = config.getSessionsDir();

    AsyncAction openFunc = [this, contentDir](QString uri, promisePtr promise) {
        _impl->appController.open(_makeAbsPath(contentDir, uri), QPointF(),
                                  promise);
    };
    AsyncAction loadFunc = [this, sessionDir](QString uri, promisePtr promise) {
        _impl->appController.load(_makeAbsPath(sessionDir, uri), promise);
    };
    AsyncAction saveFunc = [this, sessionDir](QString uri, promisePtr promise) {
        _impl->appController.save(_makeAbsPath(sessionDir, uri), promise);
    };

    server.handle(http::Method::PUT, "tide/open",
                  std::bind(handleAsyncRpc, _1, openFunc));
    server.handle(http::Method::PUT, "tide/load",
                  std::bind(handleAsyncRpc, _1, loadFunc));
    server.handle(http::Method::PUT, "tide/save",
                  std::bind(handleAsyncRpc, _1, saveFunc));
}

RestInterface::~RestInterface()
{
}

void RestInterface::exposeStatistics(const LoggingUtility& logger) const
{
    _impl->server.expose(logger, "tide/stats");
}

const AppController& RestInterface::getAppController() const
{
    return _impl->appController;
}
