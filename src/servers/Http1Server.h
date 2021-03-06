/*
 * Copyright (C) 1996-2016 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_SRC_SERVERS_HTTP1SERVER_H
#define SQUID_SRC_SERVERS_HTTP1SERVER_H

#include "servers/forward.h"

namespace Http
{
namespace One
{

/// Manages a connection from an HTTP/1 or HTTP/0.9 client.
class Server: public ConnStateData
{
    CBDATA_CLASS(Server);

public:
    Server(const MasterXaction::Pointer &xact, const bool beHttpsServer);
    virtual ~Server() {}

    void readSomeHttpData();

protected:
    /* ConnStateData API */
    virtual Http::Stream *parseOneRequest();
    virtual void processParsedRequest(Http::Stream *context);
    virtual void handleReply(HttpReply *rep, StoreIOBuffer receivedData);
    virtual void writeControlMsgAndCall(HttpReply *rep, AsyncCall::Pointer &call);
    virtual time_t idleTimeout() const;

    /* BodyPipe API */
    virtual void noteMoreBodySpaceAvailable(BodyPipe::Pointer);
    virtual void noteBodyConsumerAborted(BodyPipe::Pointer);

    /* AsyncJob API */
    virtual void start();

    void proceedAfterBodyContinuation(Http::StreamPointer context);

private:
    void processHttpRequest(Http::Stream *const context);
    void handleHttpRequestData();

    /// Handles parsing results. May generate and deliver an error reply
    /// to the client if parsing is failed, or parses the url and build the
    /// HttpRequest object using parsing results.
    /// Return false if parsing is failed, true otherwise.
    bool buildHttpRequest(Http::Stream *context);

    Http1::RequestParserPointer parser_;
    HttpRequestMethod method_; ///< parsed HTTP method

    /// temporary hack to avoid creating a true HttpsServer class
    const bool isHttpsServer;
};

} // namespace One
} // namespace Http

#endif /* SQUID_SRC_SERVERS_HTTP1SERVER_H */

