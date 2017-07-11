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

#include "JsonRpc.h"

#include "json.h"

#include <QJsonDocument>

#include <map>

namespace
{
const QJsonObject parseError{{"code", -32700}, {"message", "Parse error"}};
const QJsonObject invalidRequest{{"code", -32600},
                                 {"message", "Invalid Request"}};
const QJsonObject methodNotFound{{"code", -32601},
                                 {"message", "Method not found"}};

QJsonObject _makeErrorResponse(const QJsonObject& error,
                               const QJsonValue& id = QJsonValue())
{
    return QJsonObject{{"jsonrpc", "2.0"}, {"error", error}, {"id", id}};
}

QJsonObject _makeErrorResponse(const int code, const QString& message,
                               const QJsonValue& id = QJsonValue())
{
    return _makeErrorResponse({{"code", code}, {"message", message}}, id);
}

QJsonObject _makeResponse(const QString& result, const QJsonValue& id)
{
    return QJsonObject{{"jsonrpc", "2.0"}, {"result", result}, {"id", id}};
}

bool _isValidJsonRpc(const QJsonObject& object)
{
    return object["jsonrpc"].toString() == "2.0";
}

bool _isValidJsonRpcRequest(const QJsonObject& object)
{
    const auto params = object["params"];
    return object["method"].isString() &&
           (params.isUndefined() || params.isObject() || params.isArray()) &&
           (object["id"].isUndefined() || object["id"].isDouble());
}

std::string _getAsString(const QJsonValue& params)
{
    if (params.isObject())
        return json::toString(params.toObject());
    if (params.isArray())
        return json::toString(params.toArray());
    return std::string();
}

std::string _toString(const QJsonObject& response)
{
    return response.isEmpty() ? std::string() : json::toString(response);
}
} // anonymous namespace

class JsonRpc::Impl
{
public:
    QJsonObject process(const QJsonObject& request)
    {
        if (!_isValidJsonRpc(request))
            return _makeErrorResponse(parseError);

        if (!_isValidJsonRpcRequest(request))
            return _makeErrorResponse(invalidRequest);

        const auto id = request["id"];
        const auto action =
            methods.find(request["method"].toString().toStdString());
        if (action == methods.end())
            return _makeErrorResponse(methodNotFound, id);

        const auto rep = action->second(_getAsString(request["params"]));

        // No reply for notifications (request objects without an "id" member)
        if (id.isUndefined())
            return {};

        if (rep.error != 0)
            return _makeErrorResponse(rep.error,
                                      QString::fromStdString(rep.result), id);

        return _makeResponse(QString::fromStdString(rep.result), id);
    }

    std::map<std::string, JsonRpc::ResponseCallback> methods;
};

JsonRpc::JsonRpc()
    : _impl{new Impl}
{
}

JsonRpc::~JsonRpc()
{
}

void JsonRpc::bind(const std::string& method, ResponseCallback action)
{
    _impl->methods[method] = action;
}

void JsonRpc::notify(const std::string& method, NotifyCallback action)
{
    bind(method, [action](const std::string& request) {
        action(request);
        return JsonRpc::Response{"OK"};
    });
}

void JsonRpc::notify(const std::string& method, VoidCallback action)
{
    bind(method, [action](const std::string&) {
        action();
        return JsonRpc::Response{"OK"};
    });
}

std::string JsonRpc::process(const std::string& request)
{
    const auto input = QByteArray::fromRawData(request.c_str(), request.size());
    const auto doc = QJsonDocument::fromJson(input);

    if (doc.isObject())
        return _toString(_impl->process(doc.object()));

    if (doc.isArray())
    {
        const auto array = doc.array();
        if (array.isEmpty())
            return json::toString(_makeErrorResponse(invalidRequest));

        QJsonArray responses;
        for (auto entry : array)
        {
            if (entry.isObject())
            {
                const auto response = _impl->process(entry.toObject());
                if (!response.isEmpty())
                    responses.append(response);
            }
            else
                responses.append(_makeErrorResponse(invalidRequest));
        }
        return responses.empty() ? std::string() : json::toString(responses);
    }

    return json::toString(_makeErrorResponse(parseError));
}
