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
  \file    GLRenderer.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef GLRENDERER_H
#define GLRENDERER_H

#include <string>
#include <vector>

#include "../../StdTuvokDefines.h"
#include <Basics/Timer.h>
#include "../AbstrRenderer.h"
#include "GLTargetBinder.h"

namespace tuvok {

class MasterController;
class GLTexture1D;
class GLTexture2D;
class GLTexture3D;
class GLSLProgram;

class GLRenderer : public AbstrRenderer {
  public:
    GLRenderer(MasterController* pMasterController, bool bUseOnlyPowerOfTwo,
               bool bDownSampleTo8Bits, bool bDisableBorder);
    virtual ~GLRenderer();
    virtual bool Initialize();
    virtual void Set1DTrans(const std::vector<unsigned char>&);
    virtual void Changed1DTrans();
    virtual void Changed2DTrans();

    /** Set the bit depth mode of the offscreen buffer used for blending.
        Causes a full redraw. */
    virtual void SetBlendPrecision(EBlendPrecision eBlendPrecision);

    /** Deallocates GPU memory allocated during the rendering process. */
    virtual void Cleanup();

    /** Paint the image */
    virtual void Paint();

    /** Sends a message to the master to ask for a dataset to be loaded.
     * The dataset is converted to UVF if it is not one already.
     * @param strFilename path to a file */
    virtual bool LoadDataset(const std::string& strFilename, bool& bRebrickingRequired);

    /** Change the size of the FBO we render to.  Any previous image is
     * destroyed, causing a full redraw on the next render.
     * \param vWinSize  new width and height of the view window */
    virtual void Resize(const UINTVECTOR2& vWinSize);

    virtual void SetLogoParams(std::string strLogoFilename, int iLogoPos);

    void RenderSlice(const RenderRegion2D& region, double fSliceIndex,
                     FLOATVECTOR3 vMinCoords, FLOATVECTOR3 vMaxCoords,
                     DOUBLEVECTOR3 vAspectRatio, DOUBLEVECTOR2 vWinAspectRatio);
    virtual void NewFrameClear(const RenderRegion& renderRegion);

  protected:
    GLTargetBinder  m_TargetBinder;
    GLTexture1D*    m_p1DTransTex;
    GLTexture2D*    m_p2DTransTex;
    std::vector<unsigned char> m_p1DData;
    unsigned char*  m_p2DData;
    GLFBOTex*       m_pFBO3DImageLast;
    GLFBOTex*       m_pFBO3DImageCurrent[2];
    GLFBOTex*       m_pFBOResizeQuickBlit;
    GLFBOTex*       m_pFBOIsoHit[2];
    GLFBOTex*       m_pFBOCVHit[2];
    bool            m_bFirstDrawAfterResize;
    GLTexture2D*    m_pLogoTex;
    GLSLProgram*    m_pProgram1DTrans[2];
    GLSLProgram*    m_pProgram2DTrans[2];
    GLSLProgram*    m_pProgramIso;
    GLSLProgram*    m_pProgramColor;
    GLSLProgram*    m_pProgramHQMIPRot;
    Timer           m_Timer;

    void SetRenderTargetArea(const RenderRegion& renderRegion,
                             bool bDecreaseScreenResNow);
    void SetRenderTargetArea(UINTVECTOR2 minCoord, UINTVECTOR2 maxCoord,
                             bool bDecreaseScreenResNow);
    void SetRenderTargetAreaScissor(const RenderRegion& renderRegion);
    void SetViewPort(UINTVECTOR2 viLowerLeft, UINTVECTOR2 viUpperRight,
                     bool bDecreaseScreenResNow);

    bool Render2DView(RenderRegion2D& renderRegion);
    void RenderBBox(const FLOATVECTOR4 vColor = FLOATVECTOR4(1,0,0,1),
                    bool bEpsilonOffset=true);
    void RenderBBox(const FLOATVECTOR4 vColor, bool bEpsilonOffset,
                    const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend);
    void RenderClipPlane(size_t iStereoID);
    bool Execute3DFrame(RenderRegion3D& renderRegion, float& fMsecPassed);
    void CopyImageToDisplayBuffer();
    void DrawLogo();
    void DrawBackGradient();

