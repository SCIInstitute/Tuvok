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

//!    File   : MasterController.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : September 2008
//
//!    Copyright (C) 2008 SCI Institute

#include <sstream>

#include "MasterController.h"
#include "../Basics/SystemInfo.h"
#include "../DebugOut/TextfileOut.h"
#include "../IO/IOManager.h"
#include "../IO/TransferFunction1D.h"
#include "../Renderer/GPUMemMan/GPUMemMan.h"
#include "../Renderer/GPUMemMan/GPUMemManDataStructs.h"
#include "../Renderer/GL/GLRaycaster.h"
#include "../Renderer/GL/GLTreeRaycaster.h"
#include "../Renderer/GL/GLSBVR.h"
#include "../Renderer/GL/GLSBVR2D.h"

#include "../Scripting/Scripting.h"
#include "../LUAScripting/LUAScripting.h"

using namespace tuvok;

MasterController::MasterController() :
  m_bDeleteDebugOutOnExit(false),
  m_pProvenance(NULL),
  m_bExperimentalFeatures(false),
  m_pLuaScript(new LuaScripting()),
  m_pActiveRenderer(NULL)
{
  m_pSystemInfo   = new SystemInfo();
  m_pIOManager    = new IOManager();
  m_pGPUMemMan    = new GPUMemMan(this);
  m_pScriptEngine = new Scripting();
  RegisterCalls(m_pScriptEngine);
}


MasterController::~MasterController() {
  Cleanup();
  m_DebugOut.clear();
}

void MasterController::Cleanup() {
  for (AbstrRendererListIter i = m_vVolumeRenderer.begin();
       i < m_vVolumeRenderer.end(); ++i) {
    delete (*i);
  }
  m_vVolumeRenderer.clear();
  delete m_pSystemInfo;
  m_pSystemInfo = NULL;
  delete m_pIOManager;
  m_pIOManager = NULL;
  delete m_pGPUMemMan;
  m_pGPUMemMan = NULL;
  delete m_pScriptEngine;
  m_pScriptEngine = NULL;
  m_pActiveRenderer = NULL;
}


void MasterController::AddDebugOut(AbstrDebugOut* debugOut) {
  if (debugOut != NULL) {
    m_DebugOut.Other(_func_, "Disconnecting from this debug out");

    m_DebugOut.AddDebugOut(debugOut);

    debugOut->Other(_func_, "Connected to this debug out");
  } else {
    m_DebugOut.Warning(_func_,
                       "New debug is a NULL pointer, ignoring it.");
  }
}


void MasterController::RemoveDebugOut(AbstrDebugOut* debugOut) {
  m_DebugOut.RemoveDebugOut(debugOut);
}

/// Access the currently-active debug stream.
AbstrDebugOut* MasterController::DebugOut()
{
  return (m_DebugOut.empty()) ? static_cast<AbstrDebugOut*>(&m_DefaultOut)
                                  : static_cast<AbstrDebugOut*>(&m_DebugOut);
}
const AbstrDebugOut *MasterController::DebugOut() const {
  return (m_DebugOut.empty())
           ? static_cast<const AbstrDebugOut*>(&m_DefaultOut)
           : static_cast<const AbstrDebugOut*>(&m_DebugOut);
}

