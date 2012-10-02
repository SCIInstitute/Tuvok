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
  \file    DSFactory.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Instantiates the correct kind of dataset from a file
*/
#pragma once
#ifndef TUVOK_DS_FACTORY_H
#define TUVOK_DS_FACTORY_H

#include "StdTuvokDefines.h"

#include <list>
#include <memory>
#include <stdexcept>
#include "TuvokIOError.h"

// Get rid of the warning about "non-empty throw specification".
#ifdef DETECTED_OS_WINDOWS
  #pragma warning(disable:4290)
#endif

namespace tuvok {

class Dataset;

namespace io {

class DSFactory {
public:
  /// Instantiates a new dataset.
  /// @param the filename for the dataset to be opened
  /// @param maximum brick size allowed by the caller
  /// @param whether the dataset should do expensive work to verify that the
  ///        file is valid/correct.
  Dataset* Create(const std::string&, uint64_t, bool) const throw(DSOpenFailed);

  /// Identify the reader which can read the given file.
  /// @return The dataset if we find a match, or NULL if not.
  const std::weak_ptr<Dataset> Reader(const std::string&) const;

  void AddReader(std::shared_ptr<Dataset>);

  // We can't copy datasets.  So we store pointers to them instead.
  typedef std::list<std::shared_ptr<Dataset>> DSList;
  const DSList& Readers() const;

private:
  DSList datasets;
};

} // io
} // tuvok

#endif // TUVOK_DS_FACTORY_H
