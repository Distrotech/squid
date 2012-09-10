/*
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
#ifndef SQUID_STRUCTS_H
#define SQUID_STRUCTS_H

#include "cbdata.h"
#include "defines.h"
#include "dlink.h"
#include "hash.h"
#include "ip/Address.h"

/* needed for the global config */
#include "HttpHeader.h"
#include "HttpHeaderTools.h"

#define PEER_MULTICAST_SIBLINGS 1

class ACLChecklist;

#if SQUID_SNMP

#include "snmp_session.h"
class snmp_request_t {
public:
    u_char *buf;
    u_char *outbuf;
    int len;
    int sock;
    long reqid;
    int outlen;

    Ip::Address from;

    struct snmp_pdu *PDU;
    ACLChecklist *acl_checklist;
    u_char *community;

    struct snmp_session session;
};

#endif

class ACLList;

struct acl_tos {
    acl_tos *next;
    ACLList *aclList;
    tos_t tos;
};

struct acl_nfmark {
    acl_nfmark *next;
    ACLList *aclList;
    nfmark_t nfmark;
};

struct acl_size_t {
    acl_size_t *next;
    ACLList *aclList;
    int64_t size;
};

#if USE_DELAY_POOLS
#include "DelayConfig.h"
#include "ClientDelayConfig.h"
#endif

#if USE_ICMP
#include "icmp/IcmpConfig.h"
#endif

#include "HelperChildConfig.h"

/* forward decl for SquidConfig, see RemovalPolicy.h */

class CpuAffinityMap;
class RemovalPolicySettings;
class external_acl;
class Store;
class CustomLog;
class CacheMgrPasswd;
namespace AnyP
{
struct PortCfg;
}
class SwapDir;

class close_handler {
public:
    PF *handler;
    void *data;
    close_handler *next;
};

class dread_ctrl {
public:
    int fd;
    off_t offset;
    int req_len;
    char *buf;
    int end_of_file;
    DRCB *handler;
    void *client_data;
};

class dwrite_q {
public:
    off_t file_offset;
    char *buf;
    size_t len;
    size_t buf_offset;
    dwrite_q *next;
    FREE *free_func;
};

struct _fde_disk {
    DWCB *wrt_handle;
    void *wrt_handle_data;
    dwrite_q *write_q;
    dwrite_q *write_q_tail;
    off_t offset;
};


class http_state_flags {
public:
    unsigned int proxying:1;
    unsigned int keepalive:1;
    unsigned int only_if_cached:1;
    unsigned int handling1xx:1; ///< we are ignoring or forwarding 1xx response
    unsigned int headers_parsed:1;
    unsigned int front_end_https:2;
    unsigned int originpeer:1;
    unsigned int keepalive_broken:1;
    unsigned int abuse_detected:1;
    unsigned int request_sent:1;
    unsigned int do_next_read:1;
    unsigned int consume_body_data:1;
    unsigned int chunked:1; ///< reading a chunked response; TODO: rename
    unsigned int chunked_request:1; ///< writing a chunked request
    unsigned int sentLastChunk:1; ///< do not try to write last-chunk again
};

class domain_ping {
public:
    char *domain;
    int do_ping;		/* boolean */
    domain_ping *next;
};

class domain_type {
public:
    char *domain;
    peer_t type;
    domain_type *next;
};

class PeerDigest;


class netdbEntry;

class net_db_name {
public:
    hash_link hash;		/* must be first */
    net_db_name *next;
    netdbEntry *net_db_entry;
};

class net_db_peer {
public:
    const char *peername;
    double hops;
    double rtt;
    time_t expires;
};

class netdbEntry {
public:
    hash_link hash;		/* must be first */
    char network[MAX_IPSTRLEN];
    int pings_sent;
    int pings_recv;
    double hops;
    double rtt;
    time_t next_ping_time;
    time_t last_use_time;
    int link_count;
    net_db_name *hosts;
    net_db_peer *peers;
    int n_peers_alloc;
    int n_peers;
};

class iostats {
public:
    static const int histSize=16;

//    enum { histSize = 16 };

    struct {
        int reads;
        int reads_deferred;
        int read_hist[histSize];
        int writes;
        int write_hist[histSize];
    }

