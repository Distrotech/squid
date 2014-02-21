#define SQUID_UNIT_TEST 1
#include "squid.h"

#include <cppunit/TestAssert.h>

#include "Mem.h"
#include "testURL.h"
#include "URL.h"

#include <sstream>

CPPUNIT_TEST_SUITE_REGISTRATION( testURL );

/* init memory pools */

void
testURL::setUp()
{
    Mem::Init();
}

/*
 * we can construct a URL with a AnyP::UriScheme.
 * This creates a URL for that scheme.
 */
void
testURL::testConstructScheme()
{
    AnyP::UriScheme empty_scheme;
    URL protoless_url(AnyP::PROTO_NONE);
    CPPUNIT_ASSERT_EQUAL(empty_scheme, protoless_url.getScheme());

    AnyP::UriScheme ftp_scheme(AnyP::PROTO_FTP);
    URL ftp_url(AnyP::PROTO_FTP);
    CPPUNIT_ASSERT_EQUAL(ftp_scheme, ftp_url.getScheme());
}

/*
 * a default constructed URL has scheme "NONE".
 * Also, we should be able to use new and delete on
 * scheme instances.
 */
void
testURL::testDefaultConstructor()
{
    AnyP::UriScheme aScheme;
    URL aUrl;
    CPPUNIT_ASSERT_EQUAL(aScheme, aUrl.getScheme());

    URL *urlPointer = new URL;
    CPPUNIT_ASSERT(urlPointer != NULL);
    delete urlPointer;
}
