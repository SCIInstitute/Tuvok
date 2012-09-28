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
  \file    AbstrRenderer.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef ABSTRRENDERER_H
#define ABSTRRENDERER_H

#include "StdTuvokDefines.h"
#include <string>
#include <memory>

#include "../StdTuvokDefines.h"
#include "../Renderer/CullingLOD.h"
#include "../Renderer/RenderRegion.h"
#include "../IO/Dataset.h"
#include "../Basics/Plane.h"
#include "../Basics/GeometryGenerator.h"
#include "Context.h"
#include "Basics/Interpolant.h"

#include "LuaScripting/LuaScripting.h"
#include "LuaScripting/LuaClassRegistration.h"

class TransferFunction1D;
class TransferFunction2D;

namespace tuvok {

class MasterController;
class RenderMesh;
class LuaDatasetProxy;
class LuaTransferFun1DProxy;
class LuaTransferFun2DProxy;

class Brick {
public:
  Brick() :
    vCenter(0,0,0),
    vExtension(0,0,0),
    vVoxelCount(0, 0, 0),
    vCoords(0,0,0),
    kBrick(),
    fDistance(0),
    bIsEmpty(false)
  {
  }

  Brick(uint32_t x, uint32_t y, uint32_t z,
        uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ,
        const BrickKey& k) :
    vCenter(0,0,0),
    vExtension(0,0,0),
    vVoxelCount(iSizeX, iSizeY, iSizeZ),
    vCoords(x,y,z),
    kBrick(k),
    fDistance(0),
    bIsEmpty(false)
  {
  }

  FLOATVECTOR3 vCenter;
  FLOATVECTOR3 vTexcoordsMin;
  FLOATVECTOR3 vTexcoordsMax;
  FLOATVECTOR3 vExtension;
  UINTVECTOR3 vVoxelCount;
  UINTVECTOR3 vCoords;
  BrickKey kBrick;
  float fDistance;
  bool bIsEmpty;
};

inline bool operator < (const Brick& left, const Brick& right) {
  return (left.fDistance < right.fDistance);
}

/** \class AbstrRenderer
 * Base for all renderers. */
class AbstrRenderer {
  public:

    enum ERendererTarget {
      RT_INTERACTIVE = 0,
      RT_CAPTURE,
      RT_HEADLESS,
      RT_INVALID_MODE
    };

    enum ERendererType {
      RT_SBVR = 0,
      RT_RC,
      RT_INVALID
    };

    enum EStereoMode {
      SM_RB = 0,        // red blue anaglyph
      SM_SCANLINE,      // scan line interleave
      SM_SBS,           // side by side
      SM_AF,            // alternating frame
      SM_INVALID
    };

    enum EStereoID {
      SI_LEFT_OR_MONO = 0,
      SI_RIGHT = 1,
      SI_INVALID
    };

    enum ERenderMode {
      RM_1DTRANS = 0,  /**< one dimensional transfer function */
      RM_2DTRANS,      /**< two dimensional transfer function */
      RM_ISOSURFACE,   /**< render isosurfaces                */
      RM_INVALID
    };

    enum EBlendPrecision {
      BP_8BIT = 0,
      BP_16BIT,
      BP_32BIT,
      BP_INVALID
    };

    enum ScalingMethod {
      SMETH_BIT_WIDTH=0,    ///< scaled based on DS and TF bit width
      SMETH_BIAS_AND_SCALE  ///< bias + scale factors of DS calculated
    };
    /** Default settings: 1D transfer function, single-image view, white text,
     * black BG.
     * @param pMasterController message router
     * @param bUseOnlyPowerOfTwo force power of two textures, for systems that
     *                           do not support npot textures.
     * @param bDownSampleTo8Bits force 8bit textures, for systems that do not
     *                           support 16bit textures (or 16bit linear
     *                           interpolation).
     * @param bDisableBorder     don't use OpenGL borders for textures.
     * @param smeth              method to scale values into TF range */
    AbstrRenderer(MasterController* pMasterController,
                  bool bUseOnlyPowerOfTwo,
                  bool bDownSampleTo8Bits, 
                  bool bDisableBorder,
                  enum ScalingMethod smeth = SMETH_BIT_WIDTH);
    /** Deallocates dataset and transfer functions. */
    virtual ~AbstrRenderer();


