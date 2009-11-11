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
  \file    GLSBVR2D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef GLSBVR2D_H
#define GLSBVR2D_H

#include "../../StdTuvokDefines.h"
#include "GLRenderer.h"
#include "../SBVRGeogen2D.h"

/** \class GLSBVR2D
 * Slice-based GPU volume renderer.
 *
 * GLSBVR2D is a slice based volume renderer which uses GLSL. */
class GLSBVR2D : public GLRenderer {
  public:
    /** Constructs a VRer with immediate redraw, and
     * wireframe mode off.
     * \param pMasterController message routing object */
    GLSBVR2D(MasterController* pMasterController, bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits, bool bDisableBorder);
    virtual ~GLSBVR2D();

    /** Loads GLSL vertex and fragment shaders. */
    virtual bool Initialize();

    virtual void SetDataDepShaderVars();

    /** Sends a message to the master to ask for a dataset to be loaded.
     * The dataset is converted to UVF if it is not one already.
     * @param strFilename path to a file */
    virtual bool LoadDataset(const std::string& strFilename);

    virtual bool SupportsClearView() {return !m_bAvoidSeperateCompositing && m_pDataset->GetComponentCount() == 1;}

    virtual void EnableClipPlane();
    virtual void DisableClipPlane();

    virtual ERendererType GetRendererType() {return RT_SBVR;}

  protected:
    SBVRGeogen2D  m_SBVRGeogen;
    GLSLProgram*  m_pProgramIsoNoCompose;
    GLSLProgram*  m_pProgramColorNoCompose;

    void SetBrickDepShaderVars(const Brick& currentBrick);

    virtual void Render3DPreLoop();
    virtual void Render3DInLoop(size_t iCurrentBrick, int iStereoID);
    virtual void Render3DPostLoop();

    virtual void RenderHQMIPPreLoop(const RenderRegion &region);
    virtual void RenderHQMIPInLoop(const Brick& b);
    virtual void RenderHQMIPPostLoop();

    void RenderProxyGeometry();
    virtual void Cleanup();

    virtual void ComposeSurfaceImage(int iStereoID);
    virtual void UpdateColorsInShaders();

    //virtual bool BindVolumeTex(const tuvok::BrickKey& bkey, const UINT64 iIntraFrameCounter);
    //virtual bool UnbindVolumeTex();
};
#endif // GLSBVR2D_H
