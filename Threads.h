#pragma once

#ifndef THREADS_H
#define THREADS_H

#include "StdDefines.h"
#include <memory>
#include <functional>
#include <cassert>
#include <limits>

#ifdef DETECTED_OS_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif

// TODO
// add timeout to CriticalSection Lock() (use CreateMutex/WaitForSingleObject etc. for this on Windows)
// add exceptions (same style as in Sockets)

namespace tuvok
{
  static const uint32_t INFINITE_TIMEOUT = std::numeric_limits<uint32_t>::max();

  class CriticalSection {
    friend class WaitCondition;
  public:
    CriticalSection();
    ~CriticalSection();
  
    void Lock();
    bool TryLock();
    void Unlock();
  
  protected:
#ifdef DETECTED_OS_WINDOWS
    CRITICAL_SECTION m_csIDGuard;
#else
    pthread_mutex_t m_csIDGuard;
#endif

  private:
    // object is non copyable, declare private c'tor and assignment operator to prevent compiler warnings
    CriticalSection(CriticalSection const&);
    CriticalSection& operator=(CriticalSection const&);
  };

  class ScopedLock {
  public:
    ScopedLock(CriticalSection& guard) : m_Guard(guard) { m_Guard.Lock(); }
    ScopedLock(CriticalSection* pGuard) : m_Guard(*pGuard) { assert(pGuard != NULL); m_Guard.Lock(); }
    ScopedLock(std::shared_ptr<CriticalSection>& pGuard) : m_Guard(*pGuard.get()) { assert(pGuard.get() != NULL); m_Guard.Lock(); }
    ~ScopedLock() { m_Guard.Unlock(); }

  private:
    // object is non copyable, declare private c'tor and assignment operator to prevent compiler warnings
    ScopedLock(ScopedLock const& other);
    ScopedLock& operator=(ScopedLock const& other); 
    CriticalSection& m_Guard;
  };

#define SCOPEDLOCK(guard) tuvok::ScopedLock scopedLock(guard);

  class WaitCondition {
  public:
    WaitCondition();
    ~WaitCondition();

    bool Wait(CriticalSection& criticalSection, uint32_t timeoutInMilliseconds = INFINITE_TIMEOUT); // returns false if timeout occurred
    void WakeOne();
    void WakeAll();

  protected:
#ifdef DETECTED_OS_WINDOWS
    CONDITION_VARIABLE m_cvWaitCondition;
#else
    pthread_cond_t m_cvWaitCondition;
#endif

  private:
    // object is non copyable, declare private c'tor and assignment operator to prevent compiler warnings
    WaitCondition(WaitCondition const&);
    WaitCondition& operator=(WaitCondition const&);
  };

  class ThreadClass {
  public:
    ThreadClass();
    virtual ~ThreadClass(); // will kill thread if not joined in derived class d'tor

    virtual bool StartThread(void* pData = NULL);
    bool JoinThread(uint32_t timeoutInMilliseconds = INFINITE_TIMEOUT);
    bool KillThread(); // only use in extreme cases at own risk

    void RequestThreadStop() { m_bContinue = false; Resume(); }
    bool IsThreadStopRequested() const { return !m_bContinue; }
    bool IsRunning();
    bool Resume();
    typedef std::function<bool ()> PredicateFunction;

  protected:
    virtual void ThreadMain(void* pData = NULL) = 0;
    bool Suspend(PredicateFunction pPredicate = PredicateFunction());
    
    CriticalSection m_SuspendGuard;
    bool            m_bContinue;
#ifdef DETECTED_OS_WINDOWS
    HANDLE          m_hThread;
#else
    pthread_t       m_hThread;
#endif

  private:
    // object is non copyable, declare private c'tor and assignment operator to prevent compiler warnings
    ThreadClass(ThreadClass const&);
    ThreadClass& operator=(ThreadClass const&);

    struct ThreadData {
      void* pData;
      ThreadClass* pThread;
    };
#ifdef DETECTED_OS_WINDOWS
    static DWORD WINAPI StaticStartFunc(LPVOID pThreadStarterData);
#else
    static void* StaticStartFunc(void* pThreadStarterData);
    bool            m_bInitialized;
    bool            m_bJoinable;
    CriticalSection m_JoinMutex;
    WaitCondition   m_JoinWaitCondition;
#endif
    ThreadData*     m_pStartData;
    bool            m_bResumable;
    WaitCondition   m_SuspendWait;
  };

  class LambdaThread : public ThreadClass {
  public:
    // interface to be able to call protected thread methods from some function that gets executed by ThreadMain
    class Interface {
      friend class LambdaThread;
    public:
      bool Suspend(PredicateFunction pPredicate = PredicateFunction()) { return m_Parent.Suspend(pPredicate); }
      CriticalSection& GetSuspendGuard() { return m_Parent.m_SuspendGuard; }

    private:
      // object is non copyable, declare private c'tor and assignment operator to prevent compiler warnings
      Interface(Interface const&);
      Interface& operator=(Interface const&);

      Interface(LambdaThread& parent) : m_Parent(parent) {}
      LambdaThread& m_Parent;
    };

    typedef std::function<void (bool const& bContinue, Interface& threadInterface)> ThreadMainFunction;
    LambdaThread(ThreadMainFunction pMain) : ThreadClass(), m_pMain(pMain) {}

  protected:
    virtual void ThreadMain(void*) { Interface threadInterface(*this); m_pMain(m_bContinue, threadInterface); }

  private:
    ThreadMainFunction m_pMain;
  };

  template<class T>
  class AtomicAccess {
  public:
    AtomicAccess() : m_Value(), m_pGuard(new CriticalSection()) {}
    AtomicAccess(T const& value) : m_Value(value), m_pGuard(new CriticalSection()) {}
    AtomicAccess<T>& operator=(T const& rhs) { SetValue(rhs); return *this; }
    CriticalSection& GetGuard() const { return *(m_pGuard.get()); }
    T GetValue() const { SCOPEDLOCK(GetGuard()); return m_Value; }
    void SetValue(T const& value) { SCOPEDLOCK(GetGuard()); m_Value = value; }

  private:
    T m_Value;
    std::unique_ptr<CriticalSection> m_pGuard;
  };
}

#endif // THREADS_H

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