    Http, Ftp, Gopher;
};

struct request_flags {
    request_flags(): range(0),nocache(0),ims(0),auth(0),cachable(0),hierarchical(0),loopdetect(0),proxy_keepalive(0),proxying(0),refresh(0),redirected(0),need_validation(0),fail_on_validation_err(0),stale_if_hit(0),accelerated(0),ignore_cc(0),intercepted(0),hostVerified(0),spoof_client_ip(0),internal(0),internalclient(0),must_keepalive(0),pinned(0),canRePin(0),chunked_reply(0),stream_error(0),sslPeek(0),sslBumped(0),destinationIPLookedUp_(0) {
#if USE_HTTP_VIOLATIONS
        nocache_hack = 0;
#endif
#if FOLLOW_X_FORWARDED_FOR
        done_follow_x_forwarded_for = 0;
#endif /* FOLLOW_X_FORWARDED_FOR */
    }

    unsigned int range:1;
    unsigned int nocache:1;            ///< whether the response to this request may be READ from cache
    unsigned int ims:1;
    unsigned int auth:1;
    unsigned int cachable:1;           ///< whether the response to thie request may be stored in the cache
    unsigned int hierarchical:1;
    unsigned int loopdetect:1;
    unsigned int proxy_keepalive:1;
unsigned int proxying:
    1;	/* this should be killed, also in httpstateflags */
    unsigned int refresh:1;
    unsigned int redirected:1;
    unsigned int need_validation:1;
    unsigned int fail_on_validation_err:1; ///< whether we should fail if validation fails
    unsigned int stale_if_hit:1; ///< reply is stale if it is a hit
#if USE_HTTP_VIOLATIONS
    unsigned int nocache_hack:1;	/* for changing/ignoring no-cache requests */
#endif
    unsigned int accelerated:1;
    unsigned int ignore_cc:1;
    unsigned int intercepted:1;        ///< intercepted request
    unsigned int hostVerified:1;       ///< whether the Host: header passed verification
    unsigned int spoof_client_ip:1;  /**< spoof client ip if possible */
    unsigned int internal:1;
    unsigned int internalclient:1;
    unsigned int must_keepalive:1;
    unsigned int connection_auth:1; /** Request wants connection oriented auth */
    unsigned int connection_auth_disabled:1; /** Connection oriented auth can not be supported */
    unsigned int connection_proxy_auth:1; /** Request wants connection oriented auth */
    unsigned int pinned:1;      /* Request sent on a pinned connection */
    unsigned int canRePin:1; ///< OK to reopen a failed pinned connection
    unsigned int auth_sent:1;   /* Authentication forwarded */
    unsigned int no_direct:1;	/* Deny direct forwarding unless overriden by always_direct. Used in accelerator mode */
    unsigned int chunked_reply:1; /**< Reply with chunked transfer encoding */
    unsigned int stream_error:1; /**< Whether stream error has occured */
    unsigned int sslPeek:1; ///< internal ssl-bump request to get server cert
    unsigned int sslBumped:1; /**< ssl-bumped request*/

    // When adding new flags, please update cloneAdaptationImmune() as needed.

    bool resetTCP() const;
    void setResetTCP();
    void clearResetTCP();
    void destinationIPLookupCompleted();
    bool destinationIPLookedUp() const;

    // returns a partial copy of the flags that includes only those flags
    // that are safe for a related (e.g., ICAP-adapted) request to inherit
    request_flags cloneAdaptationImmune() const;

#if FOLLOW_X_FORWARDED_FOR
    unsigned int done_follow_x_forwarded_for;
#endif /* FOLLOW_X_FORWARDED_FOR */
private:

    unsigned int reset_tcp:1;
    unsigned int destinationIPLookedUp_:1;
};

#if USE_SSL
struct _sslproxy_cert_sign {
    int alg;
    ACLList *aclList;
    sslproxy_cert_sign *next;
};

struct _sslproxy_cert_adapt {
    int alg;
    char *param;
    ACLList *aclList;
    sslproxy_cert_adapt *next;
};
#endif

#endif /* SQUID_STRUCTS_H */
