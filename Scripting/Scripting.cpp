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
//!    Copyright (C) 2009 SCI Institute

#include "StdTuvokDefines.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#ifndef TUVOK_NO_QT
# include <QtCore/QTime>
# include <QtCore/QDate>
#endif
#include "Scripting.h"

#include "Basics/SysTools.h"
#include "Controller/Controller.h"

using namespace std;
using namespace tuvok;

ScriptableListElement::ScriptableListElement(Scriptable* source,
                                             const std::string& strCommand,
                                             const std::string& strParameters,
                                             std::string strDescription) :
      m_source(source),
      m_strCommand(strCommand),
      m_strDescription(strDescription)
{
  m_vParameters = SysTools::Tokenize(strParameters, false);

  m_iMaxParam = uint32_t(m_vParameters.size());
  m_iMinParam = 0;

  bool bFoundOptional = false;
  for (uint32_t i = 0;i<m_vParameters.size();i++) {
    if (m_vParameters[i] == "...") {
      m_iMaxParam = numeric_limits<uint32_t>::max();
    } else {
      if (m_vParameters[i][0] == '[' && 
          m_vParameters[i][m_vParameters[i].size()-1] == ']') {
        bFoundOptional = true;
        m_vParameters[i] = string(m_vParameters[i].begin()+1,
                           m_vParameters[i].end()-1);
      } else {
        if (!bFoundOptional)
          m_iMinParam++;
        // else // this else would be an syntax error case but lets just
                // assume all parameters after the first optional parameter
                // are also optional
      }
    }
  }
}


Scripting::Scripting() :
  m_bSorted(false),
  m_bEcho(false),
  m_bDontStoreInHistory(false)
{
  RegisterCalls(this);
}

template<class T> static void Delete(T* t) { delete t; }

Scripting::~Scripting() {
  std::for_each(m_ScriptableList.begin(), m_ScriptableList.end(),
                Delete<ScriptableListElement>);
}


bool Scripting::RegisterCommand(Scriptable* source,
                                const std::string& strCommand,
                                const std::string& strParameters,
                                std::string strDescription) {
  // commands may not contain whitespaces
  vector<string> strTest = SysTools::Tokenize(strCommand, false);
  if (strTest.size() != 1) return false;

  // commands must be unique
  for(ScriptList::const_iterator iter = m_ScriptableList.begin();
      iter != m_ScriptableList.end(); ++iter) {
    if((*iter)->m_strCommand == strCommand) {
      WARNING("Command '%s' is not unique, ignoring.", strCommand.c_str());
      return false;
    }
  }

  // ok, all seems fine: add the command to the list
  ScriptableListElement* elem = new ScriptableListElement(source, strCommand,
                                                          strParameters,
                                                          strDescription);
  m_bSorted = false;
  m_ScriptableList.push_front(elem);
  return true;
}


bool Scripting::ParseLine(const string& strLine) {
  // tokenize string
  vector<string> vParameters = SysTools::Tokenize(strLine);
  if(vParameters.empty()) { return true; }

  string strMessage = "";
  bool bResult = ParseCommand(vParameters, strMessage);

  if (!bResult) {
    if (strMessage == "") {
      std::ostringstream badinput;
      badinput << "Input '" << strLine << "' not understood; try 'help'!";
      Controller::Debug::Out().printf(badinput.str().c_str());
    }
    else
      Controller::Debug::Out().printf(strMessage.c_str());
  } else {

    if (m_bDontStoreInHistory) 
      m_bDontStoreInHistory = false;
    else
      m_vHistory.push_back(strLine);

    if (m_bEcho) {
      std::ostringstream ok;
      ok << "OK (" << strLine << ")";
      Controller::Debug::Out().printf(ok.str().c_str());
    }
  }

  return bResult;
}

struct CmpByCmdName: public std::binary_function<bool, ScriptableListElement*,
                                                 ScriptableListElement*> {
  bool operator()(const ScriptableListElement* a,
                  const ScriptableListElement* b) const {
    return a->m_strCommand <= b->m_strCommand;
  }
};

