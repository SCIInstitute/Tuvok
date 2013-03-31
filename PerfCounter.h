#ifndef TUVOK_PERF_COUNTER_H
#define TUVOK_PERF_COUNTER_H

/// Valid performance counters the system should track.
/// When adding a new counter, please add a (units) clause so we know how to
/// interpret the value!
enum PerfCounter {
  PERF_DISK_READ=0,   // reading bricks from disk (seconds)
  PERF_DECOMPRESSION, // decompressing brick data (seconds)
  PERF_COMPRESSION,   // compressing brick data (seconds)
  PERF_BRICKS,        // number of bricks read/processed (counter)
  PERF_BRICK_COPY,    // copying data into rebricked bricks (seconds)
  PERF_MM_PRECOMPUTE, // computing min/max for new bricks (seconds)
  PERF_CACHE_LOOKUP,  // looking up/copying from cache (seconds)
  PERF_CACHE_ADD,     // adding/copying into the brick cache (seconds)
  PERF_DY_GETBRICK,   // overall operation of GetBrick call (seconds)
  PERF_SOMETHING,     // ad hoc, always changing (seconds)
  PERF_END_IO, // invalid; end of IO-based metrics

  PERF_READ_HTABLE=1000, // reading hash table from GPU (seconds)
  PERF_CONDENSE_HTABLE,  // condensing hash table [removing empties] (seconds)
  PERF_RENDER,           // (seconds)
  PERF_UPLOAD_BRICKS,    // uploading bricks to GPU [tex updates] (seconds)
  PERF_RAYCAST,       // raycasting part of rendering (seconds)
  PERF_END_RENDER, // invalid; end of render-based metrics
  PERF_END // invalid; used for sizing table.
};

/// interface for classes which can be queried in this way.
struct PerfQueryable {
  virtual double PerfQuery(enum PerfCounter) = 0;
};

#endif
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 Interactive Visualization and Data Analysis Group

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
