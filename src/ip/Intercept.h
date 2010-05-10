/*
 * DEBUG: section 89    NAT / IP Interception
 * AUTHOR: Robert Collins
 * AUTHOR: Amos Jeffries
 *
 */
#ifndef SQUID_IP_IPINTERCEPT_H
#define SQUID_IP_IPINTERCEPT_H

/* for time_t */
#include "SquidTime.h"

namespace Ip
{

class Address;

/**
 \defgroup IpInterceptAPI IP Interception and Transparent Proxy API
 \ingroup SquidComponent
 \par
 * There is no formal state-machine for transparency and interception
 * instead there is this neutral API which other connection state machines
 * and the comm layer use to co-ordinate their own state for transparency.
 */
class Intercept
{
public:
    Intercept() : transparent_active(0), intercept_active(0), last_reported(0) {};
    ~Intercept() {};

    /** Perform NAT lookups */
    int NatLookup(int fd, const Address &me, const Address &peer, Address &client, Address &dst);

    /**
     * Test system networking calls for TPROXY support.
     * Detects IPv6 and IPv4 level of support matches the address being listened on
     * and if the compiled v2/v4 is usable as far down as a bind()ing.
     *
     * \param test    Address set on the http(s)_port being checked.
     * \retval true   TPROXY is available.
     * \retval false  TPROXY is not available.
     */
    bool ProbeForTproxy(Address &test);

    /**
     \retval 0	Full transparency is disabled.
     \retval 1  Full transparency is enabled and active.
     */
    inline int TransparentActive() { return transparent_active; };

    /** \par
     * Turn on fully Transparent-Proxy activities.
     * This function should be called during parsing of the squid.conf
     * When any option requiring full-transparency is encountered.
     */
    inline void StartTransparency() { transparent_active=1; };

    /** \par
     * Turn off fully Transparent-Proxy activities on all new connections.
     * Existing transactions and connections are unaffected and will run
     * to their natural completion.
     \param str    Reason for stopping. Will be logged to cache.log
     */
    void StopTransparency(const char *str);

    /**
     \retval 0	IP Interception is disabled.
     \retval 1  IP Interception is enabled and active.
     */
    inline int InterceptActive() { return intercept_active; };

    /** \par
     * Turn on IP-Interception-Proxy activities.
     * This function should be called during parsing of the squid.conf
     * When any option requiring interception / NAT handling is encountered.
     */
    inline void StartInterception() { intercept_active=1; };

    /** \par
     * Turn off IP-Interception-Proxy activities on all new connections.
     * Existing transactions and connections are unaffected and will run
     * to their natural completion.
     \param str    Reason for stopping. Will be logged to cache.log
     */
    inline void StopInterception(const char *str);


private:

    /**
     * perform Lookups on Netfilter interception targets (REDIRECT, DNAT).
     *
     \param silent   0 if errors are to be displayed. 1 if errors are to be hidden.
     \param fd       FD for the current TCP connection being tested.
     \param me       IP address Squid received the connection on
     \param client   IP address from which Squid received the connection.
     *               May be updated by the NAT table information.
     *               Default is the same value as the me IP address.
     \retval 0     Successfuly located the new address.
     \retval -1    An error occured during NAT lookups.
     */
    int NetfilterInterception(int fd, const Address &me, Address &client, int silent);

    /**
     * perform Lookups on Netfilter fully-transparent interception targets (TPROXY).
     *
     \param silent   0 if errors are to be displayed. 1 if errors are to be hidden.
     \param fd       FD for the current TCP connection being tested.
     \param me       IP address Squid received the connection on
     \param dst      IP address to which the request was made.
     *               expected to be updated from the NAT table information.
     *               Default is the same value as the peer IP address sent to NatLookup().
     \retval 0     Successfuly located the new address.
     \retval -1    An error occured during NAT lookups.
     */
    int NetfilterTransparent(int fd, const Address &me, Address &dst, int silent);

    /**
     * perform Lookups on IPFW interception.
     *
     \param silent   0 if errors are to be displayed. 1 if errors are to be hidden.
     \param fd       FD for the current TCP connection being tested.
     \param me       IP address Squid received the connection on
     \param client   IP address from which Squid received the connection.
     *               May be updated by the NAT table information.
     *               Default is the same value as the me IP address.
     \retval 0     Successfuly located the new address.
     \retval -1    An error occured during NAT lookups.
     */
    int IpfwInterception(int fd, const Address &me, Address &client, int silent);

    /**
     * perform Lookups on IPF interception.
     *
     \param silent   0 if errors are to be displayed. 1 if errors are to be hidden.
     \param fd       FD for the current TCP connection being tested.
     \param me       IP address Squid received the connection on
     \param client   IP address from which Squid received the connection.
     *               May be updated by the NAT table information.
     *               Default is the same value as the me IP address.
     \param dst      IP address to which the request was made.
     *               expected to be updated from the NAT table information.
     *               Default is the same value as the peer IP address sent to NatLookup().
     \retval 0     Successfuly located the new address.
     \retval -1    An error occured during NAT lookups.
     */
    int IpfInterception(int fd, const Address &me, Address &client, Address &dst, int silent);

    /**
     * perform Lookups on PF interception.
     *
     \param silent   0 if errors are to be displayed. 1 if errors are to be hidden.
     \param fd       FD for the current TCP connection being tested.
     \param me       IP address Squid received the connection on
     \param client   IP address from which Squid received the connection.
     *               May be updated by the NAT table information.
     *               Default is the same value as the me IP address.
     \param dst      IP address to which the request was made.
     *               expected to be updated from the NAT table information.
     *               Default is the same value as the peer IP address sent to NatLookup().
     \retval 0     Successfuly located the new address.
     \retval -1    An error occured during NAT lookups.
     */
    int PfInterception(int fd, const Address &me, Address &client, Address &dst, int silent);


    int transparent_active;
    int intercept_active;
    time_t last_reported; /**< Time of last error report. Throttles NAT error display to 1 per minute */
};

#if LINUX_NETFILTER && !defined(IP_TRANSPARENT)
/// \ingroup IpInterceptAPI
#define IP_TRANSPARENT 19
#endif

/**
 \ingroup IpInterceptAPI
 * Globally available instance of the IP Interception manager.
 */
extern Intercept Interceptor;

}; // namespace Ip

#endif /* SQUID_IP_IPINTERCEPT_H */