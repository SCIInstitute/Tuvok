#include "ProgressTimer.h"

#include <sstream>
#include <iomanip>

ProgressTimer::ProgressTimer() : Timer() {}

static std::string TimeToString(double dMiliSecs) {
  const uint64_t miliSecs = uint64_t(dMiliSecs);
  const uint64_t secs   = (miliSecs/(1000))%60;
  const uint64_t mins   = (miliSecs/(1000*60))%60;
  const uint64_t hours  = (miliSecs/(1000*60*60))%24;
  const uint64_t days   = (miliSecs/(1000*60*60*24))%7;
  const uint64_t weeks  = (miliSecs/(1000*60*60*24*7));

  std::stringstream result;

  if (weeks > 0) {
    if (weeks > 1)
      result << weeks << " Weeks ";
    else
      result << weeks << " Week ";
  }

  if (days > 0) {
    if (days > 1)
      result << days << " Days ";
    else
      result << days << " Day ";
  }

  if (hours > 0) {
    result << hours << ":";
  }

  result << std::setw(2) << std::setfill('0') << mins 
         << ":" << std::setw(2) << std::setfill('0') << secs;

  return result.str();
}

std::string ProgressTimer::GetProgressMessage(double progress,
                                              bool bIncludeElapsed,
                                              bool bIncludeRemaining) const {

  const double miliSecs = Elapsed();
  std::string result;

  if (bIncludeElapsed) {
    if (!result.empty()) result += " "; 
    result += "Elapsed: " + TimeToString(miliSecs);
  }
  if (bIncludeRemaining && progress > 0.0) {
    if (!result.empty()) result += " "; 
    result += "Remaining: " + TimeToString(miliSecs * (1.0-progress)/progress);
  }

  return result;
}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group

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
