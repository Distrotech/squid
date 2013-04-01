/*
 * DEBUG: section 83    SSL accelerator support
 *
 */

#include "squid.h"
#include "ssl/support.h"

/* support.cc says this is needed */
#if USE_SSL

#include "comm.h"
#include "ip/Address.h"
#include "fde.h"
#include "globals.h"
#include "Mem.h"
#include "ssl/bio.h"
#if HAVE_OPENSSL_SSL_H
#include <openssl/ssl.h>
#endif

#undef DO_SSLV23

// TODO: fde.h should probably export these for wrappers like ours
extern int default_read_method(int, char *, int);
extern int default_write_method(int, const char *, int);
#if _SQUID_WINDOWS_
extern int socket_read_method(int, char *, int);
extern int socket_write_method(int, const char *, int);
#endif

/* BIO callbacks */
static int squid_bio_write(BIO *h, const char *buf, int num);
static int squid_bio_read(BIO *h, char *buf, int size);
static int squid_bio_puts(BIO *h, const char *str);
//static int squid_bio_gets(BIO *h, char *str, int size);
static long squid_bio_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int squid_bio_create(BIO *h);
static int squid_bio_destroy(BIO *data);
/* SSL callbacks */
static void squid_ssl_info(const SSL *ssl, int where, int ret);

/// Initialization structure for the BIO table with
/// Squid-specific methods and BIO method wrappers.
static BIO_METHOD SquidMethods = {
    BIO_TYPE_SOCKET,
    "squid",
    squid_bio_write,
    squid_bio_read,
    squid_bio_puts,
    NULL, // squid_bio_gets not supported
    squid_bio_ctrl,
    squid_bio_create,
    squid_bio_destroy,
    NULL // squid_callback_ctrl not supported
};

BIO *
Ssl::Bio::Create(const int fd, Ssl::Bio::Type type)
{
    if (BIO *bio = BIO_new(&SquidMethods)) {
        BIO_int_ctrl(bio, BIO_C_SET_FD, type, fd);
        return bio;
    }
    return NULL;
}

void
Ssl::Bio::Link(SSL *ssl, BIO *bio)
{
    SSL_set_bio(ssl, bio, bio); // cannot fail
    SSL_set_info_callback(ssl, &squid_ssl_info); // does not provide diagnostic
}


Ssl::Bio::Bio(const int anFd): fd_(anFd)
{
    debugs(83, 7, "Bio constructed, this=" << this << " FD " << fd_);
}

Ssl::Bio::~Bio()
{
    debugs(83, 7, "Bio destructing, this=" << this << " FD " << fd_);
}

int Ssl::Bio::write(const char *buf, int size, BIO *table)
{
    errno = 0;
#if _SQUID_WINDOWS_
    const int result = socket_write_method(fd_, buf, size);
#else
    const int result = default_write_method(fd_, buf, size);
#endif
    const int xerrno = errno;
    debugs(83, 5, "FD " << fd_ << " wrote " << result << " <= " << size);

    BIO_clear_retry_flags(table);
    if (result < 0) {
        const bool ignoreError = ignoreErrno(xerrno) != 0;
        debugs(83, 5, "error: " << xerrno << " ignored: " << ignoreError);
        if (ignoreError)
            BIO_set_retry_write(table);
    }

    return result;
}

int
Ssl::Bio::read(char *buf, int size, BIO *table)
{
    errno = 0;
#if _SQUID_WINDOWS_
    const int result = socket_read_method(fd_, buf, size);
#else
    const int result = default_read_method(fd_, buf, size);
#endif
    const int xerrno = errno;
    debugs(83, 5, "FD " << fd_ << " read " << result << " <= " << size);

    BIO_clear_retry_flags(table);
    if (result < 0) {
        const bool ignoreError = ignoreErrno(xerrno) != 0;
        debugs(83, 5, "error: " << xerrno << " ignored: " << ignoreError);
        if (ignoreError)
            BIO_set_retry_read(table);
    }

    return result;
}