    /// Sets the dataset from external source; only meant to be used by clients
    /// which don't want to use the LOD subsystem.
    void SetDataset(Dataset *vds);
/*    /// Modifies previously uploaded data.
    void UpdateData(const BrickKey&,
                    std::shared_ptr<float> fp, size_t len);
*/
    void ClearBricks() { m_pDataset->Clear(); }


    virtual void Set1DTrans(const std::vector<unsigned char>& rgba) = 0;
    /** Notify renderer that 1D TF has changed.  In most cases, this will cause
     * the renderer to start anew. */
    virtual void Changed1DTrans();
    /** Notify renderer that 2D TF has changed.  In most cases, this will cause
     * the renderer to start anew. */
    virtual void Changed2DTrans();


    FLOATVECTOR4 GetTextColor() const {return m_vTextColor;}


    virtual void SetViewPort(UINTVECTOR2 lower_left, UINTVECTOR2 upper_right,
                             bool decrease_screen_res) = 0;



    virtual void SetRotation(RenderRegion * region,
                             const FLOATMATRIX4& rotation);
    virtual const FLOATMATRIX4& GetRotation(const RenderRegion *renderRegion) const;

    virtual void SetTranslation(RenderRegion *renderRegion,
                                const FLOATMATRIX4& translation);
    virtual const FLOATMATRIX4& GetTranslation(
                                        const RenderRegion *renderRegion) const;

    virtual void SetClipPlane(RenderRegion *renderRegion,
                      const ExtendedPlane& plane);
    virtual bool IsClipPlaneEnabled(RenderRegion *renderRegion=NULL);
    virtual void EnableClipPlane(RenderRegion *renderRegion=NULL);
    virtual void DisableClipPlane(RenderRegion *renderRegion=NULL);

    /// slice parameter for slice views.
    virtual void SetSliceDepth(RenderRegion *renderRegion, uint64_t fSliceDepth);
    virtual uint64_t GetSliceDepth(const RenderRegion *renderRegion) const;

    void SetClearFramebuffer(bool bClearFramebuffer) {
      m_bClearFramebuffer = bClearFramebuffer;
    }
    bool GetClearFramebuffer() const {return m_bClearFramebuffer;}

    void Set2DFlipMode(RenderRegion *renderRegion, bool bFlipX, bool bFlipY);
    void Get2DFlipMode(const RenderRegion *renderRegion, bool& bFlipX,
                       bool& bFlipY) const;
    bool GetUseMIP(const RenderRegion *renderRegion) const;
    void SetUseMIP(RenderRegion *renderRegion, bool bUseMIP);

    virtual void SetUserMatrices(const FLOATMATRIX4& view, const FLOATMATRIX4& projection,
                                 const FLOATMATRIX4& viewLeft, const FLOATMATRIX4& projectionLeft,
                                 const FLOATMATRIX4& viewRight, const FLOATMATRIX4& projectionRight);
    virtual void UnsetUserMatrices();

    // ClearView

    virtual INTVECTOR2 GetCVMousePos() const {return m_vCVMousePos;}

    virtual FLOATVECTOR3 Pick(const UINTVECTOR2&) const = 0;

    virtual void ScheduleWindowRedraw(RenderRegion *renderRegion);


    void SetViewParameters(float angle, float znear, float zfar,
                           const FLOATVECTOR3& eye,
                           const FLOATVECTOR3& ref,
                           const FLOATVECTOR3& vup) {
      m_vEye = eye;
      m_vAt = ref;
      m_vUp = vup;
      m_fFOV = angle;
      m_fZNear = znear;
      m_fZFar = zfar;
      this->ScheduleCompleteRedraw();
    }

    void SetScalingMethod(enum ScalingMethod sm) {
      this->m_TFScalingMethod = sm;
    }

    virtual void NewFrameClear(const RenderRegion &) = 0;


    const FLOATMATRIX4& GetProjectionMatrix(size_t eyeIndex = 0) const {return m_mProjection[eyeIndex];}
    const FLOATMATRIX4& GetViewMatrix(size_t eyeIndex = 0) const {return m_mView[eyeIndex];}

    bool Execute(const std::string& strCommand,
                 const std::vector<std::string>& strParams,
                 std::string& strMessage);

