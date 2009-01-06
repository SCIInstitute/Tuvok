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


//!    File   : Scripting.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : January 2009
//
//!    Copyright (C) 2008 SCI Institute

#include "Scripting.h"
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>

#include <sstream>

using namespace std;

Scripting::Scripting(MasterController* pMasterController) : 
  m_pMasterController(pMasterController)
{
}

Scripting::~Scripting() {
  for (size_t i = 0;i<m_ScriptableList.size();i++) delete m_ScriptableList[i];
}


bool Scripting::RegisterCommand(Scriptable* source, const std::string& strCommand, const std::string& strDescription) {
  // commands may not contain whitespaces
  string buf;
  stringstream ss(strCommand); 
  vector<string> strTest;
  while (ss >> buf) strTest.push_back(buf);
  if (strTest.size() != 1) return false;

  // comannds must be unique
  for (size_t i = 0;i<m_ScriptableList.size();i++) {
    if (m_ScriptableList[i]->m_strCommand == strCommand) return false;
  }
  // comannds must not be "help"
  if ("help" == strCommand) return false;

  // ok all seems fine add the comannd to the list
  ScriptableListElement* elem = new ScriptableListElement(source, strCommand, strDescription);
  m_ScriptableList.push_back(elem);
  return true;
}


bool Scripting::ParseLine(const string& strLine) {
  // tokenize string
  string buf; 
  stringstream ss(strLine); 
  vector<string> strParameters;
  while (ss >> buf) strParameters.push_back(buf);

  bool bResult = ParseCommand(strParameters);
  
  if (!bResult)
    m_pMasterController->DebugOut()->printf("Input \"%s\" not understood, try \"help\"!", strLine.c_str());
//  else m_pMasterController->DebugOut()->printf("OK (%s)", strLine.c_str());

  return bResult;
}

bool Scripting::ParseCommand(const vector<string>& strTokenized) {

  if (strTokenized.size() == 0) return false;
  string strCommand = strTokenized[0];
  vector<string> strParams(strTokenized.begin()+1, strTokenized.end());

  if (strCommand == "help") {
    m_pMasterController->DebugOut()->printf("Command Listing:");
    m_pMasterController->DebugOut()->printf("\"help\" : this help screen");
    for (size_t i = 0;i<m_ScriptableList.size();i++) {
      m_pMasterController->DebugOut()->printf("\"%s\" : %s", m_ScriptableList[i]->m_strCommand.c_str(), m_ScriptableList[i]->m_strDescription.c_str());
    }
    return true;
  }


  for (size_t i = 0;i<m_ScriptableList.size();i++) {
    if (m_ScriptableList[i]->m_strCommand == strCommand)
      return m_ScriptableList[i]->m_source->Execute(strCommand, strParams);      
  }

  return false;
}

bool Scripting::ParseFile(const std::string& strFilename) {
  string line;
  ifstream fileData(strFilename.c_str());  

  UINT32 iLine=0;
  if (fileData.is_open())
  {
    while (! fileData.eof() )
    {
      getline (fileData,line);
      iLine++;
      SysTools::RemoveLeadingWhitespace(line);
      if (line.size() == 0) continue;    // skip empty lines
      if (line[0] == '#') continue;       // skip comments

      if (!ParseLine(line)) {
        m_pMasterController->DebugOut()->Error("Scripting::ParseFile:","Error reading line %i in file %s (%s)", iLine, strFilename.c_str(), line.c_str());
        fileData.close();
        return false;
      }
    }
  } else {
    m_pMasterController->DebugOut()->Error("Scripting::ParseFile:","Error opening script file %s", strFilename.c_str());
    return false;
  }

  fileData.close();
  return true;
}