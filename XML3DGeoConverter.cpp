/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Interactive Visualization and Data Analysis Group.

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

//!    File   : XML3DGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "XML3DGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;

XML3DGeoConverter::XML3DGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = "XML3D File";
  m_vSupportedExt.push_back("xml");
  m_vSupportedExt.push_back("xhtml");
}

bool XML3DGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::string& strTargetFilename) {

  bool bHasTexCoords = m.GetTexCoordIndices().size() == m.GetVertexIndices().size();
  bool bHasNormals = m.GetNormalIndices().size() == m.GetVertexIndices().size();
  bool bHasColors = m.GetColorIndices().size() == m.GetVertexIndices().size();

  std::ofstream outStream(strTargetFilename.c_str());
  if (outStream.fail()) return false;

  outStream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << std::endl;
  outStream << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">" << std::endl;
  outStream << "<html xmlns=\"http://www.w3.org/1999/xhtml\"> " << std::endl;
  outStream << "  <head>" << std::endl; 
  outStream << "    <title>Mesh exported by ImageVis3D</title>" << std::endl;
  outStream << "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />" << std::endl;
  outStream << "    <link rel=\"stylesheet\" type=\"text/css\" media=\"all\" href=\"http://www.xml3d.org/xml3d/script/xml3d.css\"/>" << std::endl;
  outStream << "    <script type=\"text/javascript\" src=\"http://www.xml3d.org/xml3d/script/xml3d.js\"></script>" << std::endl;
  outStream << "  </head>" << std::endl;
  outStream << "  <body>" << std::endl;
  outStream << "    <xml3d id=\"scene\" xmlns=\"http://www.xml3d.org/2009/xml3d\" style=\"width: 600px; height: 400px; background-color: black;\" activeView=\"#defaultView\">" << std::endl;
  outStream << "      <defs>" << std::endl;
  outStream << "        <transform id=\"meshTransform\" translation=\"0 0 0\" />" << std::endl;
  outStream << "        <shader id=\"meshShader\" script=\"urn:xml3d:shader:phong\">" << std::endl;
  outStream << "          <float3 name=\"diffuseColor\">0.4 0.4 0.4</float3>" << std::endl;
  outStream << "          <float name=\"ambientIntensity\">0.4</float>" << std::endl;
  outStream << "        </shader>" << std::endl;
  outStream << "        <transform id=\"lightTransform1\" translation=\"0 0  2\" />" << std::endl;
  outStream << "        <transform id=\"lightTransform2\" translation=\"0 0 -2\" />" << std::endl;
  outStream << "        <lightshader id=\"lightShader\" script=\"urn:xml3d:lightshader:point\">" << std::endl;
  outStream << "          <float3 name=\"intensity\">1.0 1.0 1.0</float3>" << std::endl;
  outStream << "          <float3 name=\"attenuation\">1.0 0.01 0.0</float3>" << std::endl;
  outStream << "        </lightshader>" << std::endl;
  outStream << "        <data id=\"mesh\">" << std::endl;
  outStream << "          <int name=\"index\">";
  
  size_t iVPP = m.GetVerticesPerPoly();
  for (size_t i = 0;i<m.GetVertexIndices().size();i+=iVPP) {

      if (iVPP < 1) continue; // WTF?
      if (iVPP == 1) continue; // no point support in XML3D yet
      if (iVPP == 2) continue; // no consistent line support in XML3D yet
      if (iVPP > 3) continue; // TODO: triangulate

      for (size_t j = 0;j<iVPP;j++) {
        outStream << m.GetVertexIndices()[i+j] << " ";
      }
  }

  outStream << "</int>" << std::endl;
  outStream << "          <float3 name=\"position\">";
  for (size_t i = 0;i<m.GetVertices().size();i++) {
      outStream << m.GetVertices()[i].x << " " << m.GetVertices()[i].y << " " << m.GetVertices()[i].z << " ";
  }
  outStream << "</float3>" << std::endl;
  if (bHasNormals) {
    outStream << "          <float3 name=\"normal\">";
    for (size_t i = 0;i<m.GetNormals().size();i++) {
      outStream << m.GetNormals()[i].x << " " << m.GetNormals()[i].y << " " << m.GetNormals()[i].z << " ";
    }
    outStream << "</float3>" << std::endl;
  }
  if (bHasTexCoords) {
    outStream << "          <float2 name=\"textcoord\">";
    for (size_t i = 0;i<m.GetTexCoords().size();i++) {
      outStream << m.GetTexCoords()[i].x << " " << m.GetTexCoords()[i].y << " ";
    }
    outStream << "</float2>" << std::endl;
  }
  if (bHasColors) {
    outStream << "          <float4 name=\"color\">";
    for (size_t i = 0;i<m.GetColors().size();i++) {
      outStream << m.GetColors()[i].x << " " << m.GetColors()[i].y << " " << m.GetColors()[i].z << " " << m.GetColors()[i].w << " ";
    }
    outStream << "</float4>" << std::endl;
  }
  outStream << "        </data>" << std::endl;
  outStream << "      </defs>" << std::endl;
  outStream << "      <view id=\"defaultView\" position=\"0 0 2\" orientation=\"0 1 0 0\" />" << std::endl;
  outStream << "      <group transform=\"#lightTransform1\">" << std::endl;
  outStream << "        <light shader=\"#lightShader\" />" << std::endl;
  outStream << "      </group>" << std::endl;
  outStream << "      <group transform=\"#lightTransform2\">" << std::endl;
  outStream << "        <light shader=\"#lightShader\" />" << std::endl;
  outStream << "      </group>" << std::endl;
  outStream << "      <group transform=\"#meshTransform\" style=\"shader:url(#meshShader)\">" << std::endl;
  outStream << "        <mesh src=\"#mesh\" type=\"triangles\" />" << std::endl;
  outStream << "      </group>" << std::endl;
  outStream << "    </xml3d>" << std::endl;
  outStream << "  </body>" << std::endl;
  outStream << "</html>" << std::endl;
  outStream.close();

  return true;
}
