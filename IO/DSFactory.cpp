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
  \file    DSFactory.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Instantiates the correct kind of dataset from a file
*/
#include "StdTuvokDefines.h"

#include <fstream>

#include "DSFactory.h"
#include "Controller/Controller.h"
#include "Dataset.h"
#include "FileBackedDataset.h"

namespace tuvok {
namespace io {

static void first_block(const std::string& filename,
                        std::vector<int8_t>& block)
{
  std::ifstream ifs(filename.c_str(), std::ifstream::in |
                                      std::ifstream::binary);
  block.resize(512, 0);
  if(!ifs.is_open()) {
    return;
  }
  ifs.read(reinterpret_cast<char*>(&block[0]), 512);
  ifs.close();
}

Dataset* DSFactory::Create(const std::string& filename,
                           uint64_t max_brick_size,
                           bool verify) const
{
  std::vector<int8_t> bytes(512);
  first_block(filename, bytes);

  const std::weak_ptr<FileBackedDataset> ds = this->Reader(filename);
  if(!ds.expired()) {
    return ds.lock()->Create(filename, max_brick_size, verify);
  }
  throw DSOpenFailed(filename.c_str(), "No reader can read this data!",
                     __FILE__, __LINE__);
}

const std::weak_ptr<FileBackedDataset>
DSFactory::Reader(const std::string& filename) const
{
  std::vector<int8_t> bytes(512);
  first_block(filename, bytes);

  for(auto ds = datasets.cbegin(); ds != datasets.cend(); ++ds)
  {
    /// downcast to FileBackedDataset for now.  We could move CanRead
    /// up into Dataset, but there's currently no need and that doesn't
    /// make much sense.
    const std::shared_ptr<FileBackedDataset> fds =
      std::dynamic_pointer_cast<FileBackedDataset>(*ds);
    if(fds->CanRead(filename, bytes)) {
      return *ds;
    }
  }
  return std::weak_ptr<FileBackedDataset>();
}

void DSFactory::AddReader(std::shared_ptr<FileBackedDataset> ds)
{
  this->datasets.push_front(ds);
}

const DSFactory::DSList&
DSFactory::Readers() const { return this->datasets; }

} // io
} // tuvok