/// Called whenever the SSL connection state changes, an alert appears, or an
/// error occurs. See SSL_set_info_callback().
void
Ssl::Bio::stateChanged(const SSL *ssl, int where, int ret)
{
    // Here we can use (where & STATE) to check the current state.
    // Many STATE values are possible, including: SSL_CB_CONNECT_LOOP,
    // SSL_CB_ACCEPT_LOOP, SSL_CB_HANDSHAKE_START, and SSL_CB_HANDSHAKE_DONE.
    // For example:
    // if (where & SSL_CB_HANDSHAKE_START)
    //    debugs(83, 9, "Trying to establish the SSL connection");
    // else if (where & SSL_CB_HANDSHAKE_DONE)
    //    debugs(83, 9, "SSL connection established");

    debugs(83, 7, "FD " << fd_ << " now: 0x" << std::hex << where << std::dec << ' ' <<
           SSL_state_string(ssl) << " (" << SSL_state_string_long(ssl) << ")");
}

bool
Ssl::ClientBio::isClientHello(int state)
{
    return (state == SSL2_ST_GET_CLIENT_HELLO_A ||
            state == SSL3_ST_SR_CLNT_HELLO_A ||
            state == SSL23_ST_SR_CLNT_HELLO_A ||
            state == SSL23_ST_SR_CLNT_HELLO_B ||
            state == SSL3_ST_SR_CLNT_HELLO_B ||
            state == SSL3_ST_SR_CLNT_HELLO_C
        );
}

void 
Ssl::ClientBio::stateChanged(const SSL *ssl, int where, int ret)
{
    Ssl::Bio::stateChanged(ssl, where, ret);
}

int
Ssl::ClientBio::write(const char *buf, int size, BIO *table)
{
    if (holdWrite_) {
        BIO_set_retry_write(table);
        return 0;
    }

    return Ssl::Bio::write(buf, size, table);
}

const char *objToString(unsigned char const *bytes, int len)
{
    static std::string buf;
    buf.clear();
    for(int i = 0; i < len; i++ ) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%.2x", bytes[i]);
        buf.append(tmp);
    }
    return buf.c_str();
}

int
Ssl::ClientBio::read(char *buf, int size, BIO *table)
{
    if (headerState < 2) {

        if (rbuf.isNull())
            rbuf.init(1024, 4096);

        size = rbuf.spaceSize() > size ? size : rbuf.spaceSize();

        if (!size)
            return 0;

        int bytes = Ssl::Bio::read(buf, size, table);
        if (!bytes)
            return 0;
        rbuf.append(buf, bytes);
        debugs(83, 7, "rbuf size: " << rbuf.contentSize());
    }

    if (headerState == 0) {

        const unsigned char *head = (const unsigned char *)rbuf.content();
        const char *s = objToString(head, rbuf.contentSize());
        debugs(83, 7, "SSL Header: " << s);
        if (rbuf.contentSize() < 5) {
            BIO_set_retry_read(table);
            return 0;
        }

        if (head[0] == 0x16) {
            debugs(83, 7, "SSL version 3 handshake message");
            headerBytes = (head[3] << 8) + head[4];
            debugs(83, 7, "SSL Header Size: " << headerBytes);
#ifdef DO_SSLV23
        } else if ((head[0] & 0x80) && head[2] == 0x01 && head[3] == 0x03) { 
            debugs(83, 7, "SSL version 2 handshake message with v3 support");
            headerBytes = head[1];
#endif
        }else {
            debugs(83, 7, "Not an SSL acceptable handshake message (SSLv2 message?)");
            return -1;
        }

        headerState = 1; //Next state
    }

    if (headerState == 1) {
        const unsigned char *head = (const unsigned char *)rbuf.content();
        const char *s = objToString(head, rbuf.contentSize());
        debugs(83, 7, "SSL Header: " << s);

        if (headerBytes > rbuf.contentSize()) {
            BIO_set_retry_read(table);
            return -1;
        }
        features.get((const unsigned char *)rbuf.content());
        headerState = 2;
    }

    if (holdRead_) {
        debugs(83, 7, "Hold flag is set, retry latter. (Hold " << size << "bytes)");
        BIO_set_retry_read(table);
        return -1;
    }

    if (headerState >=2) {
        if (rbuf.hasContent()) {
            int bytes = (size <= rbuf.contentSize() ? size : rbuf.contentSize());
            memcpy(buf, rbuf.content(), bytes);
            rbuf.consume(bytes);
            return bytes;
        } else
            return Ssl::Bio::read(buf, size, table);
    }

    return -1;
}

void
Ssl::ServerBio::stateChanged(const SSL *ssl, int where, int ret)
{
    Ssl::Bio::stateChanged(ssl, where, ret);
}

