
/*
 * $Id: StoreMetaMD5.cc,v 1.2 2003/02/21 22:50:06 robertc Exp $
 *
 * DEBUG: section 20    Storage Manager Swapfile Metadata
 * AUTHOR: Kostas Anagnostakis
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
#include "StoreMetaMD5.h"
#include "Store.h"
#include "MemObject.h"

MemPool *StoreMetaMD5::pool = NULL;

void *
StoreMetaMD5::operator new (size_t byteCount)
{
    /* derived classes with different sizes must implement their own new */
    assert (byteCount == sizeof (StoreMetaMD5));

    if (!pool)
        pool = memPoolCreate("StoreMetaMD5", sizeof (StoreMetaMD5));

    return memPoolAlloc(pool);
}

void
StoreMetaMD5::operator delete (void *address)
{
    memPoolFree(pool, address);
}

void
StoreMetaMD5::deleteSelf()
{
    delete this;
}

bool
StoreMetaMD5::validLength(int len) const
{
    return len == MD5_DIGEST_CHARS;
}

int StoreMetaMD5::md5_mismatches = 0;

bool
StoreMetaMD5::checkConsistency(StoreEntry *e) const
{
    assert (getType() == STORE_META_KEY_MD5);
    assert(length == MD5_DIGEST_CHARS);

    if (!EBIT_TEST(e->flags, KEY_PRIVATE) &&
            memcmp(value, e->key, MD5_DIGEST_CHARS)) {
        debug(20, 2) ("storeClientReadHeader: swapin MD5 mismatch\n");
        //	debug(20, 2) ("\t%s\n", storeKeyText((const cache_key *)value));
        debug(20, 2) ("\t%s\n", e->getMD5Text());

        if (isPowTen(++md5_mismatches))
            debug(20, 1) ("WARNING: %d swapin MD5 mismatches\n",
                          md5_mismatches);

        return false;
    }

    return true;
}
