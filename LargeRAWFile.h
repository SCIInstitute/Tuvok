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
  LargeRAWFile(const std::string& strFilename, uint64_t iHeaderSize=0);
  LargeRAWFile(const std::wstring& wstrFilename, uint64_t iHeaderSize=0);
  LargeRAWFile(const LargeRAWFile &other);
  virtual ~LargeRAWFile() {Close();}

  virtual bool Open(bool bReadWrite=false);
  virtual bool IsOpen() const { return m_bIsOpen;}
  virtual bool IsWritable() const { return m_bWritable;}
  virtual bool Create(uint64_t iInitialSize=0);
  virtual bool Append();
  virtual void Close();
  virtual void Delete();
  virtual bool Truncate();
  virtual bool Truncate(uint64_t iPos);
  virtual uint64_t GetCurrentSize();
  std::string GetFilename() const { return m_strFilename;}

  virtual void SeekStart();
  virtual uint64_t SeekEnd();
  virtual uint64_t GetPos();
  virtual void SeekPos(uint64_t iPos);
  virtual size_t ReadRAW(unsigned char* pData, uint64_t iCount);
  virtual size_t WriteRAW(const unsigned char* pData, uint64_t iCount);
  virtual bool CopyRAW(uint64_t iCount, uint64_t iSourcePos, uint64_t iTargetPos,
                       unsigned char* pBuffer, uint64_t iBufferSize);

  template<class T> void Read(const T* pData, uint64_t iCount, uint64_t iPos,
                              uint64_t iOffset) {
    SeekPos(iOffset+sizeof(T)*iPos);
    ReadRAW((unsigned char*)pData, sizeof(T)*iCount);
  }

  template<class T> void Write(const T* pData, uint64_t iCount, uint64_t iPos,
                               uint64_t iOffset) {
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

  template<class T> void ReadData(std::vector<T> &value, uint64_t count,
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
    uint64_t count = value.size();

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

  virtual void ReadData(std::string &value, uint64_t count) {
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
  virtual void Hint(IOHint hint, uint64_t offset, uint64_t length) const;

  static bool Copy(const std::string& strSource, const std::string& strTarget,
                   uint64_t iSourceHeaderSkip=0, std::string* strMessage=NULL);
  static bool Copy(const std::wstring& wstrSource,
                   const std::wstring& wstrTarget, uint64_t iSourceHeaderSkip=0,
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
  uint64_t      m_iHeaderSize;
};

#include <memory>

typedef std::shared_ptr<LargeRAWFile> LargeRAWFile_ptr;

#endif // LARGERAWFILE_H
