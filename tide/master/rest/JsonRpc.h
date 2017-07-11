/*********************************************************************/
/* Copyright (c) 2017, EPFL/Blue Brain Project                       */
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

#ifndef JSONRPC_H
#define JSONRPC_H

#include <functional>
#include <memory>
#include <string>

/**
 * JSON-RPC 2.0 request processor.
 *
 * See specification: http://www.jsonrpc.org/specification
 */
class JsonRpc
{
public:
    /**
     * Response to a well-formed RPP request.
     */
    struct Response
    {
        std::string result;
        int error = 0;

        Response() = default;
        Response(std::string&& res)
            : result(res)
        {
        }
        Response(std::string&& res, const int err)
            : result(res)
            , error{err}
        {
        }

        static Response invalidParams() { return {"Invalid params", -32602}; }
    };

    /** @name Callback methods that can be registered. */
    //@{
    using ResponseCallback = std::function<Response(const std::string&)>;
    using NotifyCallback = std::function<void(const std::string&)>;
    using VoidCallback = std::function<void()>;
    //@}

    /** Constructor. */
    JsonRpc();

    /** Destructor. */
    ~JsonRpc();

    /**
     * Bind a method to a response callback.
     *
     * @param method to register.
     * @param action to perform.
     */
    void bind(const std::string& method, ResponseCallback action);

    /**
     * Bind a method to a response callback with a parameters object.
     *
     * @param method to register.
     * @param action to perform.
     */
    template <typename Params>
    void bind(const std::string& method, std::function<Response(Params)> action)
    {
        bind(method, [action](const std::string& request) {
            Params params;
            if (!from_json(params, request))
                return JsonRpc::Response::invalidParams();
            return action(std::move(params));
        });
    }

    /**
     * Bind a method to a callback with parameter but without a response.
     *
     * @param method to register.
     * @param action to perform.
     */
    template <typename Params>
    void notify(const std::string& method, std::function<void(Params)> action)
    {
        bind(method, [action](const std::string& request) {
            Params params;
            if (!from_json(params, request))
                return JsonRpc::Response::invalidParams();
            action(std::move(params));
            return JsonRpc::Response{"OK"};
        });
    }

    /**
     * Bind a method to a callback without a response.
     *
     * @param method to register.
     * @param action to perform.
     */
    void notify(const std::string& method, NotifyCallback action);

    /**
     * Bind a method to a callback with no response and payload.
     *
     * @param method to register.
     * @param action to perform.
     */
    void notify(const std::string& method, VoidCallback action);

    /**
     * Process a json request.
     *
     * @param request string in JSON-RPC 2.0 format.
     * @return json response string in JSON-RPC 2.0 format.
     */
    std::string process(const std::string& request);

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif
