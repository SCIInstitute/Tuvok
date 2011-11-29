#pragma once

#ifndef LARGERAWFILE_H
#define LARGERAWFILE_H

#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1

#include <string>
#include <vector>
#include "EndianConvert.h"

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <io.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <share.h>
  // undef stupid windows defines to max and min
  #ifdef max
  #undef max
  #endif

  #ifdef min
  #undef min
  #endif
#endif

#ifdef _WIN32
  typedef HANDLE FILETYPE;
#else
  typedef FILE* FILETYPE;
#endif

class LargeRAWFile {
public:
  LargeRAWFile(const std::string& strFilename, UINT64 iHeaderSize=0);
  LargeRAWFile(const std::wstring& wstrFilename, UINT64 iHeaderSize=0);
  LargeRAWFile(const LargeRAWFile &other);
  virtual ~LargeRAWFile() {Close();}

  virtual bool Open(bool bReadWrite=false);
  virtual bool IsOpen() const { return m_bIsOpen;}
  virtual bool IsWritable() const { return m_bWritable;}
  virtual bool Create(UINT64 iInitialSize=0);
  virtual bool Append();
  virtual void Close();
  virtual void Delete();
  virtual bool Truncate();
  virtual bool Truncate(UINT64 iPos);
  virtual UINT64 GetCurrentSize();
  std::string GetFilename() const { return m_strFilename;}

  virtual void SeekStart();
  virtual UINT64 SeekEnd();
  virtual UINT64 GetPos();
  virtual void SeekPos(UINT64 iPos);
  virtual size_t ReadRAW(unsigned char* pData, UINT64 iCount);
  virtual size_t WriteRAW(const unsigned char* pData, UINT64 iCount);
  virtual bool CopyRAW(UINT64 iCount, UINT64 iSourcePos, UINT64 iTargetPos,
                       unsigned char* pBuffer, UINT64 iBufferSize);

  template<class T> void Read(const T* pData, UINT64 iCount, UINT64 iPos,
                              UINT64 iOffset) {
    SeekPos(iOffset+sizeof(T)*iPos);
    ReadRAW((unsigned char*)pData, sizeof(T)*iCount);
  }

  template<class T> void Write(const T* pData, UINT64 iCount, UINT64 iPos,
                               UINT64 iOffset) {
    SeekPos(iOffset+sizeof(T)*iPos);
    WriteRAW((unsigned char*)pData, sizeof(T)*iCount);
  }

  template<class T> void ReadData(T& value, bool bIsBigEndian) {
    ReadRAW((unsigned char*)&value, sizeof(T));
    if (EndianConvert::IsBigEndian() != bIsBigEndian)
      EndianConvert::Swap<T>(value);
  }

  template<class T> void WriteData(const T& value, bool bIsBigEndian) {
    if (EndianConvert::IsBigEndian() != bIsBigEndian)
      EndianConvert::Swap<T>(value);
    WriteRAW((unsigned char*)&value, sizeof(T));
    if (EndianConvert::IsBigEndian() != bIsBigEndian)
      EndianConvert::Swap<T>(value);
  }

  template<class T> void ReadData(std::vector<T> &value, UINT64 count,
                                  bool bIsBigEndian) {
    if (count == 0) return;
    value.resize(size_t(count));
    ReadRAW( (unsigned char*)&value[0], sizeof(T)*size_t(count));
    if (EndianConvert::IsBigEndian() != bIsBigEndian) {
      for (size_t i = 0; i < count; i++) {
        EndianConvert::Swap<T>(value[i]);
      }
    }
  }

  template<class T> void WriteData(const std::vector<T> &value,
                                   bool bIsBigEndian) {
    UINT64 count = value.size();

    if (count == 0) return;
    if (EndianConvert::IsBigEndian() != bIsBigEndian) {
      for (size_t i = 0; i < count; i++) {
        EndianConvert::Swap<T>(value[i]);
      }
    }
    WriteRAW((unsigned char*)&value[0], sizeof(T)*size_t(count));
    if (EndianConvert::IsBigEndian() != bIsBigEndian) {
      for (size_t i = 0; i < count; i++) {
        EndianConvert::Swap<T>(value[i]);
      }
    }
  }

  virtual void ReadData(std::string &value, UINT64 count) {
    if (count == 0) return;
    value.resize(size_t(count));
    ReadRAW((unsigned char*)&value[0], sizeof(char)*size_t(count));
  }

  virtual void WriteData(const std::string &value) {
    if (value.empty()) return;
    WriteRAW((unsigned char*)&value[0], sizeof(char)*size_t(value.length()));
  }

  enum IOHint {
    NORMAL,     ///< reset back to default state
    SEQUENTIAL, ///< going to access this sequentially
    NOREUSE,    ///< will use this once and then it's useless.
    WILLNEED,   ///< don't need this now, but will soon
    DONTNEED    ///< no longer need this region
  };
  // Hint to the underlying driver how we'll access data
  virtual void Hint(IOHint hint, UINT64 offset, UINT64 length) const;

  static bool Copy(const std::string& strSource, const std::string& strTarget,
                   UINT64 iSourceHeaderSkip=0, std::string* strMessage=NULL);
  static bool Copy(const std::wstring& wstrSource,
                   const std::wstring& wstrTarget, UINT64 iSourceHeaderSkip=0,
                   std::wstring* wstrMessage=NULL);
  static bool Compare(const std::string& strFirstFile,
                      const std::string& strSecondFile,
                      std::string* strMessage=NULL);
  static bool Compare(const std::wstring& wstrFirstFile,
                      const std::wstring& wstrSecondFile,
                      std::wstring* wstrMessage=NULL);
protected:
  FILETYPE      m_StreamFile;
  std::string   m_strFilename;
  bool          m_bIsOpen;
  bool          m_bWritable;
  UINT64        m_iHeaderSize;
};

#endif // LARGERAWFILE_H