void
Ssl::ServerBio::setClientRandom(const unsigned char *r)
{
    memcpy(clientRandom, r, SSL3_RANDOM_SIZE);
    randomSet = true;
};

int
Ssl::ServerBio::read(char *buf, int size, BIO *table)
{
    int bytes = Ssl::Bio::read(buf, size, table);

    if (bytes > 0 && record_) {
        if (rbuf.isNull())
            rbuf.init(1024, 8196);
        rbuf.append(buf, bytes);
    }
    return bytes;
}

int
Ssl::ServerBio::write(const char *buf, int size, BIO *table)
{

    if (holdWrite_) {
        debugs(83, 7,  "Hold write, for SSL connection on " << fd_);
        BIO_set_retry_write(table);
        return -1;
    }

    if (!helloBuild) {
        if (
            buf[1] >= 3  //it is an SSL Version3 message
            && buf[0] == 0x16 // and it is a Handshake/Hello message
            ) {
            if (helloMsg.isNull())
                helloMsg.init(1024, 4096);

            //Hello message is the first message we write to server
            assert(!helloMsg.hasContent());

            SSL *ssl = fd_table[fd_].ssl;
            if (randomSet && ssl && ssl->s3) {
                assert(size > 11 + SSL3_RANDOM_SIZE);
                helloMsg.append(buf, 11);
                //The random number is stored in the 11 position of the 
                // message we are going to sent
                helloMsg.append((char *)clientRandom, SSL3_RANDOM_SIZE);
                size_t len = size - 11 - SSL3_RANDOM_SIZE;
                helloMsg.append(buf + 11 + SSL3_RANDOM_SIZE, len);

                // We need to fix the random in SSL struct:
                memcpy(ssl->s3->client_random, clientRandom, SSL3_RANDOM_SIZE);
                // We also need to fix the raw message in SSL struct
                // stored in SSL->init_buf. Looks that it is used to get
                // digest of the previous sent SSL message, to compute keys
                // for encryption/decryption:
                memcpy(ssl->init_buf->data + 6, clientRandom, SSL3_RANDOM_SIZE);

                debugs(83, 7,  "SSL HELLO message for FD " << fd_ << ": Random number is adjusted");
            }
        }
        helloBuild = true;
        helloMsgSize = helloMsg.contentSize();
    }

    if (helloMsg.hasContent()) {
        debugs(83, 7,  "buffered write for FD " << fd_);
        int ret = Ssl::Bio::write(helloMsg.content(), helloMsg.contentSize(), table);
        helloMsg.consume(ret);
        if (helloMsg.hasContent()) {
            // We need to retry sendind data.
            // Say to openSSL to retry sending hello message
            BIO_set_retry_write(table);
            return -1;
        }

        // Sending hello message complete. Do not send more data for now...
        holdWrite_ = true; 
        // The size should be less than the size of the hello message
        assert(size >= helloMsgSize);
        return helloMsgSize;
    } else
        return Ssl::Bio::write(buf, size, table);
}

void
Ssl::ServerBio::flush(BIO *table)
{
    if (helloMsg.hasContent()) {
        int ret = Ssl::Bio::write(helloMsg.content(), helloMsg.contentSize(), table);
        helloMsg.consume(ret);
    }
}

/// initializes BIO table after allocation
static int
squid_bio_create(BIO *bi)
{
    bi->init = 0; // set when we store Bio object and socket fd (BIO_C_SET_FD)
    bi->num = 0;
    bi->ptr = NULL;
    bi->flags = 0;
    return 1;
}

/// cleans BIO table before deallocation
static int
squid_bio_destroy(BIO *table)
{
    delete static_cast<Ssl::Bio*>(table->ptr);
    table->ptr = NULL;
    return 1;
}

/// wrapper for Bio::write()
static int
squid_bio_write(BIO *table, const char *buf, int size)
{
    Ssl::Bio *bio = static_cast<Ssl::Bio*>(table->ptr);
    assert(bio);
    return bio->write(buf, size, table);
}

/// wrapper for Bio::read()
static int
squid_bio_read(BIO *table, char *buf, int size)
{
    Ssl::Bio *bio = static_cast<Ssl::Bio*>(table->ptr);
    assert(bio);
    return bio->read(buf, size, table);
}

