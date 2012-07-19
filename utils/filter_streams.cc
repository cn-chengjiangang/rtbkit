/* filter_streams.cc
   Jeremy Barnes, 17 March 2005
   Copyright (c) 2005 Jeremy Barnes.  All rights reserved.
   
   This file is part of "Jeremy's Machine Learning Library", copyright (c)
   1999-2005 Jeremy Barnes.
   
   This program is available under the GNU General Public License, the terms
   of which are given by the file "license.txt" in the top level directory of
   the source code distribution.  If this file is missing, you have no right
   to use the program; please contact the author.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   ---
   
   Implementation of filter streams.
*/

#include "filter_streams.h"
#include <fstream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/version.hpp>
#include "jml/arch/exception.h"
#include "jml/arch/format.h"
#include <errno.h>
#include "lzma.h"


using namespace std;


namespace ML {


/*****************************************************************************/
/* FILTER_OSTREAM                                                            */
/*****************************************************************************/

filter_ostream::filter_ostream()
    : ostream(std::cout.rdbuf())
{
}

filter_ostream::
filter_ostream(const std::string & file, std::ios_base::openmode mode,
               const std::string & compression, int level)
    : ostream(std::cout.rdbuf())
{
    open(file, mode, compression, level);
}

filter_ostream::
filter_ostream(int fd, std::ios_base::openmode mode,
               const std::string & compression, int level)
    : ostream(std::cout.rdbuf())
{
    open(fd, mode);
}

namespace {

bool ends_with(const std::string & str, const std::string & what)
{
    string::size_type result = str.rfind(what);
    return result != string::npos
        && result == str.size() - what.size();
}

} // file scope

void filter_ostream::
open(const std::string & file_, std::ios_base::openmode mode,
     const std::string & compression, int level)
{
    exceptions(ios::badbit | ios::failbit);

    using namespace boost::iostreams;

    string file = file_;
    if (file == "") file = "/dev/null";

    auto_ptr<filtering_ostream> new_stream
        (new filtering_ostream());

    if (compression == "gz" || compression == "gzip"
        || (compression == ""
            && (ends_with(file, ".gz") || ends_with(file, ".gz~")))) {
        if (level == -1)
            new_stream->push(gzip_compressor());
        else new_stream->push(gzip_compressor(level));
    }
    else if (compression == "bz2" || compression == "bzip2"
        || (compression == ""
            && (ends_with(file, ".bz2") || ends_with(file, ".bz2~")))) {
        if (level == -1)
            new_stream->push(bzip2_compressor());
        else new_stream->push(bzip2_compressor(level));
    }
    else if (compression == "lzma" || compression == "xz"
        || (compression == ""
            && (ends_with(file, ".xz") || ends_with(file, ".xz~")))) {
        if (level == -1)
            new_stream->push(lzma_compressor());
        else new_stream->push(lzma_compressor(level));
    }
    else if (compression != "" && compression != "none")
        throw ML::Exception("unknown filter compression " + compression);
    
    if (file == "-") {
        new_stream->push(std::cout);
    }
    else {
        file_sink sink(file.c_str(), mode);
        if (!sink.is_open())
            throw Exception("couldn't open file " + file);
        new_stream->push(sink);
    }

    stream.reset(new_stream.release());
    rdbuf(stream->rdbuf());

    //stream.reset(new ofstream(file.c_str(), mode));
}

void filter_ostream::
open(int fd, std::ios_base::openmode mode,
     const std::string & compression, int level)
{
    exceptions(ios::badbit | ios::failbit);

    using namespace boost::iostreams;
    
    auto_ptr<filtering_ostream> new_stream
        (new filtering_ostream());

    if (compression == "gz" || compression == "gzip") {
        if (level == -1)
            new_stream->push(gzip_compressor());
        else new_stream->push(gzip_compressor(level));
    }
    else if (compression == "bz2" || compression == "bzip2") {
        if (level == -1)
            new_stream->push(bzip2_compressor());
        else new_stream->push(bzip2_compressor(level));
    }
    else if (compression == "lzma" || compression == "xz") {
        if (level == -1)
            new_stream->push(lzma_compressor());
        else new_stream->push(lzma_compressor(level));
    }
    else if (compression != "" && compression != "none")
        throw ML::Exception("unknown filter compression " + compression);
    
#if (BOOST_VERSION < 104100)
    new_stream->push(file_descriptor_sink(fd));
#else
    new_stream->push(file_descriptor_sink(fd,
                                          boost::iostreams::never_close_handle));
#endif
    stream.reset(new_stream.release());
    rdbuf(stream->rdbuf());

    //stream.reset(new ofstream(file.c_str(), mode));
}

void
filter_ostream::
close()
{
    rdbuf(0);
    //stream->close();
}

std::string
filter_ostream::
status() const
{
    if (*this) return "good";
    else return format("%s%s%s",
                       fail() ? " fail" : "",
                       bad() ? " bad" : "",
                       eof() ? " eof" : "");
}


/*****************************************************************************/
/* FILTER_ISTREAM                                                            */
/*****************************************************************************/

filter_istream::filter_istream()
    : istream(std::cin.rdbuf())
{
}

filter_istream::
filter_istream(const std::string & file, std::ios_base::openmode mode)
    : istream(std::cin.rdbuf())
{
    open(file, mode);
}

void filter_istream::
open(const std::string & file_, std::ios_base::openmode mode)
{
    using namespace boost::iostreams;

    string file = file_;
    if (file == "") file = "/dev/null";
    
    auto_ptr<filtering_istream> new_stream
        (new filtering_istream());

    bool gzip = (ends_with(file, ".gz") || ends_with(file, ".gz~"));
    bool bzip2 = (ends_with(file, ".bz2") || ends_with(file, ".bz2~"));
    bool lzma  = (ends_with(file, ".xz") || ends_with(file, ".xz~"));

    if (gzip) new_stream->push(gzip_decompressor());
    if (bzip2) new_stream->push(bzip2_decompressor());
    if (lzma) new_stream->push(lzma_decompressor());

    if (file == "-") {
        new_stream->push(std::cin);
    }
    else {
        file_source source(file.c_str(), mode);
        if (!source.is_open())
            throw Exception("stream open failed for file %s: %s",
                            file_.c_str(), strerror(errno));
        new_stream->push(source);
    }

    stream.reset(new_stream.release());
    rdbuf(stream->rdbuf());
}

void
filter_istream::
close()
{
    rdbuf(0);
    //stream->close();
}

} // namespace ML