bool Scripting::ParseCommand(const vector<string>& strTokenized,
                             string& strMessage) {

  if (strTokenized.empty()) return false;
  string strCommand = strTokenized[0];
  vector<string> strParams(strTokenized.begin()+1, strTokenized.end());

  if(!m_bSorted) {
    m_ScriptableList.sort(CmpByCmdName());
    m_bSorted = true;
  }

  strMessage = "";
  for(ScriptList::const_iterator cmd = m_ScriptableList.begin();
      cmd != m_ScriptableList.end(); ++cmd) {
    if((*cmd)->m_strCommand == strCommand) {
      if (strParams.size() >= (*cmd)->m_iMinParam &&
          strParams.size() <= (*cmd)->m_iMaxParam) {
        return (*cmd)->m_source->Execute(strCommand, strParams, strMessage);
      } else {
         strMessage = "Parameter mismatch for command \""+strCommand+"\"";
         return false;
      }
    }
  }

  return false;
}

bool Scripting::ParseFile(const std::string& strFilename) {
  string line;
  std::ifstream fileData(strFilename.c_str());

  uint32_t iLine=0;
  if (fileData.is_open())
  {
    while (! fileData.eof() )
    {
      getline (fileData,line);
      iLine++;
      line = SysTools::TrimStrLeft(line);
      if (line.empty()) continue;         // skip empty lines
      if (line[0] == '#') continue;       // skip comments

      if (!ParseLine(line)) {
        T_ERROR("Error executing line %i in file %s (%s)", iLine,
                strFilename.c_str(), line.c_str());
        fileData.close();
        return false;
      }
    }
  } else {
    T_ERROR("Error opening script file %s", strFilename.c_str());
    return false;
  }

  fileData.close();
  return true;
}

void Scripting::RegisterCalls(Scripting* pScriptEngine) {
  pScriptEngine->RegisterCommand(this, "help",
    "",
    "show all commands");
  pScriptEngine->RegisterCommand(this, "execute",
    "filename",
    "run the script saved as 'filename'");
  pScriptEngine->RegisterCommand(this, "l",
    "",
    "re-execute last command");
  pScriptEngine->RegisterCommand(this, "echo",
    "on/off",
    "turn feedback on successful command execution on or off");
  pScriptEngine->RegisterCommand(this, "time",
    "",
    "print out the current time");
  pScriptEngine->RegisterCommand(this, "date",
    "",
    "print out the current date");
  pScriptEngine->RegisterCommand(this, "write",
    "test",
    "print out 'text'");
  pScriptEngine->RegisterCommand(this, "clearhistory",
    "",
    "purge the command history");
  pScriptEngine->RegisterCommand(this, "printhistory",
    "",
    "display the command history");
  pScriptEngine->RegisterCommand(this, "exechistory",
    "[a] [b]",
    "execute line a to line b of the command history, "
    "if both parameters are ommited the entire history is processed");
  pScriptEngine->RegisterCommand(this, "storehistory",
    "filename [a] [b]",
    "store line a to line b of the command history to file 'filename', "
    "if both parameters are ommited the entire history is processed");
}


