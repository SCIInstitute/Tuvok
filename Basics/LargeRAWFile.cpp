#include <memory>
#include <sstream>
#include <algorithm> // for std::max, std::min
#include "LargeRAWFile.h"
#include "nonstd.h"

using namespace std;

LargeRAWFile::LargeRAWFile(const std::string& strFilename, uint64_t iHeaderSize):
  m_StreamFile(NULL),
  m_strFilename(strFilename),
  m_bIsOpen(false),
  m_bWritable(false),
  m_iHeaderSize(iHeaderSize)
{
}

LargeRAWFile::LargeRAWFile(const std::wstring& wstrFilename, uint64_t iHeaderSize):
  m_bIsOpen(false),
  m_bWritable(false),
  m_iHeaderSize(iHeaderSize)
{
  string strFilename(wstrFilename.begin(), wstrFilename.end());
  m_strFilename = strFilename;
}

LargeRAWFile::LargeRAWFile(const LargeRAWFile &other) :
  m_StreamFile(NULL),
  m_strFilename(other.m_strFilename),
  m_bIsOpen(other.m_bIsOpen),
  m_bWritable(other.m_bWritable),
  m_iHeaderSize(other.m_iHeaderSize)
{
  /// @todo !?!? need a better fix, a copy constructor shouldn't be expensive.
  assert(!m_bWritable && "Copying a file in write mode is too expensive.");
  if(m_bIsOpen) {
    Open(false);
  }
}

