#ifndef SQUID_TRANSIENTS_H
#define SQUID_TRANSIENTS_H

#include "http/MethodType.h"
#include "ipc/mem/Page.h"
#include "ipc/mem/PageStack.h"
#include "ipc/StoreMap.h"
#include "Store.h"

// StoreEntry restoration info not already stored by Ipc::StoreMap
struct TransientsMapExtras {
    char url[MAX_URL+1]; ///< Request-URI; TODO: decrease MAX_URL by one
    RequestFlags reqFlags; ///< request flags
    Http::MethodType reqMethod; ///< request method; extensions are not supported
};
typedef Ipc::StoreMapWithExtras<TransientsMapExtras> TransientsMap;

/// Keeps track of hits being delivered to clients that arrived before those
/// hits were [fully] cached. This shared table is necessary to synchronize hit
/// caching (writing) workers with other workers serving (reading) those hits.
class Transients: public Store, public Ipc::StoreMapCleaner
{
public:
    Transients();
    virtual ~Transients();

    /// add an in-transit entry suitable for collapsing future requests
    void startWriting(StoreEntry *e, const RequestFlags &reqFlags, const HttpRequestMethod &reqMethod);

    /// called when the in-transit entry has been successfully cached
    void completeWriting(const StoreEntry &e);

    /// the calling entry writer no longer expects to cache this entry
    void abandon(const StoreEntry &e);

    /// whether an in-transit entry is now abandoned by its writer
    bool abandoned(const StoreEntry &e) const;

    /// the caller is done writing or reading this entry
    void disconnect(MemObject &mem_obj);

    /* Store API */
    virtual int callback();
    virtual StoreEntry * get(const cache_key *);
    virtual void get(String const key , STOREGETCLIENT callback, void *cbdata);
    virtual void init();
    virtual uint64_t maxSize() const;
    virtual uint64_t minSize() const;
    virtual uint64_t currentSize() const;
    virtual uint64_t currentCount() const;
    virtual int64_t maxObjectSize() const;
    virtual void getStats(StoreInfoStats &stats) const;
    virtual void stat(StoreEntry &) const;
    virtual StoreSearch *search(String const url, HttpRequest *);
    virtual void reference(StoreEntry &);
    virtual bool dereference(StoreEntry &, bool);
    virtual void maintain();

    static int64_t EntryLimit();

protected:
    StoreEntry *copyFromShm(const sfileno index);
    bool copyToShm(const StoreEntry &e, const sfileno index, const RequestFlags &reqFlags, const HttpRequestMethod &reqMethod);

    bool abandonedAt(const sfileno index) const;

    // Ipc::StoreMapCleaner API
    virtual void noteFreeMapSlice(const sfileno sliceId);

private:
    TransientsMap *map; ///< index of mem-cached entries
};

// TODO: Why use Store as a base? We are not really a cache.

#endif /* SQUID_MEMSTORE_H */