bool Scripting::Execute(const std::string& strCommand, 
                        const std::vector< std::string >& strParams, 
                        std::string& strMessage) {
  strMessage = "";
  if (strCommand == "echo") {
    m_bEcho = SysTools::ToLowerCase(strParams[0]) == "on";
    return true;
  } else
  if (strCommand == "execute") {
    return ParseFile(strParams[0]);
  } else
  if (strCommand == "help") {
    m_bDontStoreInHistory = true;
    Controller::Debug::Out().printf("Command Listing:");
    for(ScriptList::const_iterator cmd = m_ScriptableList.begin();
        cmd != m_ScriptableList.end(); ++cmd) {
      string strParams = "";
      uint32_t iMin = (*cmd)->m_iMinParam;
      for (size_t j = 0; j < (*cmd)->m_vParameters.size(); j++) {
        if (j < iMin) {
          if ((*cmd)->m_vParameters[j] == "...") iMin++;
          strParams = strParams + (*cmd)->m_vParameters[j];
        } else {
          strParams = strParams + "[" + (*cmd)->m_vParameters[j]+  "]";
        }
        if (j != (*cmd)->m_vParameters.size()-1) strParams = strParams + " ";
      }

      std::ostringstream echo;
      echo << "'" << (*cmd)->m_strCommand << "' " << strParams << ": "
           << (*cmd)->m_strDescription;
      Controller::Debug::Out().printf(echo.str().c_str());
    }
    return true;
  } else
  if (strCommand == "time") {
#ifndef TUVOK_NO_QT
    static QTime qt;
    if(!qt.isNull() && qt.secsTo(QTime::currentTime()) < 1) {
      Controller::Debug::Out().printf("To get a watch!");
    } else {
      qt = QTime::currentTime();
      std::string strTime(qt.toString().toAscii());
      Controller::Debug::Out().printf(strTime.c_str());
    }
#endif
    return true;
  } else
  if (strCommand == "date") {
#ifndef TUVOK_NO_QT
    string strDate(QDate::currentDate().toString().toAscii());
    Controller::Debug::Out().printf(strDate.c_str());
#endif
    return true;
  } else
  if (strCommand == "write") {
    Controller::Debug::Out().printf(strParams[0].c_str());
    return true;
  } else
  if (strCommand == "l") {
    if (m_vHistory.size() > 0) {
      bool bResult = ParseLine(m_vHistory[0]);
      m_bDontStoreInHistory = true;
      return bResult;
    } else {
      strMessage = "History is empty.";
      return false;
    }
  } else
  if (strCommand == "clearhistory") {
    m_vHistory.clear();
    m_bDontStoreInHistory = true;
    return true;
  } else
  if (strCommand == "printhistory") {
    for (size_t i = 0;i<m_vHistory.size();i++) {
      std::ostringstream msg;
      msg << setw( 3 ) << i << " : " << m_vHistory[i];
      Controller::Debug::Out().printf(msg.str().c_str());
    }
    m_bDontStoreInHistory = true;
    return true;
  } else
  if (strCommand == "exechistory") {
    size_t p0, p1;
    if (strParams.size() == 3) { 
      int i0, i1;
      SysTools::FromString(i0, strParams[0]);
      SysTools::FromString(i1, strParams[1]);
      const int int_sz = static_cast<int>(m_vHistory.size());
      p0 = (i0 < 0) ? 0 
                    : ((i0 >= int_sz) ? m_vHistory.size()-1 
                                      : static_cast<size_t>(i0));
      p1 = (i1 < 0) ? 0 
                    : ((i1 >= int_sz) ? m_vHistory.size()-1 
                                      : static_cast<size_t>(i1));
      if (p1 > p0) { p0 = p1; }
    } else {
      if (!strParams.empty()) { 
        strMessage = "Either specify both boundaries or none.";
        return false;
      }
      p0 = 0;
      p1 = m_vHistory.size()-1;
    }

    std::vector<std::string> vOldHist(m_vHistory);
    for (size_t i = p0;i<=p1;i++) {
      if (!ParseLine(vOldHist[i])) {
        m_vHistory = vOldHist;
        return false;
      }
    }

    m_vHistory = vOldHist;
    m_bDontStoreInHistory = true;
    return true;
  } else
  if (strCommand == "storehistory") {
    size_t p0, p1;
    if (strParams.size() == 3) { 
      int i0, i1;
      SysTools::FromString(i0, strParams[1]);
      SysTools::FromString(i1, strParams[2]);
      const int int_sz = static_cast<int>(m_vHistory.size());
      p0 = (i0 < 0) ? 0 
                    : ((i0 >= int_sz) ? m_vHistory.size()-1 
                                      : static_cast<size_t>(i0));
      p1 = (i1 < 0) ? 0 
                    : ((i1 >= int_sz) ? m_vHistory.size()-1 
                                      : static_cast<size_t>(i1));

      if (p1 > p0) { p0 = p1; }
    } else {
      if (strParams.size() != 1) { 
        strMessage = "Either specify both boundaries or none.";
        return false;
      }
      p0 = 0;
      p1 = m_vHistory.size()-1;
    }

    ofstream historyFile(strParams[0].c_str());
    if (!historyFile.is_open()) {
      strMessage = "Unable to create history file.";
      return false;
    }
    for (size_t i = p0;i<=p1;i++) {
      std::ostringstream msg;
      historyFile << m_vHistory[i] << endl;
    }
    historyFile.close();
    m_bDontStoreInHistory = true;

    return true;
  }

  return false;

}
