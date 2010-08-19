/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
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
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Tries to load all of Tuvok's shaders, to make sure they compile.
*/
#include "StdTuvokDefines.h"
#include <cstdlib>
#include <iostream>

#include <GL/glew.h>
#include <tclap/CmdLine.h>

#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "IO/IOManager.h"
#include "Renderer/AbstrRenderer.h"
#include "Renderer/GL/GLFBOTex.h"
#include "Renderer/GL/GLFrameCapture.h"
#include "Renderer/GL/GLRenderer.h"

#include "context.h"

using namespace tuvok;

int main(int argc, char *argv[])
{
  std::string filename;
  try {
    TCLAP::CmdLine cmd("shader test program");
    TCLAP::ValueArg<std::string> dset("d", "dataset", "Dataset to render.",
                                      true, "", "filename");
    cmd.add(dset);
    cmd.parse(argc, argv);

    filename = dset.getValue();
  } catch(const TCLAP::ArgException& e) {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << "\n";
    return EXIT_FAILURE;
  }

  try {
    std::auto_ptr<TvkContext> ctx(TvkContext::Create());

    GLenum err = glewInit();
    if(err != GLEW_OK) {
      T_ERROR("Error initializing GLEW: %s", glewGetErrorString(err));
      return EXIT_FAILURE;
    }
    Controller::Instance().DebugOut()->SetOutput(true,true,false,true);

    // Convert the data into a UVF.
    std::string uvf_file;
    uvf_file = SysTools::RemoveExt(filename) + ".uvf";
    const std::string tmpdir = "/tmp/";
    const bool quantize8 = false;
    Controller::Instance().IOMan()->ConvertDataset(
      filename, uvf_file, tmpdir, true, 256, 4, quantize8
    );

    AbstrRenderer* ren;

    for(int i=0; i < MasterController::RENDERER_LAST; ++i) {
      ren = Controller::Instance().RequestNewVolumeRenderer(
              static_cast<MasterController::EVolumeRendererType>(i),
              false, false, false, false, false
            );
      if(ren != NULL) {
        ren->LoadDataset(uvf_file);
        ren->AddShaderPath("../../Shaders");
        ren->Resize(UINTVECTOR2(100,100));
        ren->Initialize();
        ren->Cleanup();
        Controller::Instance().ReleaseVolumeRenderer(ren);
      }

      // again w/ Raycaster clip planes disabled.
      ren = Controller::Instance().RequestNewVolumeRenderer(
              static_cast<MasterController::EVolumeRendererType>(i),
              false, false, false, true, false
            );
      if(ren != NULL) {
        ren->LoadDataset(uvf_file);
        ren->AddShaderPath("../../Shaders");
        ren->Resize(UINTVECTOR2(100,100));
        ren->Initialize();
        ren->Cleanup();
        Controller::Instance().ReleaseVolumeRenderer(ren);
      }
    }
  } catch(const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
