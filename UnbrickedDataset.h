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
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef TUVOK_UNBRICKED_DATASET_H
#define TUVOK_UNBRICKED_DATASET_H

#include "StdTuvokDefines.h"
#ifdef DETECTED_OS_WINDOWS
# include <memory>
#else
# include <tr1/memory>
#endif

#include "ExternalDataset.h"
#include "../Controller/Controller.h"

namespace tuvok {

/** A dataset which consists of a single brick and a single LOD. */
class UnbrickedDataset : public ExternalDataset {
public:
  UnbrickedDataset();
  virtual ~UnbrickedDataset();

  virtual UINT64VECTOR3 GetBrickSize(const BrickKey& = BrickKey(0,0)) const;
  virtual bool GetBrick(const BrickKey&, std::vector<unsigned char>&) const;

  void SetData(const std::tr1::shared_ptr<float>, size_t len);
  void SetData(const std::tr1::shared_ptr<unsigned char>, size_t len);
};

}; // namespace tuvok

#endif // TUVOK_UNBRICKED_DATASET_H
