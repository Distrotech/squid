#ifndef _SQUID_SRC_HTTP_ONE_PARSER_H
#define _SQUID_SRC_HTTP_ONE_PARSER_H

#include "anyp/ProtocolVersion.h"
#include "http/one/forward.h"
#include "SBuf.h"

namespace Http {
namespace One {

// Parser states
enum ParseState {
    HTTP_PARSE_NONE,     ///< initialized, but nothing usefully parsed yet
    HTTP_PARSE_FIRST,    ///< HTTP/1 message first-line
    HTTP_PARSE_MIME,     ///< HTTP/1 mime-header block
    HTTP_PARSE_DONE      ///< parsed a message header, or reached a terminal syntax error
};

/** HTTP/1.x protocol parser
 *
 * Works on a raw character I/O buffer and tokenizes the content into
 * the major CRLF delimited segments of an HTTP/1 procotol message:
 *
 * \item first-line (request-line / simple-request / status-line)
 * \item mime-header 0*( header-name ':' SP field-value CRLF)
 */
class Parser : public RefCountable
{
    explicit Parser(const Parser&); // do not implement
    Parser& operator =(const Parser&); // do not implement

public:
    Parser() { clear(); }
    virtual ~Parser() {}

    /// Set this parser back to a default state.
    /// Will DROP any reference to a buffer (does not free).
    virtual void clear();

    /// attempt to parse a message from the buffer
    /// \retval true if a full message was found and parsed
    /// \retval false if incomplete, invalid or no message was found
    virtual bool parse(const SBuf &aBuf) = 0;

    /** Whether the parser is waiting on more data to complete parsing a message.
     * Use to distinguish between incomplete data and error results
     * when parse() returns false.
     */
    bool needsMoreData() const {return parsingStage_!=HTTP_PARSE_DONE;}

    /// size in bytes of the first line including CRLF terminator
    virtual int64_t firstLineSize() const = 0;

    /// size in bytes of the message headers including CRLF terminator(s)
    /// but excluding first-line bytes
    int64_t headerBlockSize() const {return mimeHeaderBlock_.length();}

    /// size in bytes of HTTP message block, includes first-line and mime headers
    /// excludes any body/entity/payload bytes
    /// excludes any garbage prefix before the first-line
    int64_t messageHeaderSize() const {return firstLineSize() + headerBlockSize();}

    /// buffer containing HTTP mime headers, excluding message first-line.
    SBuf mimeHeader() const {return mimeHeaderBlock_;}

    /// the protocol label for this message
    const AnyP::ProtocolVersion & messageProtocol() const {return msgProtocol_;}

    /**
     * \return A pointer to a field-value of the first matching field-name, or NULL.
     */
    char *getHeaderField(const char *name);

public:
    SBuf buf;

protected:
    /// what stage the parser is currently up to
    ParseState parsingStage_;

    /// what protocol label has been found in the first line (if any)
    AnyP::ProtocolVersion msgProtocol_;

    /// buffer holding the mime headers (if any)
    SBuf mimeHeaderBlock_;
};

} // namespace One
} // namespace Http

#endif /*  _SQUID_SRC_HTTP_ONE_PARSER_H */
