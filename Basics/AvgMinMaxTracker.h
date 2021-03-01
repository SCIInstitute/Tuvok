#pragma once

#ifndef TUVOK_AVGMINMAXTRACKER_H
#define TUVOK_AVGMINMAXTRACKER_H

#include <deque>
#include <vector>
#include <iostream>

namespace tuvok
{
  template<class T>
  struct AvgMinMaxStruct {
    AvgMinMaxStruct(T const& average, T const& minimum, T const& maximum) : avg(average), min(minimum), max(maximum) {}
    T avg, min, max;
    friend std::ostream& operator<<(std::ostream &os, AvgMinMaxStruct const& amm) { return os << amm.avg << " [" << amm.min << ", " << amm.max << "]"; }
  };

  template<class T>
  class AvgMinMaxTracker {
  public:
    AvgMinMaxTracker(uint32_t historyLength)
      : m_History()
      , m_Avg()
      , m_Min()
      , m_Max()
      , m_MaxHistoryLength(historyLength)
    {}

    void SetMaxHistoryLength(uint32_t maxHistoryLength) { m_MaxHistoryLength = maxHistoryLength; }
    uint32_t GetMaxHistoryLength() const { return m_MaxHistoryLength; }
    uint32_t GetHistroryLength() const { return (uint32_t)m_History.size(); }

    AvgMinMaxStruct<T> GetAvgMinMax() const { return AvgMinMaxStruct<T>(GetAvg(), GetMin(), GetMax()); }
    T const GetAvg() const { if (m_History.empty()) return 0; else return m_Avg / GetHistroryLength(); }
    T const& GetMin() const { return m_Min; }
    T const& GetMax() const { return m_Max; }

    void Push(T const& value)
    {
      m_History.push_back(value);
      m_Avg += value;
      while (m_MaxHistoryLength < m_History.size()) {
        m_Avg -= m_History.front();
        m_History.pop_front();
      }
      if (!m_History.empty()) {
        auto i = m_History.begin();
        m_Min = *i; // min in avg interval
        m_Max = *i; // max in avg interval
        for (; i != m_History.end(); ++i) {
          m_Min = std::min(m_Min, *i);
          m_Max = std::max(m_Max, *i);
        }
      }
    }

    std::vector<T> GetHistory() const { return std::vector<T>(m_History.begin(), m_History.end()); }

  private:
    std::deque<T> m_History;
    T m_Avg;
    T m_Min;
    T m_Max;
    uint32_t m_MaxHistoryLength;
  };

} // namespace tuvok

#endif // TUVOK_AVGMINMAXTRACKER_H

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