    std::shared_ptr<Context> GetContext() const {return m_pContext;}

    /// Registers base class Lua functions.
    static void RegisterLuaFunctions(LuaClassRegistration<AbstrRenderer>& reg,
                                     AbstrRenderer* me,
                                     LuaScripting* ss);

    /// This function is called at the end of RegisterLuaFunctions.
    /// Use it to register additional renderer specific functions.
    /// (functions that don't make sense as an inherited abstract method).
    virtual void RegisterDerivedClassLuaFunctions(
        LuaClassRegistration<AbstrRenderer>&,
        LuaScripting*) 
    {}

    /// "PH" == "paper hacks".  sorry.  delete these after pacvis.
    ///@{
    virtual void PH_ClearWorkingSet();
    virtual void PH_SetPagedBricks(size_t bricks);
    virtual size_t PH_FramePagedBricks() const;
    virtual size_t PH_SubframePagedBricks() const;
    virtual void PH_RecalculateVisibility();
    virtual bool PH_Converged() const;

    virtual double PH_BrickIOTime() const;
    virtual void PH_SetBrickIOTime(double d);
    virtual uint64_t PH_BrickIOBytes() const;
    virtual void PH_SetBrickIOBytes(uint64_t b);
    virtual double PH_RenderingTime() const;

    virtual bool PH_OpenLogfile(const std::string&);
    virtual bool PH_CloseLogfile();
    virtual void PH_SetOptimalFrameAverageCount(size_t);
    virtual size_t PH_GetOptimalFrameAverageCount() const;
    virtual bool PH_IsDebugViewAvailable() const;
    virtual bool PH_IsWorkingSetTrackerAvailable() const;
    ///@}

  public:
    // The following functions are public ONLY because there are dependencies
    // upon them within Tuvok. Their dependencies are listed.

    // Dependency: RenderRegion.cpp
    virtual void ShowClipPlane(bool, RenderRegion *renderRegion=NULL);

    // Dependency: RenderRegion.cpp
    const ExtendedPlane& GetClipPlane() const;

  protected:
    // Functions in this section were made protected due to the transition to 
    // Lua. All functions in this section are exposed through the Lua interface.

    virtual ERendererType GetRendererType() const {return RT_INVALID;}
    virtual void SetRendermode(ERenderMode eRenderMode);
    virtual void SetBlendPrecision(EBlendPrecision eBlendPrecision);
    
    /** Sets up a gradient background which fades vertically.
     * @param vColors[0] is the color at the bottom;
     * @param vColors[1] is the color at the top. */
    virtual bool SetBackgroundColors(FLOATVECTOR3 vTopColor,
                                     FLOATVECTOR3 vBotColor) {
      if (vTopColor != m_vBackgroundColors[0] ||
          vBotColor != m_vBackgroundColors[1]) {
        m_vBackgroundColors[0]=vTopColor;
        m_vBackgroundColors[1]=vBotColor;
        ScheduleCompleteRedraw();
        return true;
      } return false;
    }

    virtual bool SetTextColor(FLOATVECTOR4 vColor) {
      if (vColor != m_vTextColor) {
        m_vTextColor=vColor;ScheduleCompleteRedraw();
        return true;
      } return false;
    }

    virtual void SetLogoParams(std::string strLogoFilename, int iLogoPos);
    virtual void ClipPlaneRelativeLock(bool);
    virtual bool CanDoClipPlane() {return true;}

    // Clear view
    virtual bool SupportsClearView() {return false;}
    virtual std::string ClearViewDisableReason() const {
      return "this renderer does not support ClearView";
    }
    virtual void SetCV(bool bEnable);
    virtual bool GetCV() const {return m_bDoClearView;}
    virtual void SetCVIsoValue(float fIsovalue);
    virtual float GetCVIsoValue() const { return m_fCVIsovalue; }
    virtual void SetCVColor(const FLOATVECTOR3& vColor);
    virtual FLOATVECTOR3 GetCVColor() const {return m_vCVColor;}
    virtual void SetCVSize(float fSize);
    virtual float GetCVSize() const {return m_fCVSize;}
    virtual void SetCVContextScale(float fScale);
    virtual float GetCVContextScale() const {return m_fCVContextScale;}
    virtual void SetCVBorderScale(float fScale);
    virtual float GetCVBorderScale() const {return m_fCVBorderScale;}
    virtual void SetCVFocusPosFVec(const FLOATVECTOR4& vCVPos);
    virtual void SetCVFocusPos(LuaClassInstance renderRegion,
                               const INTVECTOR2& vPos);
    virtual FLOATVECTOR4 GetCVFocusPos() const {return m_vCVPos;}