bool LargeRAWFile::Open(bool bReadWrite) {
  #ifdef _WIN32
  m_StreamFile = CreateFileA(m_strFilename.c_str(),
                             (bReadWrite) ? GENERIC_READ | GENERIC_WRITE
                                          : GENERIC_READ,
                             FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    m_bIsOpen = m_StreamFile != INVALID_HANDLE_VALUE;
  #else
    m_StreamFile = fopen(m_strFilename.c_str(), (bReadWrite) ? "r+b" : "rb");
    if(m_StreamFile == NULL) {
      std::ostringstream err_file;
      err_file << "fopen '" << m_strFilename << "'";
      perror(err_file.str().c_str());
    }
    m_bIsOpen = m_StreamFile != NULL;
  #endif

  if (m_bIsOpen && m_iHeaderSize != 0) SeekStart();

  m_bWritable = (m_bIsOpen) ? bReadWrite : false;

  return m_bIsOpen;
}

bool LargeRAWFile::Create(uint64_t iInitialSize) {
#ifdef _WIN32
  m_StreamFile = CreateFileA(m_strFilename.c_str(),
                             GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, 0, NULL);
  m_bIsOpen = m_StreamFile != INVALID_HANDLE_VALUE;
#else
  m_StreamFile = fopen(m_strFilename.c_str(), "w+b");
  m_bIsOpen = m_StreamFile != NULL;
#endif

  if (m_bIsOpen && iInitialSize>0) {
    SeekPos(iInitialSize-1);
    WriteData<unsigned char>(0,false);
    SeekStart();
  }

  m_bWritable = m_bIsOpen;

  return m_bIsOpen;
}

bool LargeRAWFile::Append() {
#ifdef _WIN32
  m_StreamFile = CreateFileA(m_strFilename.c_str(),
                             GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             OPEN_ALWAYS, 0, NULL);
  m_bIsOpen = m_StreamFile != INVALID_HANDLE_VALUE;
#else
  m_StreamFile = fopen(m_strFilename.c_str(), "a+b");
  m_bIsOpen = m_StreamFile != NULL;
#endif

  if (m_bIsOpen) SeekEnd();

  m_bWritable = m_bIsOpen;

  return m_bIsOpen;
}


void LargeRAWFile::Close() {
  if (m_bIsOpen) {
#ifdef _WIN32
    CloseHandle(m_StreamFile);
#else
    fclose(m_StreamFile);
#endif
    m_bIsOpen =false;
  }
}

uint64_t LargeRAWFile::GetCurrentSize() {
  uint64_t iPrevPos = GetPos();

  SeekStart();
  uint64_t ulStart = GetPos();
  uint64_t ulEnd   = SeekEnd();
  uint64_t ulFileLength = ulEnd - ulStart;

  SeekPos(iPrevPos);

  return ulFileLength;
}

void LargeRAWFile::SeekStart() {
  SeekPos(0);
}

uint64_t LargeRAWFile::SeekEnd() {
  #ifdef _WIN32
    LARGE_INTEGER liTarget, liRealTarget; liTarget.QuadPart = 0;
    SetFilePointerEx(m_StreamFile, liTarget, &liRealTarget, FILE_END);
    return uint64_t(liRealTarget.QuadPart)-m_iHeaderSize;
  #else
    // get current position=file size!
    if(fseeko(m_StreamFile, 0, SEEK_END)==0)
      return ftello(m_StreamFile)-m_iHeaderSize;
    else
      return 0;
  #endif
}

uint64_t LargeRAWFile::GetPos() {
  #ifdef _WIN32
    LARGE_INTEGER liTarget, liRealTarget; liTarget.QuadPart = 0;
    SetFilePointerEx(m_StreamFile, liTarget, &liRealTarget, FILE_CURRENT);
    return uint64_t(liRealTarget.QuadPart)-m_iHeaderSize;
  #else
    //get current position=file size!
    return ftello(m_StreamFile)-m_iHeaderSize;
  #endif
}

void LargeRAWFile::SeekPos(uint64_t iPos) {
  #ifdef _WIN32
    LARGE_INTEGER liTarget; liTarget.QuadPart = LONGLONG(iPos+m_iHeaderSize);
    SetFilePointerEx(m_StreamFile, liTarget, NULL, FILE_BEGIN);
  #else
    fseeko(m_StreamFile, off_t(iPos+m_iHeaderSize), SEEK_SET);
  #endif
}

size_t LargeRAWFile::ReadRAW(unsigned char* pData, uint64_t iCount) {
  #ifdef _WIN32
  uint64_t iTotalRead = 0;
  DWORD dwReadBytes;
  while (iCount > std::numeric_limits<DWORD>::max())
  {
    if (!ReadFile(m_StreamFile, pData, std::numeric_limits<DWORD>::max(), &dwReadBytes, NULL)) {
      return iTotalRead;
    }
    if (dwReadBytes == 0) {
      return iTotalRead;
    }
    iCount -= dwReadBytes;
    iTotalRead += dwReadBytes;
    pData += dwReadBytes;
  }
  if (!ReadFile(m_StreamFile, pData, DWORD(iCount), &dwReadBytes, NULL)) {
    return iTotalRead;
  }
  return iTotalRead + dwReadBytes;
  #else
    return fread(pData,1,iCount,m_StreamFile);
  #endif
}

size_t LargeRAWFile::WriteRAW(const unsigned char* pData, uint64_t iCount) {
  #ifdef _WIN32
  uint64_t iTotalWritten = 0;
  DWORD dwWrittenBytes;
  while (iCount > std::numeric_limits<DWORD>::max())
  {
    if (!WriteFile(m_StreamFile, pData, std::numeric_limits<DWORD>::max(), &dwWrittenBytes, NULL)) {
      return iTotalWritten;
    }
    if (dwWrittenBytes == 0) {
      return iTotalWritten;
    }
    iCount -= dwWrittenBytes;
    iTotalWritten += dwWrittenBytes;
    pData += dwWrittenBytes;
  }
  if (!WriteFile(m_StreamFile, pData, DWORD(iCount), &dwWrittenBytes, NULL)) {
    return iTotalWritten;
  }
  return iTotalWritten + dwWrittenBytes;
  #else
    return fwrite(pData,1,iCount,m_StreamFile);
  #endif
}

bool LargeRAWFile::CopyRAW(uint64_t iCount, uint64_t iSourcePos, uint64_t iTargetPos, 
                           unsigned char* pBuffer, uint64_t iBufferSize) {

  uint64_t iBytesRead = 0;
  do {
    SeekPos(iSourcePos+iBytesRead);
    uint64_t iBytesJustRead = ReadRAW(pBuffer,min(iBufferSize,iCount-iBytesRead));
    SeekPos(iTargetPos+iBytesRead);
    uint64_t iBytesJustWritten = WriteRAW(pBuffer, iBytesJustRead);
    if (iBytesJustRead != iBytesJustWritten) return false;
    iBytesRead += iBytesJustRead;
  } while(iBytesRead < iCount);

  return true;
}


void LargeRAWFile::Delete() {
  if (m_bIsOpen) Close();
  remove(m_strFilename.c_str());
}

bool LargeRAWFile::Truncate() {
  #ifdef _WIN32
    return 0 != SetEndOfFile(m_StreamFile);
  #else
    uint64_t iPos = GetPos();
    return 0 == ftruncate(fileno(m_StreamFile), off_t(iPos));
  #endif
}

bool LargeRAWFile::Truncate(uint64_t iPos) {
  #ifdef _WIN32
    SeekPos(iPos);
    return 0 != SetEndOfFile(m_StreamFile);
  #else
    return 0 == ftruncate(fileno(m_StreamFile), off_t(iPos+m_iHeaderSize));
  #endif
}

// no-op, but I want to leave the argument names in the header so it's nicer to
// implement it here.
void LargeRAWFile::Hint(IOHint, uint64_t, uint64_t) const { }

bool LargeRAWFile::Copy(const std::string& strSource,
                        const std::string& strTarget, uint64_t iSourceHeaderSkip,
                        std::string* strMessage) {
  std::wstring wstrSource(strSource.begin(), strSource.end());
  std::wstring wstrTarget(strTarget.begin(), strTarget.end());
  if (!strMessage)
    return Copy(wstrSource, wstrTarget, iSourceHeaderSkip, NULL);
  else {
    std::wstring wstrMessage;
    bool bResult = Copy(wstrSource, wstrTarget, iSourceHeaderSkip,
                        &wstrMessage);
    (*strMessage) = string(wstrMessage.begin(), wstrMessage.end());
    return bResult;
  }
}


bool LargeRAWFile::Copy(const std::wstring& wstrSource,
                        const std::wstring& wstrTarget,
                        uint64_t iSourceHeaderSkip, std::wstring* wstrMessage) {
  LargeRAWFile source(wstrSource, iSourceHeaderSkip);
  LargeRAWFile target(wstrTarget);

  source.Open(false);
  if (!source.IsOpen()) {
    if (wstrMessage) (*wstrMessage) = L"Unable to open source file " +
                                      wstrSource;
    return false;
  }

  target.Create();
  if (!target.IsOpen()) {
    if (wstrMessage) (*wstrMessage) = L"Unable to open target file " +
                                      wstrTarget;
    source.Close();
    return false;
  }

  uint64_t iFileSize = source.GetCurrentSize();
  uint64_t iCopySize = min(iFileSize,BLOCK_COPY_SIZE);
  std::shared_ptr<unsigned char> pBuffer(
    new unsigned char[size_t(iCopySize)],
    nonstd::DeleteArray<unsigned char>()
  );

  do {
    iCopySize = source.ReadRAW(pBuffer.get(), iCopySize);
    target.WriteRAW(pBuffer.get(), iCopySize);
  } while (iCopySize>0);

  target.Close();
  source.Close();
  return true;
}


bool LargeRAWFile::Compare(const std::string& strFirstFile,
                           const std::string& strSecondFile,
                           std::string* strMessage) {
  std::wstring wstrFirstFile(strFirstFile.begin(), strFirstFile.end());
  std::wstring wstrSecondFile(strSecondFile.begin(), strSecondFile.end());
  if (!strMessage)
    return Compare(wstrFirstFile, wstrSecondFile, NULL);
  else {
    std::wstring wstrMessage;
    bool bResult = Compare(wstrFirstFile, wstrSecondFile, &wstrMessage);
    (*strMessage) = string(wstrMessage.begin(), wstrMessage.end());
    return bResult;
  }
}

bool LargeRAWFile::Compare(const std::wstring& wstrFirstFile,
                           const std::wstring& wstrSecondFile,
                           std::wstring* wstrMessage) {
  LargeRAWFile first(wstrFirstFile);
  LargeRAWFile second(wstrSecondFile);

  first.Open(false);
  if (!first.IsOpen()) {
    if (wstrMessage) (*wstrMessage) = L"Unable to open input file " +
                                      wstrFirstFile;
    return false;
  }
  second.Open(false);
  if (!second.IsOpen()) {
    if (wstrMessage) (*wstrMessage) = L"Unable to open input file " +
                                      wstrSecondFile;
    first.Close();
    return false;
  }

  if (first.GetCurrentSize() != second.GetCurrentSize()) {
    first.Close();
    second.Close();
    if (wstrMessage) (*wstrMessage) = L"Files differ in size";
    return false;
  }

  uint64_t iFileSize = first.GetCurrentSize();
  uint64_t iCopySize = min(iFileSize,BLOCK_COPY_SIZE/2);
  unsigned char* pBuffer1 = new unsigned char[size_t(iCopySize)];
  unsigned char* pBuffer2 = new unsigned char[size_t(iCopySize)];
  uint64_t iCopiedSize = 0;
  uint64_t iDiffCount = 0;

  if (wstrMessage) (*wstrMessage) = L"";

  do {
    iCopySize = first.ReadRAW(pBuffer1, iCopySize);
    second.ReadRAW(pBuffer2, iCopySize);

    for (uint64_t i = 0;i<iCopySize;i++) {
      if (pBuffer1[i] != pBuffer2[i]) {
        if (wstrMessage) {
          wstringstream ss;
          if (iDiffCount == 0) {
            ss << L"Files differ at address " << i+iCopiedSize;
            iDiffCount = 1;
          } else {
            // don't report more than 10 differences.
            if (++iDiffCount == 10) {
              (*wstrMessage) += L" and more";
              delete [] pBuffer1;
              delete [] pBuffer2;
              first.Close();
              second.Close();
              return false;
            } else {
              ss << L", " << i+iCopiedSize;
            }
          }
          (*wstrMessage) +=ss.str();
        }
      }

    }
    iCopiedSize += iCopySize;
  } while (iCopySize > 0);

  first.Close();
  second.Close();
  delete [] pBuffer1;
  delete [] pBuffer2;
  return iDiffCount == 0;
}
