/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
/**
  \file    FileBackedDataset.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once
#ifndef TUVOK_FILE_BACKED_DATASET_H
#define TUVOK_FILE_BACKED_DATASET_H

#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace tuvok {
class Dataset;

/// Interface for Datasets which are actually backed by files
class FileBackedDataset {
public:
  virtual std::string Filename() const=0;
  /// A user-visible name for your format.  This might get displayed in UI
  /// elements; e.g. the GUI might ask if the user wants to use the "Name()
  /// reader" to open a particular file.
  virtual const char* Name() const { return "Generic"; }

  virtual bool CanRead(const std::string&, const std::vector<int8_t>&) const=0;
  /// Use to verify checksum.  Default implementation says the checksum is
  /// always valid.
  virtual bool Verify(const std::string&) const;
  virtual Dataset* Create(const std::string&, uint64_t, bool) const=0;

  /// @return a list of file extensions readable by this format
  virtual std::list<std::string> Extensions() const=0;
};

} // namespace tuvok

#endif //  TUVOK_FILE_BACKED_DATASET_H
