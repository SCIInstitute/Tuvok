/// Simple program to generate documention for the Lua functions registered in
/// Tuvok.  Basically just grabs a list of all those functions and outputs it
/// in asciidoc(1) format.  Usage:
///
///   ./a.out -o output.adoc
///
/// and then run a2x on the generated 'output.adoc' file.
///
/// This also generates individual .adoc files for *every* registered Lua
/// command.  These can be run with the 'manpage' document type to generate man
/// pages for the functions.
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>
#include <tclap/CmdLine.h>
#include "Controller/Controller.h"
#include "LuaScripting/LuaScripting.h"

using namespace tuvok;

std::string trim(std::string s) {
  const std::string ws(" \n\t");

  size_t end = s.find_last_not_of(ws);
  size_t begin = s.find_first_not_of(ws);
  return s.substr(begin, end-begin+1);
}

std::vector<std::string> split(const std::string& s, const char delim) {
  std::istringstream ss(s);
  std::vector<std::string> rv;
  while(!ss.eof()) {
    std::string elem;
    std::getline(ss, elem, delim);
    rv.push_back(trim(elem));
  }
  return rv;
}

void makeman(std::ostream& man, const LuaScripting::FunctionDesc& f) {
  man << "Tuvok API(1)\n"
         "============\n"
         ":doctype: manpage\n\n"
         "NAME\n"
         "----\n"
      << f.funcName << " - " << f.funcDesc << "\n\n"   
      << "SYNOPSIS\n"
      << "--------\n"
      << "*" << f.funcFQName << "*('";
  std::vector<std::string> args = split(f.paramSig, ',');
  std::copy(args.begin(), args.end()-1,
            std::ostream_iterator<std::string>(man, "', '"));
  man << args[args.size()-1] << "')\n\n"
      << "DESCRIPTION\n"
      << "-----------\n"
      << "The '" << f.funcName << "' function ...\n";
}

void makemanbook(std::ostream& man, const LuaScripting::FunctionDesc& f) {
  man << f.funcFQName << "\n";
  for(size_t i=0; i < f.funcFQName.length(); ++i) { man << "-"; }
  man << "\n";

  man << "[float]\n"
      << "NAME\n"
      << "~~~~\n"
      << f.funcName << " - " << f.funcDesc << "\n\n"   
      << "[float]\n"
      << "SYNOPSIS\n"
      << "~~~~~~~~\n"
      << "*" << f.funcFQName << "*('";
  std::vector<std::string> args = split(f.paramSig, ',');
  std::copy(args.begin(), args.end()-1,
            std::ostream_iterator<std::string>(man, "', '"));
  man << args[args.size()-1] << "')\n\n"
      << "[float]\n"
      << "DESCRIPTION\n"
      << "~~~~~~~~~~~\n"
      << "The '" << f.funcName << "' function ...\n";
}


void gendoc(std::ostream& ofs) {
  const std::shared_ptr<LuaScripting> ss = Controller::Instance().LuaScript();
  std::vector<LuaScripting::FunctionDesc> fd = ss->getAllFuncDescs();

  ofs << "// a2x: --dblatex-opts \"-P latex.output.revhistory=0\"\n"
      << "The Tuvok Blue Book\n"
      << "===================\n"
      << "The ImageVis3D Development Team\n\n";

  std::sort(fd.begin(), fd.end(),
            [](const LuaScripting::FunctionDesc& f1,
               const LuaScripting::FunctionDesc& f2) {
              return strcasecmp(f1.funcFQName.c_str(),
                                f2.funcFQName.c_str()) < 0;
            });

  for(auto f = fd.begin(); f != fd.end(); ++f) {
    std::ofstream manpage((f->funcName + ".adoc").c_str(), std::ios::trunc);
    makeman(manpage, *f);
    manpage.close();

    makemanbook(ofs, *f);
    ofs << "\n\n<<<<<<<\n\n"; // generates a hard page break.
  }
}

int main(int argc, char* argv[])
{
  std::string fn;
  try {
    TCLAP::CmdLine cmd("lua 'blue book' generator");
    TCLAP::ValueArg<std::string> output("o", "output", "output file",
                                        true, "", "filename");
    cmd.add(output);
    cmd.parse(argc, argv);
    fn = output.getValue();
  } catch(const TCLAP::ArgException& e) {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << "\n";
    return EXIT_FAILURE;
  }
  std::ofstream ofile(fn, std::ios::trunc);

  gendoc(ofile); // do the work.
  return EXIT_SUCCESS;
}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 Scientific Computing and Imaging Institute,
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