    /// Defines a value to use for scaling the TF.  Client apps which hand us
    /// over the raw data will usually want to override this to be 1, since
    /// they generally won't support any notion of TF scaling.
    virtual float CalculateScaling();
    virtual void SetDataDepShaderVars();

    virtual float Render3DView(RenderRegion3D& renderRegion);
    virtual void Render3DPreLoop(RenderRegion3D &) { };
    virtual void Render3DInLoop(RenderRegion3D& renderRegion,
                                size_t iCurentBrick, int iStereoID) = 0;
    virtual void Render3DPostLoop() {}
    virtual void ComposeSurfaceImage(RenderRegion& renderRegion, int iStereoID);
    virtual void Recompose3DView(RenderRegion3D& renderRegion);

    virtual void RenderHQMIPPreLoop(RenderRegion2D& region);
    virtual void RenderHQMIPInLoop(RenderRegion2D& renderRegion, const Brick& b) = 0;
    virtual void RenderHQMIPPostLoop() {}

    virtual void CreateOffscreenBuffers();
    virtual bool LoadAndVerifyShader(std::string strVSFile, std::string strFSFile,
                                     const std::vector<std::string>& strDirs,
                                     GLSLProgram** pShaderProgram);
    virtual bool LoadAndVerifyShader(std::string strVSFile, std::string strFSFile,
                                     GLSLProgram** pShaderProgram,
                                     bool bSearchSubdirs);

    void BBoxPreRender();
    void BBoxPostRender();

    void PlaneIn3DPreRender();
    void PlaneIn3DPostRender();
    void RenderPlanesIn3D(bool bDepthPassOnly);

    virtual void ClearDepthBuffer();
    virtual void ClearColorBuffer();

    virtual void StartFrame();
    virtual void EndFrame(const std::vector<char>& justCompletedRegions);
    void CopyOverCompletedRegion(const RenderRegion* region);

    void PreSubframe(RenderRegion& renderRegion);
    void PostSubframe(RenderRegion& renderRegion);

    virtual void  CVFocusHasChanged(RenderRegion& renderRegion);

    void FullscreenQuad();
    void FullscreenQuadRegions();
    void FullscreenQuadRegion(const RenderRegion* region, bool decreaseScreenRes);
    void ComputeViewAndProjection(float fAspect);
    virtual void UpdateColorsInShaders();

    GLTexture3D*    m_p3DVolTex;
    virtual bool BindVolumeTex(const BrickKey& bkey,
                               const UINT64 iIntraFrameCounter);
    virtual bool UnbindVolumeTex();

  private:
    GLSLProgram*    m_pProgramTrans;
    GLSLProgram*    m_pProgram1DTransSlice;
    GLSLProgram*    m_pProgram2DTransSlice;
    GLSLProgram*    m_pProgram1DTransSlice3D;
    GLSLProgram*    m_pProgram2DTransSlice3D;
    GLSLProgram*    m_pProgramMIPSlice;
    GLSLProgram*    m_pProgramTransMIP;
    GLSLProgram*    m_pProgramIsoCompose;
    GLSLProgram*    m_pProgramColorCompose;
    GLSLProgram*    m_pProgramCVCompose;
    GLSLProgram*    m_pProgramComposeAnaglyphs;
    GLSLProgram*    m_pProgramComposeScanlineStereo;

    float*          m_aDepthStorage;

    void SetBrickDepShaderVarsSlice(const UINTVECTOR3& vVoxelCount);
    void RenderCoordArrows(const RenderRegion& renderRegion);
    void SaveEmptyDepthBuffer();
    void SaveDepthBuffer();
    void CreateDepthStorage();
    void DeleteDepthStorage() {delete [] m_aDepthStorage;}

    void TargetIsBlankButFrameIsNotFinished(const RenderRegion* region);

};

};
#endif // GLRenderer_H