/// implements puts() via write()
static int
squid_bio_puts(BIO *table, const char *str)
{
    assert(str);
    return squid_bio_write(table, str, strlen(str));
}

/// other BIO manipulations (those without dedicated callbacks in BIO table)
static long
squid_bio_ctrl(BIO *table, int cmd, long arg1, void *arg2)
{
    debugs(83, 5, table << ' ' << cmd << '(' << arg1 << ", " << arg2 << ')');

    switch (cmd) {
    case BIO_C_SET_FD: {
        assert(arg2);
        const int fd = *static_cast<int*>(arg2);
        Ssl::Bio *bio;
        if (arg1 == Ssl::Bio::BIO_TO_SERVER)
            bio = new Ssl::ServerBio(fd);
        else
            bio = new Ssl::ClientBio(fd);
        assert(!table->ptr);
        table->ptr = bio;
        table->init = 1;
        return 0;
    }

    case BIO_C_GET_FD:
        if (table->init) {
            Ssl::Bio *bio = static_cast<Ssl::Bio*>(table->ptr);
            assert(bio);
            if (arg2)
                *static_cast<int*>(arg2) = bio->fd();
            return bio->fd();
        }
        return -1;

    case BIO_CTRL_DUP:
        // Should implemented if the SSL_dup openSSL API function 
        // used anywhere in squid.
        return 0;

    case BIO_CTRL_FLUSH:
        if (table->init) {
            Ssl::Bio *bio = static_cast<Ssl::Bio*>(table->ptr);
            assert(bio);
            bio->flush(table);
            return 1;
        }
        return 0;

/*  we may also need to implement these:
    case BIO_CTRL_RESET:
    case BIO_C_FILE_SEEK:
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
    case BIO_CTRL_GET_CLOSE:
    case BIO_CTRL_SET_CLOSE:
    case BIO_CTRL_PENDING:
    case BIO_CTRL_WPENDING:
*/
    default:
        return 0;

    }

    return 0; /* NOTREACHED */
}

/// wrapper for Bio::stateChanged()
static void
squid_ssl_info(const SSL *ssl, int where, int ret)
{
    if (BIO *table = SSL_get_rbio(ssl)) {
        if (Ssl::Bio *bio = static_cast<Ssl::Bio*>(table->ptr))
            bio->stateChanged(ssl, where, ret);
    }
}

Ssl::Bio::sslFeatures::sslFeatures(): sslVersion(-1), compressMethod(-1)
{
    memset(client_random, 0, SSL3_RANDOM_SIZE);
}

int Ssl::Bio::sslFeatures::toSquidSSLVersion() const
{
    if(sslVersion == SSL2_VERSION)
        return 2;
    else if(sslVersion == SSL3_VERSION)
        return 3;
    else if(sslVersion == TLS1_VERSION)
        return 4;
#if OPENSSL_VERSION_NUMBER >= 0x10001000L
    else if(sslVersion == TLS1_1_VERSION)
        return 5;
    else if(sslVersion == TLS1_2_VERSION)
        return 6;
#endif
    else
        return 1;
}

bool
Ssl::Bio::sslFeatures::get(const SSL *ssl)
{
    sslVersion = SSL_version(ssl);
    debugs(83, 7, "SSL version: " << SSL_get_version(ssl) << " (" << sslVersion << ")");

    if(const char *server = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))
        serverName = server;
    debugs(83, 7, "SNI server name: " << serverName);

    if (ssl->session->compress_meth)
            compressMethod = ssl->session->compress_meth;
    else if(sslVersion >= 3) //if it is 3 or newer version then compression is disabled
        compressMethod = 0;
    debugs(83, 7, "SSL compression: " << compressMethod);

    STACK_OF(SSL_CIPHER) * ciphers = NULL;
    if (ssl->server)
        ciphers = ssl->session->ciphers;
    else
        ciphers = ssl->cipher_list;
    if (ciphers) {
        for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); ++i) {
            SSL_CIPHER *c = sk_SSL_CIPHER_value(ciphers, i);
            if (c != NULL) {
                if(!clientRequestedCiphers.empty())
                    clientRequestedCiphers.append(":");
                clientRequestedCiphers.append(c->name);
            }
        }
    }
    debugs(83, 7, "Ciphers requested by client: " << clientRequestedCiphers);

    if (sslVersion >=3 && ssl->s3 && ssl->s3->client_random[0]) {
        memcpy(client_random, ssl->s3->client_random, SSL3_RANDOM_SIZE);
    }

