/*
 For more information, please see: http://software.sci.utah.edu

 The MIT License

 Copyright (c) 2012 Scientific Computing and Imaging Institute,
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

// This header file is automatically generated.
// Contains definitions for executing bound Lua functions inside of a class.
 
#ifndef TUVOK_LUAEXECHEADER_H_
#define TUVOK_LUAEXECHEADER_H_


#define TUVOK_LUA_CEXEC_FUNCTIONS \
  template <typename P1> \
  void cexec(const std::string& cmd, P1 p1);\
  template <typename P1, typename P2> \
  void cexec(const std::string& cmd, P1 p1, P2 p2);\
  template <typename P1, typename P2, typename P3> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3);\
  template <typename P1, typename P2, typename P3, typename P4> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> \
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10);
  
#define TUVOK_LUA_CEXEC_RET_FUNCTIONS \
  template <typename T, typename P1> \
  T cexecRet(const std::string& cmd, P1 p1);\
  template <typename T, typename P1, typename P2> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2);\
  template <typename T, typename P1, typename P2, typename P3> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3);\
  template <typename T, typename P1, typename P2, typename P3, typename P4> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9);\
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> \
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10);
  
#define TUVOK_LUA_SETDEFAULTS_FUNCTIONS \
  template <typename P1> \
  void setDefaults(const std::string& cmd, P1 p1, bool call);\
  template <typename P1, typename P2> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, bool call);\
  template <typename P1, typename P2, typename P3> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, bool call);\
  template <typename P1, typename P2, typename P3, typename P4> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, bool call);\
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10> \
  void setDefaults(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10, bool call);
  
#endif

