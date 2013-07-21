
/*
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
 *
 * Copyright (c) 2003, Robert Collins <robertc@squid-cache.org>
 */

#ifndef SQUID_CONFIGPARSER_H
#define SQUID_CONFIGPARSER_H

#include "SquidString.h"
#include <queue>
#include <stack>
#if HAVE_STRING
#include <string>
#endif

class wordlist;
/**
 * Limit to how long any given config line may be.
 * This affects squid.conf and all included files.
 *
 * Behaviour when setting larger than 2KB is unknown.
 * The config parser read mechanism can cope, but the other systems
 * receiving the data from its buffers on such lines may not.
 */
#define CONFIG_LINE_LIMIT	2048

/**
 * A configuration file Parser. Instances of this class track
 * parsing state and perform tokenisation. Syntax is currently
 * taken care of outside this class.
 *
 * One reason for this class is to allow testing of configuration
 * using modules without linking cache_cf.o in - because that drags
 * in all of squid by reference. Instead the tokeniser only is
 * brought in.
 */
class ConfigParser
{

public:
    /**
     * Parsed tokens type: simple tokens, quoted tokens or function
     * like parameters.
     */
    enum TokenType {SimpleToken, QuotedToken, FunctionNameToken};

    void destruct();
    static void ParseUShort(unsigned short *var);
    static void ParseBool(bool *var);
    static const char *QuoteString(const String &var);
    static void ParseWordList(wordlist **list);

    /**
     * Backward compatibility wrapper for the ConfigParser::NextToken method.
     * If the configuration_includes_quoted_values configuration parameter is
     * set to 'off' this interprets the quoted tokens as filenames.
     */
    static char * strtokFile();

    /**
     * Returns the body of the next element. The element is either a token or
     * a quoted string with optional escape sequences and/or macros. The body
     * of a quoted string element does not include quotes or escape sequences.
     * Future code will want to see Elements and not just their bodies.
     */
    static char *NextToken();

    /// \return true if the last parsed token was quoted
    static bool LastTokenWasQuoted() {return (LastTokenType == ConfigParser::QuotedToken);}

    /**
     * \return the next quoted string or the raw string data until the end of line.
     * This method allows %macros in unquoted strings to keep compatibility
     * for the logformat option.
     */
    static char *NextQuotedOrToEol();

    /**
     * Undo last NextToken call. The next call to NextToken() method will return
     * again the last parsed element.
     * Can not be called repeatedly to undo multiple NextToken calls. In this case
     * the behaviour is undefined.
     */
    static void TokenUndo();

    /**
     * The next NextToken call will return the token as next element
     * It can be used repeatedly to add more than one tokens in a FIFO list.
     */
    static void TokenPutBack(const char *token);

    /// Set the configuration file line to parse.
    static void SetCfgLine(char *line);

    /// Allow %macros inside quoted strings
    static void EnableMacros() {AllowMacros_ = true;}

    /// Do not allow %macros inside quoted strings
    static void DisableMacros() {AllowMacros_ = false;}

    /// configuration_includes_quoted_values in squid.conf
    static int RecognizeQuotedValues;

protected:
    /**
     * Class used to store required information for the current
     * configuration file.
     */
    class CfgFile
    {
    public:
        CfgFile(): wordFile(NULL), parsePos(NULL), lineNo(0) { parseBuffer[0] = '\0';}
        ~CfgFile();
        /// True if the configuration file is open
        bool isOpen() {return wordFile != NULL;}

        /**
         * Open the file given by 'path' and initializes the CfgFile object
         * to start parsing
         */
        bool startParse(char *path);

        /**
         * Do the next parsing step:
         * reads the next line from file if required.
         * \return the body of next element or a NULL pointer if there are no more token elements in the file.
         * \param type will be filled with the ConfigParse::TokenType for any element found, or left unchanged if NULL is returned.
         */
        char *parse(TokenType &type);

    private:
        bool getFileLine();   ///< Read the next line from the file
        /**
         * Return the body of the next element. If the wasQuoted is given
         * set to true if the element was quoted.
         */
        char *nextElement(TokenType &type);
        FILE *wordFile; ///< Pointer to the file.
        char parseBuffer[CONFIG_LINE_LIMIT]; ///< Temporary buffer to store data to parse
        char *parsePos; ///< The next element position in parseBuffer string
    public:
        std::string filePath; ///< The file path
        std::string currentLine; ///< The current line to parse
        int lineNo; ///< Current line number
    };

    /**
     * Return the last TokenUndo() or TokenPutBack() queued element, or NULL
     * if none exist
     */
    static char *Undo();

    /**
     * Unquotes the token, which must be quoted.
     * \param end if it is not NULL, it is set to the end of token.
     */
    static char *UnQuote(char *token, char **end = NULL);

    /**
     * Does the real tokens parsing job: Ignore comments, unquote an
     * element if required.
     * \return the next token, or NULL if there are no available tokens in the nextToken string.
     * \param nextToken updated to point to the pos after parsed token.
     * \param type      The token type
     * \param legacy    If it is true function-like parameters are not allowed
     */
    static char *TokenParse(char * &nextToken, TokenType &type, bool legacy = false);

    /// Wrapper method for TokenParse.
    static char *NextElement(TokenType &type, bool legacy = false);
    static std::stack<CfgFile *> CfgFiles; ///< The stack of open cfg files
    static TokenType LastTokenType; ///< The type of last parsed element
    static char *LastToken; ///< Points to the last parsed token
    static char *CfgLine; ///< The current line to parse
    static char *CfgPos; ///< Pointer to the next element in cfgLine string
    static std::queue<std::string> Undo_; ///< The list with TokenUndo() or TokenPutBack() queued elements
    static bool AllowMacros_;
};

int parseConfigFile(const char *file_name);

#endif /* SQUID_CONFIGPARSER_H */