#if 0 /* XXX: OpenSSL 0.9.8k lacks at least some of these tlsext_* fields */
    //The following extracted for logging purpuses:
    // TLSEXT_TYPE_ec_point_formats
    unsigned char *p;
    int len;
    if (ssl->server) {
        p = ssl->session->tlsext_ecpointformatlist;
        len = ssl->session->tlsext_ecpointformatlist_length;
    } else {
        p = ssl->tlsext_ecpointformatlist;
        len = ssl->tlsext_ecpointformatlist_length;
    }
    if (p) {
        ecPointFormatList = objToString(p, len);
        debugs(83, 7, "tlsExtension ecPointFormatList of length " << len << " :" << ecPointFormatList);
    }

    // TLSEXT_TYPE_elliptic_curves
    if (ssl->server) {
        p = ssl->session->tlsext_ellipticcurvelist;
        len = ssl->session->tlsext_ellipticcurvelist_length;
    } else {
        p = ssl->tlsext_ellipticcurvelist;
        len = ssl->tlsext_ellipticcurvelist_length;
    }
    if (p) {
        ellipticCurves = objToString(p, len);
        debugs(83, 7, "tlsExtension ellipticCurveList of length " <<  len <<" :" << ellipticCurves);
    }
    // TLSEXT_TYPE_opaque_prf_input
    p = NULL;
    if (ssl->server) {
        if (ssl->s3 &&  ssl->s3->client_opaque_prf_input) {
            p = (unsigned char *)ssl->s3->client_opaque_prf_input;
            len = ssl->s3->client_opaque_prf_input_len;
        }
    } else {
        p = (unsigned char *)ssl->tlsext_opaque_prf_input;
        len = ssl->tlsext_opaque_prf_input_len;
    }
    if (p) {
        debugs(83, 7, "tlsExtension client-opaque-prf-input of length " << len);
        opaquePrf = objToString(p, len);
    }
#endif
    return true;
}

bool
Ssl::Bio::sslFeatures::get(const unsigned char *hello)
{
    // The SSL handshake message should starts with a 0x16 byte
    if (hello[0] == 0x16) {
        return parseV3Hello(hello);
#ifdef DO_SSLV23
    } else if ((hello[0] & 0x80) && hello[2] == 0x01 && hello[3] == 0x03) {
        return parseV23Hello(hello);
#endif
    }
    
    debugs(83, 7, "Not a known SSL handshake message");
    return false;
}

