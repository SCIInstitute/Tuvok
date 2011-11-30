/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include "LargeFile.h"

LargeFile::LargeFile(const std::string fn,
                     std::ios_base::openmode,
                     boost::uint64_t hsz) :
  m_filename(fn),
  header_size(hsz),
  byte_offset(0)
{
}

std::tr1::shared_ptr<const void> LargeFile::read(size_t length)
{
  // The other 'read' will take account of the header_size.  It has to be this
  // way because the other read is public, and that's the only semantics that
  // make sense.
  return this->read(this->byte_offset - this->header_size, length);
}

void LargeFile::seek(boost::uint64_t to)
{
  byte_offset = to + this->header_size;
}

boost::uint64_t LargeFile::offset() const { return this->byte_offset; }
