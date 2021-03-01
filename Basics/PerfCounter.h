#ifndef TUVOK_PERF_COUNTER_H
#define TUVOK_PERF_COUNTER_H

/// Valid performance counters the system should track.
/// When adding a new counter, please add a (units) clause so we know how to
/// interpret the value!
enum PerfCounter {

  // structured timers, indention signals timer hierarchy
  PERF_SUBFRAMES=0,              // number of subframes (counter)
  PERF_RENDER,                   // (milliseconds)
    PERF_RAYCAST,                // raycasting part of rendering (milliseconds)
    PERF_READ_HTABLE,            // reading hash table from GPU (milliseconds)
    PERF_CONDENSE_HTABLE,        // condensing hash table [removing empties] (milliseconds)
    PERF_SORT_HTABLE,            // sort bricks by file offset, necessary for layouts
    PERF_UPLOAD_BRICKS,          // uploading bricks to GPU [tex updates] (milliseconds)
      PERF_POOL_SORT,            // sorting the brick pool info (milliseconds)
      PERF_POOL_UPLOADED_MEM,    // overall uploaded mem to GPU (bytes)
      PERF_POOL_GET_BRICK,       // overall operation of GetBrick call from the pool (milliseconds)
        PERF_DY_GET_BRICK,       // overall operation of GetBrick call for dynamic bricked datasets (milliseconds)
          PERF_DY_CACHE_LOOKUPS, // cache look ups (counter)
          PERF_DY_CACHE_LOOKUP,  // looking up/copying from cache (milliseconds)
          PERF_DY_RESERVE_BRICK, // aquire brick memory
          PERF_DY_LOAD_BRICK,    // load (GetBrick) brick from the underlying dataset (milliseconds)
          PERF_DY_CACHE_ADDS,    // cache adds (counter)
          PERF_DY_CACHE_ADD,     // adding/copying into the brick cache (milliseconds)
          PERF_DY_BRICK_COPIED,  // brick copying (counter)
          PERF_DY_BRICK_COPY,    // copying data into rebricked bricks (milliseconds)
      PERF_POOL_UPLOAD_BRICK,    // pool upload of a single brick (milliseconds)
        PERF_POOL_UPLOAD_TEXEL,  // uploading single texel of pool (milliseconds)
      PERF_POOL_UPLOAD_METADATA, // uploading complete pool metadata instead of single texels (milliseconds)

  // low level Extended Octree measures
  // these timers belong under PERF_DY_LOAD_BRICK or PERF_POOL_GET_BRICK if
  // there is no dyn bricked dataset
  PERF_EO_BRICKS,        // number of bricks read/processed (counter)
  PERF_EO_DISK_READ,     // reading bricks from disk (milliseconds)
  PERF_EO_DECOMPRESSION, // decompressing brick data (milliseconds)

  PERF_MM_PRECOMPUTE,    // computing min/max for new bricks (milliseconds)
  PERF_SOMETHING,        // ad hoc, always changing (milliseconds)

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