    RenderRegion3D* GetFirst3DRegion();

    virtual void SetRenderCoordArrows(bool bRenderCoordArrows);
    virtual bool GetRenderCoordArrows() const {return m_bRenderCoordArrows;}

    virtual void Transfer3DRotationToMIP();

    void SetMIPLOD(bool bMIPLOD) {m_bMIPLOD = bMIPLOD;}

    virtual void  SetStereo(bool bStereoRendering);
    virtual void  SetStereoEyeDist(float fStereoEyeDist);
    virtual void  SetStereoFocalLength(float fStereoFocalLength);
    virtual void  SetStereoMode(EStereoMode mode);
    virtual void  SetStereoEyeSwap(bool bSwap);
    virtual bool  GetStereo() const {return m_bRequestStereoRendering;}
    virtual float GetStereoEyeDist() const {return m_fStereoEyeDist;}
    virtual float GetStereoFocalLength() const {return m_fStereoFocalLength;}
    virtual EStereoMode GetStereoMode() const {return m_eStereoMode;}
    virtual bool  GetStereoEyeSwap() const {return m_bStereoEyeSwap;}
    virtual void InitStereoFrame();
    virtual void ToggleStereoFrame();

    virtual void ScheduleCompleteRedraw();
    /** Query whether or not we should redraw the next frame, else we should
     * reuse what is already rendered or continue with the current frame if it
     * is not complete yet. */
    virtual bool CheckForRedraw();

    /** Deallocates GPU memory allocated during the rendering process. */
    virtual void Cleanup() = 0;

    virtual void SetOrthoView(bool bOrthoView);
    virtual bool GetOrthoView() const {return m_bOrthoView;}

    virtual void SetUseLighting(bool bUseLighting);

    virtual void SetSampleRateModifier(float fSampleRateModifier);

    virtual void SetIsoValue(float fIsovalue);
    /// the given isovalue is in the range [0,1] and is interpreted as a
    /// percentage of the available isovalues.
    virtual void SetIsoValueRelative(float fIsovalue);

    virtual void SetIsosufaceColor(const FLOATVECTOR3& vColor);
    virtual FLOATVECTOR3 GetIsosufaceColor() const { return m_vIsoColor; }

    UINTVECTOR2 GetSize() const {return m_vWinSize;}

    /** Change the size of the render window.  Any previous image is
     * destroyed, causing a full redraw on the next render.
     * \param vWinSize  new width and height of the view window */
    virtual void Resize(const UINTVECTOR2& vWinSize);

    virtual bool Paint() {
      if (m_bDatasetIsInvalid) return true;

      if (renderRegions.empty()) {
        renderRegions.push_back(&simpleRenderRegion3D);
      }
      if (renderRegions.size() == 1 && renderRegions[0] == &simpleRenderRegion3D) {
        simpleRenderRegion3D.maxCoord = m_vWinSize; //minCoord is always (0,0)
      }

      // check if we are rendering a stereo frame
      m_bDoStereoRendering = m_bRequestStereoRendering &&
                             renderRegions.size() == 1 &&
                             renderRegions[0]->is3D();

      return true; // nothing can go wrong here
    }

    virtual void  SetConsiderPreviousDepthbuffer(bool bConsiderPreviousDepthbuffer);
    virtual bool  GetConsiderPreviousDepthbuffer() const {
      return m_bConsiderPreviousDepthbuffer;
    }

    /** Sends a message to the master to ask for a dataset to be loaded.
     * @param strFilename path to a file */
    virtual bool LoadDataset(const std::string& strFilename);

    virtual bool Initialize(std::shared_ptr<Context> ctx);

    virtual void SetInterpolant(Interpolant eInterpolant);

    virtual void ScanForNewMeshes() {}
    virtual void RemoveMeshData(size_t index);

