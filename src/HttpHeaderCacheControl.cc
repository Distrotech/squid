
/*
 *
 * DEBUG: section 65    HTTP Cache Control Header
 * AUTHOR: Alex Rousskov, Francesco Chemolli
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include "squid.h"
#include "Store.h"
#include "HttpHeader.h"
#include "HttpHeaderCacheControl.h"

#if HAVE_MAP
#include <map>
#endif

/* this table is used for parsing cache control header and statistics */
typedef struct {
    const char *name;
    http_hdr_cc_type id;
    HttpHeaderFieldStat stat;
} HttpHeaderCcFields;

/* order must match that of enum http_hdr_cc_type. The constraint is verified at initialization time */
static HttpHeaderCcFields CcAttrs[CC_ENUM_END] = {
        {"public", CC_PUBLIC},
        {"private", CC_PRIVATE},
        {"no-cache", CC_NO_CACHE},
        {"no-store", CC_NO_STORE},
        {"no-transform", CC_NO_TRANSFORM},
        {"must-revalidate", CC_MUST_REVALIDATE},
        {"proxy-revalidate", CC_PROXY_REVALIDATE},
        {"max-age", CC_MAX_AGE},
        {"s-maxage", CC_S_MAXAGE},
        {"max-stale", CC_MAX_STALE},
        {"min-fresh", CC_MIN_FRESH},
        {"only-if-cached", CC_ONLY_IF_CACHED},
        {"stale-if-error", CC_STALE_IF_ERROR},
        {"Other,", CC_OTHER} /* ',' will protect from matches */
};

/// Map an header name to its type, to expedite parsing
typedef std::map<String,http_hdr_cc_type> HdrCcNameToIdMap_t;
static HdrCcNameToIdMap_t HdrCcNameToIdMap;

// iterate over a table of http_header_cc_type structs
http_hdr_cc_type &operator++ (http_hdr_cc_type &aHeader)
{
    int tmp = (int)aHeader;
    aHeader = (http_hdr_cc_type)(++tmp);
    return aHeader;
}


/* module initialization */

void
httpHdrCcInitModule(void)
{
    int32_t i;
    /* build lookup and accounting structures */
    for (i=0;i<CC_ENUM_END;i++) {
        assert(i==CcAttrs[i].id); /* verify assumption: the id is the key into the array */
        HdrCcNameToIdMap[CcAttrs[i].name]=CcAttrs[i].id;
    }
}

void
httpHdrCcCleanModule(void)
{
    // HdrCcNameToIdMap is self-cleaning
}

/* parses a 0-terminating string and inits cc */
bool
HttpHdrCc::parseInit(const String & str)
{
    const char *item;
    const char *p;		/* '=' parameter */
    const char *pos = NULL;
    http_hdr_cc_type type;
    int ilen;
    int nlen;

    /* iterate through comma separated list */

    while (strListGetItem(&str, ',', &item, &ilen, &pos)) {
        String tmpstr;
        /* isolate directive name */

        if ((p = (const char *)memchr(item, '=', ilen)) && (p - item < ilen))
            nlen = p++ - item;
        else
            nlen = ilen;

        /* find type */
        tmpstr.limitInit(item,nlen);
        HdrCcNameToIdMap_t::iterator i;
        i=HdrCcNameToIdMap.find(tmpstr);
        if (i==HdrCcNameToIdMap.end())
            type=CC_OTHER;
        else
            type=i->second;

        // ignore known duplicate directives
        if (EBIT_TEST(mask, type)) {
            if (type != CC_OTHER) {
                debugs(65, 2, "hdr cc: ignoring duplicate cache-directive: near '" << item << "' in '" << str << "'");
                CcAttrs[type].stat.repCount++;
                continue;
            }
        } else {
            EBIT_SET(mask, type);
        }

        /* post-processing special cases */
        switch (type) {

        case CC_MAX_AGE:

            if (!p || !httpHeaderParseInt(p, &max_age)) {
                debugs(65, 2, "cc: invalid max-age specs near '" << item << "'");
                max_age = -1;
                EBIT_CLR(mask, type);
            }

            break;

        case CC_S_MAXAGE:

            if (!p || !httpHeaderParseInt(p, &s_maxage)) {
                debugs(65, 2, "cc: invalid s-maxage specs near '" << item << "'");
                s_maxage = -1;
                EBIT_CLR(mask, type);
            }

            break;

        case CC_MAX_STALE:

            if (!p || !httpHeaderParseInt(p, &max_stale)) {
                debugs(65, 2, "cc: max-stale directive is valid without value");
                max_stale = -1;
            }

            break;

        case CC_MIN_FRESH:

            if (!p || !httpHeaderParseInt(p, &min_fresh)) {
                debugs(65, 2, "cc: invalid min-fresh specs near '" << item << "'");
                min_fresh = -1;
                EBIT_CLR(mask, type);
            }

            break;

        case CC_STALE_IF_ERROR:
            if (!p || !httpHeaderParseInt(p, &stale_if_error)) {
                debugs(65, 2, "cc: invalid stale-if-error specs near '" << item << "'");
                stale_if_error = -1;
                EBIT_CLR(mask, type);
            }
            break;

        case CC_OTHER:

            if (other.size())
                other.append(", ");

            other.append(item, ilen);

            break;

        default:
            /* note that we ignore most of '=' specs (RFCVIOLATION) */
            break;
        }
    }

    return (mask != 0);
}

