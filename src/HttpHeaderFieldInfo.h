/*
 * Copyright (C) 1996-2015 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_HTTPHEADERFIELDINFO_H_
#define SQUID_HTTPHEADERFIELDINFO_H_

#include "http/RegisteredHeaders.h"
#include "HttpHeaderFieldStat.h"
#include "SquidString.h"

/// compiled version of HttpHeaderFieldAttrs plus stats. Currently a POD.
class HttpHeaderFieldInfo
{
public:
    HttpHeaderFieldInfo() : id(Http::HdrType::ACCEPT), type(field_type::ftInvalid) {}

    Http::HdrType id;
    String name;
    field_type type;
    HttpHeaderFieldStat stat;
};

#endif /* SQUID_HTTPHEADERFIELDINFO_H_ */

