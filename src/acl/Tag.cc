/*
 * Copyright (C) 1996-2014 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#include "squid.h"
#include "acl/Checklist.h"
#include "acl/StringData.h"
#include "acl/Tag.h"
#include "HttpRequest.h"

int
ACLTagStrategy::match (ACLData<MatchType> * &data, ACLFilledChecklist *checklist, ACLFlags &)
{
    if (checklist->request != NULL)
        return data->match (checklist->request->tag.termedBuf());
    return 0;
}

ACLTagStrategy *
ACLTagStrategy::Instance()
{
    return &Instance_;
}

ACLTagStrategy ACLTagStrategy::Instance_;

