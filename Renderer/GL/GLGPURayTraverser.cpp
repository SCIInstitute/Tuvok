#include "GLInclude.h"
#include "GLGPURayTraverser.h"

#include "GLVBO.h"

using namespace tuvok;

GLGPURayTraverser::GLGPURayTraverser(MasterController* pMasterController, 
                         bool bUseOnlyPowerOfTwo, 
                         bool bDownSampleTo8Bits, 
                         bool bDisableBorder) :
  GLRenderer(pMasterController,
             bUseOnlyPowerOfTwo, 
             bDownSampleTo8Bits, 
             bDisableBorder),
  m_pNearPlaneQuad(NULL),
  m_pBBoxVBO(NULL)
{
}

void GLGPURayTraverser::Cleanup() {
  GLRenderer::Cleanup();

  delete m_pBBoxVBO;
  m_pBBoxVBO = NULL;
  delete m_pNearPlaneQuad;
  m_pNearPlaneQuad = NULL;
}

bool GLGPURayTraverser::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  // init near plane vbo
  m_pNearPlaneQuad = new GLVBO();
  std::vector<FLOATVECTOR3> posData;
  posData.push_back(FLOATVECTOR3(-1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f, -1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3(-1.0f, -1.0f, -0.5f));
  m_pNearPlaneQuad->AddVertexData(posData);

  m_pBBoxVBO = new GLVBO();

  return true;
}

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