    virtual void SetRescaleFactors(const DOUBLEVECTOR3& vfRescale) {
      m_pDataset->SetRescaleFactors(vfRescale);
      ScheduleCompleteRedraw();
    }

    virtual void Schedule3DWindowRedraws(); // Redraw all 3D windows.

    virtual bool CropDataset(const std::string& strTempDir, bool bKeepOldData) = 0;

    ExtendedPlane LuaGetClipPlane() const;

    void GetVolumeAABB(FLOATVECTOR3& vCenter, FLOATVECTOR3& vExtend) const;

    float GetIsoValue() const { return m_fIsovalue; }

    virtual void CycleDebugViews();
    virtual void SetDebugView(uint32_t iDebugView);
    virtual uint32_t GetDebugViewCount() const;
    uint32_t GetDebugView() const;

  private:
    // Functions in this section were made private due to the transition to Lua.
    // All functions in this section are exposed through the Lua interface.
    // If a derived class needs access to these functions, it is appropriate
    // to move the function to protected, but not public.

    ERendererTarget GetRendererTarget() const { return m_eRendererTarget; }

    void SetRendererTarget(ERendererTarget eRendererTarget) {
      m_eRendererTarget = eRendererTarget;
    }

    void SetViewPos(const FLOATVECTOR3& vPos);
    FLOATVECTOR3 GetViewPos() const;
    void ResetViewPos();
    void SetViewDir(const FLOATVECTOR3& vDir);
    FLOATVECTOR3 GetViewDir() const;
    void ResetViewDir();
    void SetUpDir(const FLOATVECTOR3& vDir);
    FLOATVECTOR3 GetUpDir() const;
    void ResetUpDir();

    ERenderMode GetRendermode() {return m_eRenderMode;}

    Dataset&       GetDataset()       { return *m_pDataset; }
    const Dataset& GetDataset() const { return *m_pDataset; }

    TransferFunction1D* Get1DTrans() {return m_p1DTrans;}
    TransferFunction2D* Get2DTrans() {return m_p2DTrans;}

    FLOATVECTOR3 GetBackgroundColor(int i) const {
      return m_vBackgroundColors[i];
    }

    EBlendPrecision GetBlendPrecision() {return m_eBlendPrecision;}

    void SetRenderRegions(const std::vector<RenderRegion*>&);
    void LuaSetRenderRegions(std::vector<LuaClassInstance>);

    const std::vector<RenderRegion*>& GetRenderRegions() const {
      return renderRegions;
    }
    std::vector<LuaClassInstance> LuaGetRenderRegions();

    LuaClassInstance LuaGetFirst3DRegion();

    bool ClipPlaneLocked() const  { return m_bClipPlaneLocked; }
    bool ClipPlaneEnabled() const { return m_bClipPlaneOn; }
    bool ClipPlaneShown() const   { return m_bClipPlaneDisplayed; }

    bool SupportsMeshes() const {return m_bSupportsMeshes;}

    void SetMIPRotationAngle(float fAngle) {
      m_fMIPRotationAngle = fAngle;
      ScheduleCompleteRedraw();
    }

    virtual void Set2DPlanesIn3DView(bool bRenderPlanesIn3D,
                                     RenderRegion* renderRegion=NULL);
    virtual bool Get2DPlanesIn3DView(RenderRegion* = NULL) const {
      /// @todo: Make this bool a per 3d render region toggle.
      return m_bRenderPlanesIn3D;}

    void LuaSet2DPlanesIn3DView(bool bRenderPlanesIn3D,LuaClassInstance region);
    bool LuaGet2DPlanesIn3DView() const {return Get2DPlanesIn3DView();}

    void SetTimeSlice(uint32_t iMSecs) {m_iTimeSliceMSecs = iMSecs;}

    void SetPerfMeasures(uint32_t iMinFramerate, 
                         bool bRenderLowResIntermediateResults,
                         float fScreenResDecFactor,
                         float fSampleDecFactor, uint32_t iStartDelay);

    void LuaCloneRenderMode(LuaClassInstance inst);

    bool GetUseLighting() const { return m_bUseLighting; }

    void SetFoV(float fFoV);
    float GetFoV() const {return m_fFOV;}

    void SetTimestep(size_t);
    size_t Timestep() const;

