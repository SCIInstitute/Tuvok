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

//!    File   : ImageVis3D.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : September 2008
//
//!    Copyright (C) 2008 SCI Institute
#pragma once

#ifndef MASTERCONTROLLER_H
#define MASTERCONTROLLER_H

#include "../StdTuvokDefines.h"
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "Basics/CommandReturn.h"
#include "Basics/TuvokException.h"
#include "Basics/Vectors.h"
#include "../DebugOut/MultiplexOut.h"
#include "../DebugOut/ConsoleOut.h"

#include "LuaScripting/LuaClassInstance.h"

class SystemInfo;
class IOManager;

namespace tuvok {

class AbstrRenderer;
class GPUMemMan;
class Scripting;
class LuaScripting;
class LuaMemberReg;
class LuaIOManagerProxy;
class RenderRegion;

typedef std::deque<AbstrRenderer*> AbstrRendererList;

struct PH_HackyState {
  enum BrickStrategy {
    BS_OnlyNeeded=0, BS_RequestAll, BS_SkipOneLevel, BS_SkipTwoLevels
  };
  BrickStrategy BStrategy;
  uint32_t RehashCount;
};

/** \class MasterController
 * Centralized controller for Tuvok.
 *
 * MasterController is a router for all of the components of
 * Tuvok.  We try to keep modules communicating through this controller,
 * as opposed to directly with other modules.
 * You probably don't want to create an instance directly.  Use the singleton
 * provided by Controller::Instance(). */
class MasterController {
public:
  enum EVolumeRendererType {
    OPENGL_SBVR = 0,
    OPENGL_RAYCASTER,
    DIRECTX_SBVR,
    DIRECTX_RAYCASTER,
    OPENGL_2DSBVR,
    DIRECTX_2DSBVR,
    OPENGL_TRAYCASTER,
    DIRECTX_TRAYCASTER,
    RENDERER_LAST,
  };

  /// Defaults to using a Console-based debug output stream.
  MasterController();
  virtual ~MasterController();

  /// Deallocate any memory we're holding; rendering or doing any real work is
  /// undefined after this operation.
  void Cleanup();

  /// Lua scripting engine.
  ///@{
  std::shared_ptr<LuaScripting>  LuaScript()       { return m_pLuaScript; }
  ///@}

  /// Add another debug output
  /// \param debugOut      the new stream
  void AddDebugOut(AbstrDebugOut* debugOut);

  /// Removes the given debug output stream.
  /// The stream must be the currently connected/used one.
  void RemoveDebugOut(AbstrDebugOut* debugOut);

  /// Access the currently-active debug stream.
  ///@{
  AbstrDebugOut* DebugOut();
  const AbstrDebugOut *DebugOut() const;
  ///@}


  /// The GPU memory manager moves data from CPU to GPU memory, and
  /// removes data from GPU memory.
  ///@{
  GPUMemMan*       MemMan()       { return m_pGPUMemMan; }
  const GPUMemMan* MemMan() const { return m_pGPUMemMan; }
  ///@}

  /// The IO manager is responsible for loading data into host memory.
  ///@{
  IOManager*       IOMan()       { return m_pIOManager;}
  const IOManager* IOMan() const { return m_pIOManager;}
  ///@}

  /// System information is for looking up host parameters, such as the
  /// amount of memory available.
  ///@{
  SystemInfo*       SysInfo()       { return m_pSystemInfo; }
  const SystemInfo* SysInfo() const { return m_pSystemInfo; }
  ///@}

  /// \todo this should return a pointer to memory.
  void Filter(std::string datasetName,
              uint32_t filter,
              void *var0 = 0, void *var1 = 0,
              void *var2 = 0, void *var3 = 0 );

  /// Whether or not to expose certain features which aren't actually ready for
  /// users.
  bool ExperimentalFeatures() const;
  void ExperimentalFeatures(bool b);


  /// Indicate that a renderer is no longer needed.
  /// This is now called from Abstract renderer's destructor.
  /// All LuaClassInstances should be destroyed with the destroyClass lua call.
  /// @{
  void ReleaseVolumeRenderer(AbstrRenderer* pVolumeRenderer);
  void ReleaseVolumeRenderer(LuaClassInstance pVolumeRenderer);
  /// @}

  /// disgusting centralized storage for state we need to record for the paper
  ///@{
  void SetBrickStrategy(size_t strat);
  void SetRehashCount(uint32_t count);
  void SetMaxGPUMem(uint64_t megs);
  PH_HackyState PHState;
  ///@}

private:

  /// Returns the most recently registered renderer.
  const AbstrRenderer *Renderer() const {
    return m_vVolumeRenderer[m_vVolumeRenderer.size()-1];
  }


  /// Initializer; add all our builtin commands.
  void RegisterInternalCommands();
  void RegisterLuaCommands();

  RenderRegion* LuaCreateRenderRegion3D(LuaClassInstance ren);
  RenderRegion* LuaCreateRenderRegion2D(int mode,  // RenderRegion::EWindowMode
                                        uint64_t sliceIndex,
                                        LuaClassInstance ren);

  /// Helper function for RegisterLuaCommands. Helps in setting up the renderer
  /// types table in tuvok.renderer.
  void AddLuaRendererType(const std::string& rendererLoc,
                          const std::string& rendererName,
                          int value);

  //--------------
  // Lua commands
  //--------------

  /// Creates a new volume renderer.
  /// Used as a constructor function for Lua classes.
  AbstrRenderer* RequestNewVolumeRenderer(EVolumeRendererType eRendererType,
                                          bool bUseOnlyPowerOfTwo,
                                          bool bDownSampleTo8Bits,
                                          bool bDisableBorder,
                                          bool bBiasAndScaleTF);

private:
  SystemInfo*      m_pSystemInfo;
  GPUMemMan*       m_pGPUMemMan;
  IOManager*       m_pIOManager;
  MultiplexOut     m_DebugOut;
  ConsoleOut       m_DefaultOut;
  bool             m_bDeleteDebugOutOnExit;
  bool             m_bExperimentalFeatures;

  std::shared_ptr<LuaScripting>       m_pLuaScript;
  std::unique_ptr<LuaMemberReg>       m_pMemReg;
  std::unique_ptr<LuaIOManagerProxy>  m_pIOProxy; 

  AbstrRendererList m_vVolumeRenderer;
  // The active renderer should point into a member of the renderer list.
  AbstrRenderer*   m_pActiveRenderer;
};

}
#endif // MASTERCONTROLLER_H
