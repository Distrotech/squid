#ifndef SQUID_FS_ROCK_SWAP_DIR_H
#define SQUID_FS_ROCK_SWAP_DIR_H

#include "SwapDir.h"
#include "DiskIO/IORequestor.h"
#include "fs/rock/RockFile.h"
#include "ipc/StoreMap.h"

class DiskIOStrategy;
class DiskFile;
class ReadRequest;
class WriteRequest;

namespace Rock
{

class Rebuild;

/// \ingroup Rock
class SwapDir: public ::SwapDir, public IORequestor
{
public:
    SwapDir();
    virtual ~SwapDir();

    /* public ::SwapDir API */
    virtual void reconfigure(int, char *);
    virtual StoreSearch *search(String const url, HttpRequest *);
    virtual StoreEntry *get(const cache_key *key);
    virtual void disconnect(StoreEntry &e);
    virtual uint64_t currentSize() const;
    virtual uint64_t currentCount() const;
    virtual bool doReportStat() const;
    virtual void swappedOut(const StoreEntry &e);

    int64_t entryLimitHigh() const { return 0xFFFFFF; } /// Core sfileno maximum
    int64_t entryLimitAllowed() const;

    typedef Ipc::StoreMapWithExtras<DbCellHeader> DirMap;

protected:
    /* protected ::SwapDir API */
    virtual bool needsDiskStrand() const;
    virtual void create();
    virtual void init();
    virtual bool canStore(const StoreEntry &e, int64_t diskSpaceNeeded, int &load) const;
    virtual StoreIOState::Pointer createStoreIO(StoreEntry &, StoreIOState::STFNCB *, StoreIOState::STIOCB *, void *);
    virtual StoreIOState::Pointer openStoreIO(StoreEntry &, StoreIOState::STFNCB *, StoreIOState::STIOCB *, void *);
    virtual void maintain();
    virtual void diskFull();
    virtual void reference(StoreEntry &e);
    virtual bool dereference(StoreEntry &e);
    virtual void unlink(StoreEntry &e);
    virtual void statfs(StoreEntry &e) const;

    /* IORequestor API */
    virtual void ioCompletedNotification();
    virtual void closeCompleted();
    virtual void readCompleted(const char *buf, int len, int errflag, RefCount< ::ReadRequest>);
    virtual void writeCompleted(int errflag, size_t len, RefCount< ::WriteRequest>);

    virtual void parse(int index, char *path);
    void parseSize(); ///< parses anonymous cache_dir size option
    void validateOptions(); ///< warns of configuration problems; may quit

    void rebuild(); ///< starts loading and validating stored entry metadata
    ///< used to add entries successfully loaded during rebuild
    bool addEntry(const int fileno, const DbCellHeader &header, const StoreEntry &from);

    bool full() const; ///< no more entries can be stored without purging
    void trackReferences(StoreEntry &e); ///< add to replacement policy scope
    void ignoreReferences(StoreEntry &e); ///< delete from repl policy scope

    int64_t diskOffset(int filen) const;
    int64_t diskOffsetLimit() const;
    int entryLimit() const { return map->entryLimit(); }

    friend class Rebuild;
    const char *filePath; ///< location of cache storage file inside path/

private:
    DiskIOStrategy *io;
    RefCount<DiskFile> theFile; ///< cache storage for this cache_dir
    DirMap *map;

    static const int64_t HeaderSize; ///< on-disk db header size
};

} // namespace Rock

#endif /* SQUID_FS_ROCK_SWAP_DIR_H */
