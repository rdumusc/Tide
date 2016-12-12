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

#include "WebsocketPublisher.h"

#include <QtWebSockets/qwebsocketserver.h>
#include <QtWebSockets/qwebsocket.h>

#include <QDebug>
#include <QBuffer>
#include <QPixmap>

namespace
{
QByteArray _toJpeg( const QImage& image )
{
    QByteArray data;
    QBuffer buffer{ &data };
    buffer.open( QIODevice::WriteOnly );
    image.save( &buffer, "JPG" );
    buffer.close();
    return data;
}
const QSize maxSize = { 512, 512 };
}

QT_USE_NAMESPACE

WebsocketPublisher::WebsocketPublisher( quint16 port )
    : _websocketServer( new QWebSocketServer( QStringLiteral( "Echo Server" ),
                                              QWebSocketServer::NonSecureMode,
                                              this ))
{
    if( _websocketServer->listen( QHostAddress::Any, port ))
    {
        if( _debug )
            qDebug() << "WebsocketPublisher listening on port" << port;

        connect( _websocketServer, &QWebSocketServer::newConnection,
                 this, &WebsocketPublisher::onNewConnection );
        connect( _websocketServer, &QWebSocketServer::closed,
                 this, &WebsocketPublisher::closed );
    }
}

WebsocketPublisher::~WebsocketPublisher()
{
    _websocketServer->close();
    qDeleteAll( _clients.begin(), _clients.end( ));
}

void WebsocketPublisher::publish( const QImage image )
{
    const auto jpeg = _toJpeg( image.scaled( maxSize, Qt::KeepAspectRatio ));
    const auto message = jpeg.toBase64();
    for( auto client : _clients )
        client->sendBinaryMessage( message );
}

void WebsocketPublisher::onNewConnection()
{
    auto client = _websocketServer->nextPendingConnection();

    if( _debug )
        qDebug() << "socketConnected:" << client;

    connect( client, &QWebSocket::textMessageReceived,
             this, &WebsocketPublisher::processTextMessage );
    connect( client, &QWebSocket::binaryMessageReceived,
             this, &WebsocketPublisher::processBinaryMessage );
    connect( client, &QWebSocket::disconnected,
             this, &WebsocketPublisher::socketDisconnected );

    _clients << client;
}

void WebsocketPublisher::processTextMessage( const QString message )
{
    if( _debug )
        qDebug() << "Message received:" << message;

    if( auto client = qobject_cast<QWebSocket*>( sender( )))
        client->sendTextMessage( message );
}

void WebsocketPublisher::processBinaryMessage( const QByteArray message )
{
    if( _debug )
        qDebug() << "Binary Message received:" << message;

    if( auto client = qobject_cast<QWebSocket*>( sender( )))
        client->sendBinaryMessage( message );
}

void WebsocketPublisher::socketDisconnected()
{
    if( auto client = qobject_cast<QWebSocket*>( sender( )))
    {
        if( _debug )
            qDebug() << "socketDisconnected:" << client;

        _clients.removeAll( client );
        client->deleteLater();
    }
}