AbstrRenderer*
MasterController::RequestNewVolumeRenderer(
    EVolumeRendererType eRendererType, 
    bool bUseOnlyPowerOfTwo,
    bool bDownSampleTo8Bits, 
    bool bDisableBorder,
    bool bNoRCClipplanes, 
    bool bBiasAndScaleTF)
{
  std::string api;
  std::string method;
  AbstrRenderer *retval;

  switch (eRendererType) {
  case OPENGL_SBVR :
    api = "OpenGL";
    method = "Slice Based Volume Renderer";
    retval = new GLSBVR(this, 
                        bUseOnlyPowerOfTwo, 
                        bDownSampleTo8Bits,
                        bDisableBorder);
    break;

  case OPENGL_2DSBVR :
    api = "OpenGL";
    method = "Axis Aligned 2D Slice Based Volume Renderer";
    retval = new GLSBVR2D(this, 
                          bUseOnlyPowerOfTwo, 
                          bDownSampleTo8Bits,
                          bDisableBorder);
    break;

  case OPENGL_RAYCASTER :
    api = "OpenGL";
    method = "Raycaster";
    retval = new GLRaycaster(this, 
                             bUseOnlyPowerOfTwo, 
                             bDownSampleTo8Bits,
                             bDisableBorder, 
                             bNoRCClipplanes);
    break;
  case OPENGL_TRAYCASTER :
    api = "OpenGL";
    method = "Tree Raycaster";
    retval = new GLTreeRaycaster(this, 
                             bUseOnlyPowerOfTwo, 
                             bDownSampleTo8Bits,
                             bDisableBorder, 
                             bNoRCClipplanes);
    break;

  case DIRECTX_RAYCASTER :
  case DIRECTX_2DSBVR :
  case DIRECTX_SBVR :
  case DIRECTX_TRAYCASTER:
    m_DebugOut.Error(_func_,"DirectX 10 renderer not yet implemented."
                            "Please select OpenGL as the render API "
                            "in the settings dialog.");
    return NULL;

  default :
    m_DebugOut.Error(_func_, "Unsupported Volume renderer requested");
    return NULL;
  };

  m_DebugOut.Message(_func_, "Starting up new renderer (API=%s, Method=%s)",
                     api.c_str(), method.c_str());
  if(bBiasAndScaleTF) {
    retval->SetScalingMethod(AbstrRenderer::SMETH_BIAS_AND_SCALE);
  }
  m_vVolumeRenderer.push_back(retval);
  return m_vVolumeRenderer[m_vVolumeRenderer.size()-1];
}


void MasterController::ReleaseVolumeRenderer(AbstrRenderer* pVolumeRenderer) {
  for (AbstrRendererListIter i = m_vVolumeRenderer.begin();
       i < m_vVolumeRenderer.end();
       ++i) {

    if (*i == pVolumeRenderer) {
      m_DebugOut.Message(_func_, "Deleting volume renderer");
      delete pVolumeRenderer;

      m_vVolumeRenderer.erase(i);
      return;
    }
  }

  m_DebugOut.Warning(_func_, "requested volume renderer not found");
}


void MasterController::Filter(std::string, uint32_t,
                              void*, void *, void *, void * ) {
};


void MasterController::RegisterCalls(Scripting* pScriptEngine) {
  pScriptEngine->RegisterCommand(this, "seterrorlog", "on/off", "toggle recording of errors");
  pScriptEngine->RegisterCommand(this, "setwarninglog", "on/off", "toggle recording of warnings");
  pScriptEngine->RegisterCommand(this, "setmessagelog", "on/off", "toggle recording of messages");
  pScriptEngine->RegisterCommand(this, "printerrorlog", "", "print recorded errors");
  pScriptEngine->RegisterCommand(this, "printwarninglog", "", "print recorded warnings");
  pScriptEngine->RegisterCommand(this, "printmessagelog", "", "print recorded messages");
  pScriptEngine->RegisterCommand(this, "clearerrorlog", "", "clear recorded errors");
  pScriptEngine->RegisterCommand(this, "clearwarninglog", "", "clear recorded warnings");
  pScriptEngine->RegisterCommand(this, "clearmessagelog", "", "clear recorded messages");
  pScriptEngine->RegisterCommand(this, "fileoutput", "filename","write debug output to 'filename'");
  pScriptEngine->RegisterCommand(this, "toggleoutput", "on/off on/off on/off on/off","toggle messages, warning, errors, and other output");
  pScriptEngine->RegisterCommand(this, "set_tf_1d", "",
                                 "Sets the 1D transfer from the "
                                 "(string) argument.");
}

