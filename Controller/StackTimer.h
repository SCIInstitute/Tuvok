#ifndef TUVOK_STACK_TIMER_H
#define TUVOK_STACK_TIMER_H

#include "Basics/PerfCounter.h"
#include "Basics/Timer.h"
#include "Controller.h"

namespace tuvok {

/// Simple mechanism for timing blocks of code.  Create a StackTimer on
/// the stack and it will record timing information when it goes out of
/// scope.  For example:
///
///   if(doLongComplicatedTask) {
///     StackTimer task_identifier(PERF_DISK_READ);
///     this->Function();
///   }
struct StackTimer {
  StackTimer(enum PerfCounter pc) : counter(pc) {
    timer.Start();
  }
  ~StackTimer() {
    Controller::Instance().IncrementPerfCounter(counter, timer.Elapsed());
  }
  enum PerfCounter counter;
  Timer timer;
};

}

/// For short blocks, it may be easier to use this macro:
///
///   TimedStatement(PERF_DISK_READ, this->Function());
#define TimedStatement(pc, block)         \
  do {                                    \
    tuvok::StackTimer _generic_timer(pc); \
    block;                                \
  } while(0)

#endif
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 IVDA Group

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
