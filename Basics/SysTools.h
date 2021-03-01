/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
  \file    SysTools.h
  \brief   Simple routines for filename handling
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    Dec 2008
*/

#pragma once

#ifndef SYSTOOLS_H
#define SYSTOOLS_H

#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>

  #ifdef max
    #undef max
  #endif

  #ifdef min
    #undef min
  #endif

  #define LARGE_STAT_BUFFER struct __stat64
#else
  #include <wchar.h>
  #include <sys/stat.h>
  typedef wchar_t WCHAR;
  typedef unsigned char CHAR;
  #define LARGE_STAT_BUFFER struct stat
#endif

#include "StdDefines.h"

class AbstrDebugOut;

namespace SysTools {
  template <typename T> std::string ToString(const T& aValue)
  {
     std::stringstream ss;
     ss << aValue;
     return ss.str();
  }

  template <typename T> std::wstring ToWString(const T& aValue)
  {
     std::wstringstream ss;
     ss << aValue;
     return ss.str();
  }

  template <typename T> bool FromString(T& t, const std::string& s,
                                std::ios_base& (*f)(std::ios_base&) = std::dec)
  {
      std::istringstream iss(s);
      return !(iss >> f >> t).fail();
  }


  template <typename T> bool FromString(T& t, const std::wstring& s,
                                std::ios_base& (*f)(std::ios_base&) = std::dec)
  {
      std::wistringstream iss(s);
      return !(iss >> f >> t).fail();
  }

  template <typename T> T FromString(const std::string& s,
                             std::ios_base& (*f)(std::ios_base&) = std::dec)
  {
      T t;
      std::istringstream iss(s);
      iss >> f >> t;
      return t;
  }

  template <typename T> T FromString(const std::wstring& s,
                             std::ios_base& (*f)(std::ios_base&) = std::dec)
  {
      T t;
      std::wistringstream iss(s);
      iss >> f >> t;
      return t;
  }

  std::string toNarrow(const wchar_t* str);
  std::wstring toWide(const char* str);
  std::string toNarrow(const std::wstring& str);
  std::wstring toWide(const std::string& str);

  std::wstring ToLowerCase(const std::wstring& str);
  std::string ToLowerCase(const std::string& str);
  std::wstring ToUpperCase(const std::wstring& str);
  std::string ToUpperCase(const std::string& str);

  enum EProtectMode {
    PM_NONE = 0,
    PM_QUOTES,
    PM_BRACKETS,
    PM_CUSTOM_DELIMITER
  };
  std::vector<std::string> Tokenize(const std::string& strInput,
                                    EProtectMode mode = PM_QUOTES,
                                    char customOrOpeningDelimiter='{',
                                    char closingDelimiter='}');
  std::vector<std::wstring> Tokenize(const std::wstring& strInput,
                                     EProtectMode mode = PM_QUOTES,
                                     wchar_t customOrOpeningDelimiter=L'{',
                                     wchar_t closingDelimiter=L'{');

  std::string GetFromResourceOnMac(const std::string& fileName);
  std::wstring GetFromResourceOnMac(const std::wstring& fileName);

  bool FileExists(const std::string& fileName);
  bool FileExists(const std::wstring& fileName);

  bool RemoveFile(const std::string& fileName);
  bool RemoveFile(const std::wstring& fileName);

  bool RenameFile(const std::string& source, const std::string& target);
  bool RenameFile(const std::wstring& wsource, const std::wstring& wtarget);

  std::string GetExt(const std::string& fileName);
  std::wstring GetExt(const std::wstring& fileName);

  std::string GetPath(const std::string& fileName);
  std::wstring GetPath(const std::wstring& fileName);

  std::string GetFilename(const std::string& fileName);
  std::wstring GetFilename(const std::wstring& fileName);

  std::string basename(const std::string& f);
  std::string dirname(const std::string& f);

  std::string CanonicalizePath(const std::string& path);

  std::string FindPath(const std::string& fileName, const std::string& path);
  std::wstring FindPath(const std::wstring& fileName,
                        const std::wstring& path);

  std::string  RemoveExt(const std::string& fileName);
  std::wstring RemoveExt(const std::wstring& fileName);

  std::string  CheckExt(const std::string& fileName,
                        const std::string& newext);
  std::wstring CheckExt(const std::wstring& fileName,
                        const std::wstring& newext);

  std::string  ChangeExt(const std::string& fileName,
                         const std::string& newext);
  std::wstring ChangeExt(const std::wstring& fileName,
                         const std::wstring& newext);

