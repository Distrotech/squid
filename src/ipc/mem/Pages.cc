/*
 * $Id$
 *
 * DEBUG: section 54    Interprocess Communication
 *
 */

#include "config.h"
#include "base/TextException.h"
#include "ipc/mem/PagePool.h"
#include "ipc/mem/Pages.h"
#include "structs.h"
#include "SwapDir.h"

// Uses a single PagePool instance, for now.
// Eventually, we may have pools dedicated to memory caching, disk I/O, etc.

// TODO: make pool id more unique so it does not conflict with other Squids?
static const String PagePoolId = "squid-page-pool";
static Ipc::Mem::PagePool *ThePagePool = 0;

// TODO: make configurable to avoid waste when mem-cached objects are small/big
size_t
Ipc::Mem::PageSize() {
    return 32*1024;
}

void
Ipc::Mem::Init()
{
    Must(!ThePagePool);
    const size_t capacity = Limit() / PageSize();
    ThePagePool = new PagePool(PagePoolId, capacity, PageSize());
}

void
Ipc::Mem::Attach()
{
    Must(!ThePagePool);
    // TODO: make pool id more unique so it does not conflict with other Squid instances?
    ThePagePool = new PagePool(PagePoolId);
}

bool
Ipc::Mem::GetPage(PageId &page)
{
    return ThePagePool ? ThePagePool->get(page) : false;
}

void
Ipc::Mem::PutPage(PageId &page)
{
    Must(ThePagePool);
    ThePagePool->put(page);
}

void *
Ipc::Mem::PagePointer(const PageId &page)
{
    Must(ThePagePool);
    return ThePagePool->pagePointer(page);
}

size_t
Ipc::Mem::Limit()
{
    // TODO: adjust cache_mem description to say that in SMP mode,
    // in-transit objects are not allocated using cache_mem. Eventually,
    // they should not use cache_mem even if shared memory is not used:
    // in-transit objects have nothing to do with caching.
    return Config.memMaxSize;
}

// TODO: Implement size_t Ipc::Mem::Level()