    void SetGlobalBBox(bool bRenderBBox);
    bool GetGlobalBBox() const {return m_bRenderGlobalBBox;}
    void SetLocalBBox(bool bRenderBBox);
    bool GetLocalBBox() const {return m_bRenderLocalBBox;}

    void LuaShowClipPlane(bool bShown, LuaClassInstance inst);


    // scheduling routines
    uint64_t GetCurrentSubFrameCount() const
        { return 1+m_iMaxLODIndex-m_iMinLODForCurrentView; }
    uint32_t GetWorkingSubFrame() const
        { return 1+static_cast<uint32_t>(m_iMaxLODIndex-m_iCurrentLOD); }

    uint32_t GetCurrentBrickCount() const
        { return uint32_t(m_vCurrentBrickList.size()); }
    uint32_t GetWorkingBrick() const
        { return static_cast<uint32_t>(m_iBricksRenderedInThisSubFrame); }

    virtual uint32_t GetFrameProgress() const {
        return uint32_t(100.0f * float(GetWorkingSubFrame()) /
                      float(GetCurrentSubFrameCount()));
    }
    virtual uint32_t GetSubFrameProgress() const {
        return (m_vCurrentBrickList.empty()) ? 100 :
                uint32_t(100.0f * m_iBricksRenderedInThisSubFrame /
                float(m_vCurrentBrickList.size()));
    }

    uint64_t GetMaxLODIndex() const { return m_iMaxLODIndex; }
    uint64_t GetMinLODIndex() const { return m_iMinLODForCurrentView; }

    bool GetUseOnlyPowerOfTwo() const {return m_bUseOnlyPowerOfTwo;}
    bool GetDownSampleTo8Bits() const {return m_bDownSampleTo8Bits;}
    bool GetDisableBorder() const {return m_bDisableBorder;}

    /// Prepends the given directory to the list of paths Tuvok will
    /// try to find shaders in.
    void AddShaderPath(const std::string &path) {
      m_vShaderSearchDirs.insert(m_vShaderSearchDirs.begin(), path);
    }

    UINTVECTOR2 GetLODLimits() const { return m_iLODLimits; }
    void SetLODLimits(const UINTVECTOR2 iLODLimits);

    void SetColors(FLOATVECTOR4 ambient,
                   FLOATVECTOR4 diffuse,
                   FLOATVECTOR4 specular,
                   FLOATVECTOR3 lightDir);

    Interpolant GetInterpolant() const {return m_eInterpolant;}

    FLOATVECTOR4 GetAmbient() const;
    FLOATVECTOR4 GetDiffuse() const;
    FLOATVECTOR4 GetSpecular()const;
    FLOATVECTOR3 GetLightDir()const;

    void SetDatasetIsInvalid(bool bDatasetIsInvalid) {
      m_bDatasetIsInvalid = bDatasetIsInvalid;
    }

    // Return value references are not supported by lua.
    std::vector<std::shared_ptr<RenderMesh> > GetMeshes() {return m_Meshes;}
    void ReloadMesh(size_t index, const std::shared_ptr<Mesh> m);

    float GetSampleRateModifier() const { return m_fSampleRateModifier; }

    DOUBLEVECTOR3 GetRescaleFactors() const {
      return m_pDataset->GetRescaleFactors();
    }

    FLOATVECTOR3 LuaGetVolumeAABBExtents() const;
    FLOATVECTOR3 LuaGetVolumeAABBCenter() const;

    virtual void FixedFunctionality() const = 0;
    virtual void SyncStateManager() = 0;

  protected:
    /// Unsets the current transfer function, including deleting it from GPU
    /// memory.  It's expected you'll set another one directly afterwards.
    void Free1DTrans();

    /// This is the function that should be called after creating a new 1D
    /// transfer function and associating it with the variable m_p1DTrans.
    /// @todo Find a more elegant way of doing this.
    void LuaBindNew1DTrans();

    /// This is the function that should be called after creating a new 2D
    /// transfer function and associating it with the variable m_p2DTrans.
    void LuaBindNew2DTrans();

    /// @return the current iso value, normalized to be in [0,1]
    double GetNormalizedIsovalue() const;
    /// @return the current clearview iso value, normalized to be in [0,1]
    double GetNormalizedCVIsovalue() const;