  std::string  AppendFilename(const std::string& fileName,
                              const int iTag);
  std::wstring AppendFilename(const std::wstring& fileName,
                              const int iTag);
  std::string  AppendFilename(const std::string& fileName,
                              const std::string& tag);
  std::wstring AppendFilename(const std::wstring& fileName,
                              const std::wstring& tag);

  std::string  FindNextSequenceName(const std::string& strFilename);
  std::wstring FindNextSequenceName(const std::wstring& wStrFilename);

  std::string  FindNextSequenceName(const std::string& fileName,
                                    const std::string& ext,
                                    const std::string& dir="");
  std::wstring FindNextSequenceName(const std::wstring& fileName,
                                    const std::wstring& ext,
                                    const std::wstring& dir=L"");

  uint32_t FindNextSequenceIndex(const std::string& fileName,
                                     const std::string& ext,
                                     const std::string& dir="");
  uint32_t FindNextSequenceIndex(const std::wstring& fileName,
                                     const std::wstring& ext,
                                     const std::wstring& dir=L"");

  bool GetHomeDirectory(std::string& path);
  bool GetHomeDirectory(std::wstring& path);
  bool GetTempDirectory(std::string& path);
  bool GetTempDirectory(std::wstring& path);

#ifdef _WIN32
  std::vector<std::wstring> GetDirContents(const std::wstring& dir,
                                           const std::wstring& fileName=L"*",
                                           const std::wstring& ext=L"*");
  std::vector<std::string> GetDirContents(const std::string& dir,
                                          const std::string& fileName="*",
                                          const std::string& ext="*");
#else
  std::vector<std::wstring> GetDirContents(const std::wstring& dir,
                                           const std::wstring& fileName=L"*",
                                           const std::wstring& ext=L"");
  std::vector<std::string> GetDirContents(const std::string& dir,
                                          const std::string& fileName="*",
                                          const std::string& ext="");
#endif

  std::vector<std::wstring> GetSubDirList(const std::wstring& dir);
  std::vector<std::string> GetSubDirList(const std::string& dir);

  bool GetFileStats(const std::string& strFileName, LARGE_STAT_BUFFER& stat_buf);
  bool GetFileStats(const std::wstring& wstrFileName, LARGE_STAT_BUFFER& stat_buf);

  std::wstring TrimStrLeft(const std::wstring &str,
                           const std::wstring& c = L" \r\n\t");
  std::string TrimStrLeft(const std::string &str,
                          const std::string& c = " \r\n\t");
  std::wstring TrimStrRight(const std::wstring &str,
                            const std::wstring& c = L" \r\n\t");
  std::string TrimStrRight(const std::string &str,
                           const std::string& c = " \r\n\t");
  std::wstring TrimStr(const std::wstring &str,
                       const std::wstring& c = L" \r\n\t");
  std::string TrimStr(const std::string &str,
                      const std::string& c = " \r\n\t");

  void ReplaceAll(std::string& input, const std::string& search,
                  const std::string& replace);
  void ReplaceAll(std::wstring& input, const std::wstring& search, 
                  const std::wstring& replace);

#ifdef _WIN32
  bool GetFilenameDialog(const std::string& lpstrTitle,
                         const CHAR* lpstrFilter,
                         std::string &filename, const bool save,
                         HWND owner=NULL, DWORD* nFilterIndex=NULL);
  bool GetFilenameDialog(const std::wstring& lpstrTitle,
                         const WCHAR* lpstrFilter,
                         std::wstring &filename, const bool save,
                         HWND owner=NULL, DWORD* nFilterIndex=NULL);
#endif

  class CmdLineParams {
    public:
      #ifdef _WIN32
        CmdLineParams();
      #endif
      CmdLineParams(int argc, char** argv);

      bool SwitchSet(const std::string& parameter);
      bool SwitchSet(const std::wstring& parameter);

      bool GetValue(const std::string& parameter, double& value);
      bool GetValue(const std::wstring& parameter, double& value);
      bool GetValue(const std::string& parameter, float& value);
      bool GetValue(const std::wstring& parameter, float& value);
      bool GetValue(const std::string& parameter, int& value);
      bool GetValue(const std::wstring& parameter, int& value);
      bool GetValue(const std::string& parameter, unsigned int& value);
      bool GetValue(const std::wstring& parameter, unsigned int& value);
      bool GetValue(const std::string& parameter, std::string& value);
      bool GetValue(const std::wstring& parameter, std::wstring& value);

    protected:
      std::vector<std::string> m_strArrayParameters;
      std::vector<std::string> m_strArrayValues;

      std::string m_strFilename;
  };
}

#endif // SYSTOOLS_H
