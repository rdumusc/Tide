/*********************************************************************/
/* Copyright (c) 2016, EPFL/Blue Brain Project                       */
/*                     Raphael Dumusc <raphael.dumusc@epfl.ch>       */
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
/* or implied, of Ecole polytechnique federale de Lausanne.          */
/*********************************************************************/

#ifndef RESTSERVER_H
#define RESTSERVER_H

#include <zerozero/response.h>
#include <zerozero/server.h>

#include <QSocketNotifier>

/**
 * A Websocket + HTTP Server for use in a Qt application.
 */
class RestServer : public zerozero::Server
{
public:
    /**
     * Start a server with an OS-chosen port.
     * @throw std::runtime_error if a connection issue occured.
     */
    RestServer();

    /**
     * Start a server on a defined port.
     * @param port the TCP port to listen to.
     * @throw std::runtime_error if the port is already in use or a connection
     *        issue occured.
     */
    explicit RestServer(int port);

    /** Stop the server. */
    ~RestServer();

    /**
     * Expose a JSON-serializable object.
     *
     * @param object to expose.
     * @param endpoint for accessing the object.
     */
    template <typename Obj>
    bool expose(Obj& object, const std::string& endpoint)
    {
        using namespace zerozero::http;
        return handle(Method::GET, endpoint, [&object](const Request&) {
            return make_ready_response(Code::OK, to_json(object),
                                       "application/json");
        });
    }

    /**
     * Subscribe a JSON-deserializable object.
     *
     * @param object to subscribe.
     * @param endpoint for modifying the object.
     */
    template <typename Obj>
    bool subscribe(Obj& object, const std::string& endpoint)
    {
        using namespace zerozero::http;
        return handle(Method::PUT, endpoint, [&object](const Request& req) {
            const auto success = from_json(object, req.body);
            return make_ready_response(success ? Code::OK : Code::BAD_REQUEST);
        });
    }

private:
    std::map<zerozero::SocketDescriptor, QSocketNotifier*> _notifiers;

    void onNewSocket(zerozero::SocketDescriptor fd) final;
    void onDeleteSocket(zerozero::SocketDescriptor fd) final;
    void _init();
};

#endif