void
httpHdrCcDestroy(HttpHdrCc * cc)
{
    assert(cc);
    delete cc;
}

void
httpHdrCcPackInto(const HttpHdrCc * cc, Packer * p)
{
    http_hdr_cc_type flag;
    int pcount = 0;
    assert(cc && p);

    for (flag = CC_PUBLIC; flag < CC_ENUM_END; ++flag) {
        if (EBIT_TEST(cc->mask, flag) && flag != CC_OTHER) {

            /* print option name */
            packerPrintf(p, (pcount ? ", %s": "%s") , CcAttrs[flag].name);

            /* handle options with values */

            if (flag == CC_MAX_AGE)
                packerPrintf(p, "=%d", (int) cc->max_age);

            if (flag == CC_S_MAXAGE)
                packerPrintf(p, "=%d", (int) cc->s_maxage);

            if (flag == CC_MAX_STALE && cc->max_stale >= 0)
                packerPrintf(p, "=%d", (int) cc->max_stale);

            if (flag == CC_MIN_FRESH)
                packerPrintf(p, "=%d", (int) cc->min_fresh);

            pcount++;
        }
    }

    if (cc->other.size() != 0)
        packerPrintf(p, (pcount ? ", " SQUIDSTRINGPH : SQUIDSTRINGPH),
                     SQUIDSTRINGPRINT(cc->other));
}

/* negative max_age will clean old max_Age setting */
void
httpHdrCcSetMaxAge(HttpHdrCc * cc, int max_age)
{
    assert(cc);
    cc->max_age = max_age;

    if (max_age >= 0)
        EBIT_SET(cc->mask, CC_MAX_AGE);
    else
        EBIT_CLR(cc->mask, CC_MAX_AGE);
}

/* negative s_maxage will clean old s-maxage setting */
void
httpHdrCcSetSMaxAge(HttpHdrCc * cc, int s_maxage)
{
    assert(cc);
    cc->s_maxage = s_maxage;

    if (s_maxage >= 0)
        EBIT_SET(cc->mask, CC_S_MAXAGE);
    else
        EBIT_CLR(cc->mask, CC_S_MAXAGE);
}

void
httpHdrCcUpdateStats(const HttpHdrCc * cc, StatHist * hist)
{
    http_hdr_cc_type c;
    assert(cc);

    for (c = CC_PUBLIC; c < CC_ENUM_END; ++c)
        if (EBIT_TEST(cc->mask, c))
            statHistCount(hist, c);
}

void
httpHdrCcStatDumper(StoreEntry * sentry, int idx, double val, double size, int count)
{
    extern const HttpHeaderStat *dump_stat;	/* argh! */
    const int id = (int) val;
    const int valid_id = id >= 0 && id < CC_ENUM_END;
    const char *name = valid_id ? CcAttrs[id].name : "INVALID";

    if (count || valid_id)
        storeAppendPrintf(sentry, "%2d\t %-20s\t %5d\t %6.2f\n",
                          id, name, count, xdiv(count, dump_stat->ccParsedCount));
}