bool
Ssl::Bio::sslFeatures::parseV3Hello(const unsigned char *hello)
{
    debugs(83, 7, "Get fake features from v3 hello message.");
    // The SSL version exist in the 2nd and 3rd bytes
    sslVersion = (hello[1] << 8) | hello[2];
    debugs(83, 7, "Get fake features. Version :" << std::hex << std::setw(8) << std::setfill('0')<< sslVersion);

    // The following hello message size exist in 4th and 5th bytes
    int helloSize = (hello[3] << 8) | hello[4];
    helloSize += 5; //Include the 5 header bytes.

    //For SSLv3 or TLSv1.* protocols we can get some more informations
    if (hello[1] == 0x3 && hello[5] == 0x1 /*HELLO A message*/) {
        // Get the correct version of the sub-hello message
        sslVersion = (hello[9] << 8) | hello[10];
        //Get Client Random number. It starts on the position 11 of hello message
        memcpy(client_random, hello + 11, SSL3_RANDOM_SIZE);
        debugs(83, 7, "Client random: " <<  objToString(client_random, SSL3_RANDOM_SIZE));

        // At the position 43 (11+SSL3_RANDOM_SIZE)
        int sessIDLen = (int)hello[43];
        debugs(83, 7, "Session ID Length: " <<  sessIDLen);

        //Ciphers list. It is stored after the Session ID.
        const unsigned char *ciphers = hello + 44 + sessIDLen;
        int ciphersLen = (ciphers[0] << 8) | ciphers[1];
        ciphers += 2;
        if (ciphersLen) {
            const SSL_METHOD *method = SSLv3_method();
            int cs = method->put_cipher_by_char(NULL, NULL);
            assert(cs > 0);
            for (int i = 0; i < ciphersLen; i += cs) {
                const SSL_CIPHER *c = method->get_cipher_by_char((ciphers + i));
                if (c != NULL) {
                    if(!clientRequestedCiphers.empty())
                        clientRequestedCiphers.append(":");
                    clientRequestedCiphers.append(c->name);
                }
            }
        }
        debugs(83, 7, "Ciphers requested by client: " << clientRequestedCiphers);

        // Compression field: 1 bytes the number of compression methods and
        // 1 byte for each compression method
        const unsigned char *compression = ciphers + ciphersLen;
        if (compression[0] > 1)
            compressMethod = 1;
        else
            compressMethod = 0;
        debugs(83, 7, "SSL compression methods number: " << (int)compression[0]);

        const unsigned char *extensions = compression + 1 + (int)compression[0];
        if (extensions <  hello + helloSize) { 
            int extensionsLen = (extensions[0] << 8) | extensions[1];
            const unsigned char *ext = extensions + 2;
            while (ext < extensions+extensionsLen){
                short extType = (ext[0] << 8) | ext[1];
                ext += 2;
                short extLen = (ext[0] << 8) | ext[1];
                ext += 2;
                debugs(83, 7, "SSL Exntension: " << std::hex << extType << " of size:" << extLen);
                //The SNI extension has the type 0 (extType == 0)
                // The two first bytes indicates the length of the SNI data (should be extLen-2)
                // The next byte is the hostname type, it should be '0' for normal hostname (ext[2] == 0)
                // The 3rd and 4th bytes are the length of the hostname
                if (extType == 0 && ext[2] == 0) {
                    int hostLen = (ext[3] << 8) | ext[4];
                    serverName.assign((const char *)(ext+5), hostLen);
                    debugs(83, 7, "Found server name: " << serverName);
                }
                ext += extLen;
            }
        }
    }
    return true;
}

bool
Ssl::Bio::sslFeatures::parseV23Hello(const unsigned char *hello)
{
#ifdef DO_SSLV23
    debugs(83, 7, "Get fake features from v23 hello message.");
    sslVersion = (hello[3] << 8) | hello[4];
    debugs(83, 7, "Get fake features. Version :" << std::hex << std::setw(8) << std::setfill('0')<< sslVersion);

    // The following hello message size exist in 2nd byte
    int helloSize = hello[1];
    helloSize += 2; //Include the 2 header bytes.

    //Ciphers list. It is stored after the Session ID.

    int ciphersLen = (hello[5] << 8) | hello[6];
    const unsigned char *ciphers = hello + 11;
    if (ciphersLen) {
        const SSL_METHOD *method = SSLv23_method();
        int cs = method->put_cipher_by_char(NULL, NULL);
        assert(cs > 0);
        for (int i = 0; i < ciphersLen; i += cs) {
            // The v2 hello messages cipher has 3 bytes.
            // The v2 cipher has the first byte not null
            // Because we are going to sent only v3 message we 
            // are ignoring these ciphers
            if (ciphers[i] != 0)
                continue;
            const SSL_CIPHER *c = method->get_cipher_by_char((ciphers + i + 1));
            if (c != NULL) {
                if(!clientRequestedCiphers.empty())
                    clientRequestedCiphers.append(":");
                clientRequestedCiphers.append(c->name);
            }
        }
    }
    debugs(83, 7, "Ciphers requested by client: " << clientRequestedCiphers);    

    //Get Client Random number. It starts on the position 11 of hello message
    memcpy(client_random, ciphers + ciphersLen, SSL3_RANDOM_SIZE);
    debugs(83, 7, "Client random: " <<  objToString(client_random, SSL3_RANDOM_SIZE));

    compressMethod = 0;
    return true;
#else
    return false;
#endif
}

std::ostream &
Ssl::Bio::sslFeatures::print(std::ostream &os) const
{
    static std::string buf;
    return os << "v" << sslVersion << 
        " SNI:" << (serverName.empty() ? "-" : serverName) <<
        " comp:" << compressMethod <<
        " Ciphers:" << clientRequestedCiphers <<
        " Random:" << objToString(client_random, SSL3_RANDOM_SIZE) <<
        " ecPointFormats:" << ecPointFormatList <<
        " ec:" << ellipticCurves <<
        " opaquePrf:" << opaquePrf;
}

#endif /* USE_SSL */
