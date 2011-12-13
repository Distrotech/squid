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
 *  AUTHOR: Francesco Chemolli
 */

#ifndef STATHIST_H_
#define STATHIST_H_

#include "typedefs.h"

/** Generic histogram class
 *
 * see important comments on hbase_f restrictions in StatHist.c
 */
class StatHist {
public:
    /** Default constructor
     *
     * \note the default constructor doesn't fully initialize.
     *       you have to call one of the *init functions to specialize the
     *       histogram
     * \todo specialize the class in a small hierarchy so that all
     *       relevant initializations are done at build-time
     */
    StatHist() : scale(1.0) {};
    StatHist(const StatHist&);
    StatHist &operator=(const StatHist &);
    virtual ~StatHist();
    /** clear the contents of the histograms
     *
     * \todo remove: this function has been replaced in its purpose
     *       by the destructor
     */
    void clear();

    /** Calculate the percentile for value pctile for the difference between
     *  this and the supplied histogram.
     */
    double deltaPctile(const StatHist &B, double pctile) const;
    /** obtain the output-transformed value from the specified bin
     *
     */
    double val(int bin) const;
    /** increment the counter for the histogram entry
     * associated to the supplied value
     */
    void count(double val);
    /** iterate the supplied bd function over the histogram values
     */
    void dump(StoreEntry *sentry, StatHistBinDumper * bd) const;
    /** Initialize the Histogram using a logarithmic values distribution
     *
     */
    void logInit(int capacity, double min, double max);
    /** initialize the histogram to count occurrences in an enum-represented set
     *
     */
    void enumInit(int last_enum);
protected:
    /// low-level initialize function
    void init(int capacity, hbase_f * val_in, hbase_f * val_out, double min, double max);
    int findBin(double v);
    int *bins;
    int capacity;
    double min;
    double max;
    double scale;
    hbase_f *val_in;        /* e.g., log() for log-based histogram */
    hbase_f *val_out;       /* e.g., exp() for log based histogram */
};

/* StatHist */
void statHistCount(StatHist * H, double val);
double statHistDeltaMedian(const StatHist & A, const StatHist & B);
double statHistDeltaPctile(const StatHist & A, const StatHist & B, double pctile);
StatHistBinDumper statHistEnumDumper;
StatHistBinDumper statHistIntDumper;

#endif /* STATHIST_H_ */
