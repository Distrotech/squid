/*
 * Copyright (C) 1996-2015 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_SRC_PIPELINE_H
#define SQUID_SRC_PIPELINE_H

#include "base/RefCount.h"

#include <list>

class ClientSocketContext;
typedef RefCount<ClientSocketContext> ClientSocketContextPointer;

/**
 * A queue of requests awaiting completion.
 *
 * Requests in the queue may be fully processed, but not yet delivered,
 * or only partially processed.
 *
 * - HTTP/1 pipelined requests can be processed out of order but
 *   responses MUST be written to the client in-order.
 *
 * - HTTP/2 multiplexed streams (aka requests) can be processed
 *   and delivered in any order.
 *
 * For consistency we treat the pipeline as a FIFO queue in both cases.
 */
class Pipeline
{
    Pipeline(const Pipeline &) = delete;
    Pipeline & operator =(const Pipeline &) = delete;

public:
    Pipeline() : nrequests(0) {}
    ~Pipeline() {terminateAll(0);}

    /// register a new request context to the pipeline
    void add(const ClientSocketContextPointer &);

    /// get the first request context in the pipeline
    ClientSocketContextPointer front() const;

    /// how many requests are currently pipelined
    size_t count() const {return requests.size();}

    /// whether there are none or any requests currently pipelined
    bool empty() const {return requests.empty();}

    /// tell everybody about the err, and abort all waiting requests
    void terminateAll(const int xerrno);

    /// deregister the front request from the pipeline
    void pop();

    /// Number of requests seen in this pipeline (so far).
    /// Includes incomplete transactions.
    uint32_t nrequests;

private:
    /// requests parsed from the connection but not yet completed.
    std::list<ClientSocketContextPointer> requests;
};

#endif /* SQUID_SRC_PIPELINE_H */