    virtual void        ClearColorBuffer() const = 0;
    virtual void        UpdateLightParamsInShaders() = 0;
    virtual void        CVFocusHasChanged(LuaClassInstance region);

    /// @returns if the data we're rendering is color or not.
    virtual bool ColorData() const;


  protected:
    MasterController*   m_pMasterController;
    std::shared_ptr<Context> m_pContext;
    ERenderMode         m_eRenderMode;
    bool                m_bFirstDrawAfterModeChange;
    bool                m_bFirstDrawAfterResize;
    EBlendPrecision     m_eBlendPrecision;
    bool                m_bUseLighting;
    Dataset*            m_pDataset;
    TransferFunction1D* m_p1DTrans;
    TransferFunction2D* m_p2DTrans;
    float               m_fSampleRateModifier;
    FLOATVECTOR3        m_vIsoColor;
    FLOATVECTOR3        m_vBackgroundColors[2];
    FLOATVECTOR4        m_vTextColor;
    bool                m_bRenderGlobalBBox;
    bool                m_bRenderLocalBBox;
    UINTVECTOR2         m_vWinSize;
    int                 m_iLogoPos;
    std::string         m_strLogoFilename;
    uint32_t            m_iDebugView;

    Interpolant         m_eInterpolant;

    bool                m_bSupportsMeshes;
    std::vector<std::shared_ptr<RenderMesh> > m_Meshes;


    /// parameters for dynamic resolution adjustments
    ///@{
    float  msecPassed[2]; // time taken for last two frames
    float  msecPassedCurrentFrame; // time taken for our current rendering
    uint32_t m_iLODNotOKCounter; // n frames waited for render to become faster
    bool decreaseScreenRes; ///< dec.'d display resolution (lower n_fragments)
    bool decreaseScreenResNow;
    bool decreaseSamplingRate; ///< dec.'d sampling rate (less shader work)
    bool decreaseSamplingRateNow;
    bool doAnotherRedrawDueToLowResOutput; ///< previous subframe had res or rate
                                       ///  reduced; we need another render to
                                       ///  complete finish up.
    float  m_fMaxMSPerFrame;
    float  m_fScreenResDecFactor;
    float  m_fSampleDecFactor;
    bool   m_bRenderLowResIntermediateResults;
    bool   m_bOffscreenIsLowRes;
    uint32_t m_iStartDelay;
    uint64_t m_iMinLODForCurrentView;
    uint32_t m_iTimeSliceMSecs;
    ///@}

    uint64_t            m_iIntraFrameCounter;
    uint64_t            m_iFrameCounter;
    uint32_t            m_iCheckCounter;
    uint64_t            m_iMaxLODIndex;
    UINTVECTOR2         m_iLODLimits;
    uint64_t            m_iPerformanceBasedLODSkip;
    uint64_t            m_iCurrentLODOffset;
    uint64_t            m_iStartLODOffset;
    size_t              m_iTimestep;
    CullingLOD          m_FrustumCullingLOD;
    bool                m_bClearFramebuffer;
    bool                m_bConsiderPreviousDepthbuffer;
    uint64_t            m_iCurrentLOD;
    uint64_t            m_iBricksRenderedInThisSubFrame;
    std::vector<Brick>  m_vCurrentBrickList;
    std::vector<Brick>  m_vLeftEyeBrickList;
    ERendererTarget     m_eRendererTarget;
    bool                m_bMIPLOD;
    float               m_fMIPRotationAngle;
    FLOATMATRIX4        m_maMIPRotation;
    bool                m_bOrthoView;
    bool                m_bRenderCoordArrows;
    bool                m_bRenderPlanesIn3D;

    bool                m_bDoClearView;
    FLOATVECTOR3        m_vCVColor;
    float               m_fCVSize;
    float               m_fCVContextScale;
    float               m_fCVBorderScale;
    INTVECTOR2          m_vCVMousePos;
    FLOATVECTOR4        m_vCVPos;
    bool                m_bPerformReCompose;
    bool                m_bRequestStereoRendering;
    bool                m_bDoStereoRendering;
    bool                m_bUserMatrices;
    FLOATMATRIX4        m_UserView;
    FLOATMATRIX4        m_UserViewLeft;
    FLOATMATRIX4        m_UserViewRight;
    FLOATMATRIX4        m_UserProjection;
    FLOATMATRIX4        m_UserProjectionLeft;
    FLOATMATRIX4        m_UserProjectionRight;
  	int				          m_iAlternatingFrameID;
    float               m_fStereoEyeDist;
    float               m_fStereoFocalLength;
    EStereoMode         m_eStereoMode;
    bool                m_bStereoEyeSwap;
    std::vector<Triangle> m_vArrowGeometry;
    bool                m_bDatasetIsInvalid;