bool MasterController::Execute(const std::string& strCommand,
                               const std::vector<std::string>& strParams,
                               std::string& strMessage) {
  strMessage = "";
  if (strCommand == "seterrorlog") {
    m_DebugOut.SetListRecordingErrors(strParams[0] == "on");
    if (m_DebugOut.GetListRecordingErrors()) {
      m_DebugOut.printf("current state: true");
    } else {
      m_DebugOut.printf("current state: false");
    }
    return true;
  }
  if (strCommand == "setwarninglog") {
    m_DebugOut.SetListRecordingWarnings(strParams[0] == "on");
    if (m_DebugOut.GetListRecordingWarnings()) m_DebugOut.printf("current state: true"); else m_DebugOut.printf("current state: false");
    return true;
  }
  if (strCommand == "setmessagelog") {
    m_DebugOut.SetListRecordingMessages(strParams[0] == "on");
    if (m_DebugOut.GetListRecordingMessages()) m_DebugOut.printf("current state: true"); else m_DebugOut.printf("current state: false");
    return true;
  }
  if (strCommand == "printerrorlog") {
    m_DebugOut.PrintErrorList();
    return true;
  }
  if (strCommand == "printwarninglog") {
    m_DebugOut.PrintWarningList();
    return true;
  }
  if (strCommand == "printmessagelog") {
    m_DebugOut.PrintMessageList();
    return true;
  }
  if (strCommand == "clearerrorlog") {
    m_DebugOut.ClearErrorList();
    return true;
  }
  if (strCommand == "clearwarninglog") {
    m_DebugOut.ClearWarningList();
    return true;
  }
  if (strCommand == "clearmessagelog") {
    m_DebugOut.ClearMessageList();
    return true;
  }
  if (strCommand == "toggleoutput") {
    m_DebugOut.SetOutput(strParams[0] == "on",
                           strParams[1] == "on",
                           strParams[2] == "on",
                           strParams[3] == "on");
    return true;
  }
  if (strCommand == "fileoutput") {
    TextfileOut* textout = new TextfileOut(strParams[0]);
    textout->SetShowErrors(m_DebugOut.ShowErrors());
    textout->SetShowWarnings(m_DebugOut.ShowWarnings());
    textout->SetShowMessages(m_DebugOut.ShowMessages());
    textout->SetShowOther(m_DebugOut.ShowOther());

    m_DebugOut.AddDebugOut(textout);

    return true;
  }
  if(strCommand == "set_tf_1d") {
    assert(strParams.size() > 0);
    for(AbstrRendererList::iterator iter = m_vVolumeRenderer.begin();
        iter != m_vVolumeRenderer.end(); ++iter) {
      TransferFunction1D *tf1d = m_vVolumeRenderer[0]->Get1DTrans();
      std::istringstream serialized_tf(strParams[0]);
      tf1d->Load(serialized_tf);
    }
    return true;
  }

  return false;
}

void MasterController::RegisterProvenanceCB(provenance_func *pfunc)
{
  this->m_pProvenance = pfunc;
}

void MasterController::Provenance(const std::string kind,
                                  const std::string cmd,
                                  const std::string args)
{
  if(this->m_pProvenance) {
    this->m_pProvenance(kind, cmd, args);
  }
}

bool MasterController::ExperimentalFeatures() const {
  return m_bExperimentalFeatures;
}

void MasterController::ExperimentalFeatures(bool b) {
  m_bExperimentalFeatures = b;
}

// removes leading spaces from a string.
static std::string ltrim(const std::string s)
{
  std::istringstream iss(s);
  std::string nosp;
  iss >> nosp;
  return nosp;
}

static std::vector<std::string> split_string(const std::string s)
{
  enum ParserState { Normal, InQuotedString };
  enum ParserState st = Normal;
  std::string current;
  std::vector<std::string> rv;

  for(std::string::const_iterator is = s.begin(); is != s.end(); ++is) {
    switch(st) {
      case Normal:
        if(*is == '\"') {
          st = InQuotedString;
        } else if(isspace(*is) && current != "") {
          rv.push_back(ltrim(current));
          current = "";
        } else {
          current += *is;
        }
        break;
      case InQuotedString:
        if(*is == '\"') {
          st = Normal;
          rv.push_back(current);
          current = "";
        } else {
          current += *is;
        }
        break;
    }
  }
  // we should not have ended inside a quoted string.
  if(st == InQuotedString) {
    throw MasterController::ParseError();
  }

  if(!current.empty()) { rv.push_back(ltrim(current)); }

  return rv;
}

void MasterController::RegisterCommand(const std::string name, command& f)
{
  TuvokCommand c;
  c.name = name;
  c.cmd = f;
  m_Commands.push_front(c);
  m_Commands.sort();
}

CommandReturn MasterController::Command(const std::string cmd)
  throw(FailedCommand)
{
  std::istringstream iss(cmd);
  std::string cname;
  iss >> cname;

  std::list<TuvokCommand>::const_iterator iter =
    std::find(m_Commands.begin(), m_Commands.end(), cname);
  if(iter == m_Commands.end()) {
    throw CommandNotFound();
  }
  const command& f = iter->cmd;
  // remove the command itself from the arguments.
  std::string args = cmd.substr(cname.length());
  return this->Command(f, args);
}

CommandReturn MasterController::Command(const command& f,
                                        const std::string args)
                                        throw(FailedCommand)
{
  std::vector<std::string> split_args = split_string(args);
  return f(m_pActiveRenderer, split_args);
}