    // compatibility settings
    bool                m_bUseOnlyPowerOfTwo;
    bool                m_bDownSampleTo8Bits;
    bool                m_bDisableBorder;
    enum ScalingMethod  m_TFScalingMethod;

    FLOATMATRIX4        m_mProjection[2];
    FLOATMATRIX4        m_mView[2];
    std::vector<std::string> m_vShaderSearchDirs;

    ExtendedPlane       m_ClipPlane;
    bool                m_bClipPlaneOn;
    bool                m_bClipPlaneDisplayed;
    bool                m_bClipPlaneLocked;

    /// view parameters.
    ///@{
    FLOATVECTOR3        m_vEye, m_vAt, m_vUp;
    float               m_fFOV;
    float               m_fZNear, m_fZFar;
    bool                m_bFirstPersonMode;
    ///@}

    std::vector<RenderRegion*> renderRegions;

    // For displaying a full single 3D view without the client needing to know
    // about RenderRegions.
    RenderRegion3D simpleRenderRegion3D;

    // colors for the volume light
    FLOATVECTOR4        m_cAmbient;
    FLOATVECTOR4        m_cDiffuse;
    FLOATVECTOR4        m_cSpecular;
    // colors for the mesh light
    FLOATVECTOR4        m_cAmbientM;
    FLOATVECTOR4        m_cDiffuseM;
    FLOATVECTOR4        m_cSpecularM;
    // light direction (for both)
    FLOATVECTOR3        m_vLightDir;

    virtual void        ScheduleRecompose(RenderRegion *renderRegion=NULL);
    void                ComputeMinLODForCurrentView();
    void                ComputeMaxLODForCurrentView();
    virtual void        PlanFrame(RenderRegion3D& region);
    void                PlanHQMIPFrame(RenderRegion& renderRegion);
    /// @return true if the brick is needed to render the given region
    bool RegionNeedsBrick(const RenderRegion& rr, const BrickKey& key,
                          const BrickMD& bmd,
                          bool& bIsEmptyButInFrustum) const;
    /// @return true if this brick is clipped by a clipping plane.
    bool Clipped(const RenderRegion&, const Brick&) const;
    /// does the current brick contain relevant data?
    bool ContainsData(const BrickKey&) const;
    std::vector<Brick>  BuildSubFrameBrickList(bool bUseResidencyAsDistanceCriterion=false);
    std::vector<Brick>  BuildLeftEyeSubFrameBrickList(
                          const FLOATMATRIX4& modelview,
                          const std::vector<Brick>& vRightEyeBrickList
                        ) const;
    void                CompletedASubframe(RenderRegion* region);
    void                RestartTimer(const size_t iTimerIndex);
    void                RestartTimers();
    double              MaxValue() const;
    bool                OnlyRecomposite(RenderRegion* region) const;


    virtual bool IsVolumeResident(const BrickKey& key) const = 0;

    /// Since m_Meshes no longer gets returned as a reference from GetMeshes,
    /// we need to implement these mutation functions to provide support via
    /// lua.
    void ClearRendererMeshes();

    LuaClassInstance m_pLua2DTrans;
    LuaTransferFun2DProxy* m_pLua2DTransPtr; /// Used by GLRenderer.
    LuaClassInstance LuaGet2DTrans();

  private:
    float               m_fIsovalue;
    float               m_fCVIsovalue;


    // Private Lua-only functions/variables
  private:

    LuaClassInstance m_pLuaDataset;
    LuaDatasetProxy* m_pLuaDatasetPtr;
    LuaClassInstance LuaGetDataset();

    LuaClassInstance m_pLua1DTrans;
    LuaTransferFun1DProxy* m_pLua1DTransPtr;
    LuaClassInstance LuaGet1DTrans();

};

}; //namespace tuvok

#endif // ABSTRRENDERER_H
