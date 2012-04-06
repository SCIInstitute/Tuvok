#include "StdTuvokDefines.h"
#include "GLHashTable.h"
#include "Controller/Controller.h"
#include "Renderer/AbstrRenderer.h"
#include "Renderer/GL/GLSLProgram.h"
#include "Renderer/GL/GLImageTexture2D.h"
#include "Basics/Vectors.h"

namespace tuvok {

struct hinfo {
  hinfo(UINTVECTOR2 s,
        std::tr1::shared_ptr<GLImageTexture2D> tbl,
        std::tr1::shared_ptr<GLSLProgram> g,
        std::tr1::shared_ptr<AbstrRenderer> r) :
    size(s), table(tbl), glsl(g), renderer(r) { }
  UINTVECTOR2 size;
  std::tr1::shared_ptr<GLImageTexture2D> table;
  std::tr1::shared_ptr<GLSLProgram> glsl;
  std::tr1::shared_ptr<AbstrRenderer> renderer;
};

GLHashTable::GLHashTable(const UINTVECTOR2& texSize,
                         std::tr1::shared_ptr<AbstrRenderer>& ren) :
  hi(new struct hinfo(texSize, std::tr1::shared_ptr<GLImageTexture2D>(),
                      std::tr1::shared_ptr<GLSLProgram>(), ren))
{
  this->hi->table = std::tr1::shared_ptr<GLImageTexture2D>(
    new GLImageTexture2D(texSize.x, texSize.y, GL_RGBA, GL_RGBA16UI_EXT, 8,
                         NULL)
  );
}

void GLHashTable::Activate(uint32_t imgUnit, uint32_t texUnit) {
  this->hi->table->Bind(imgUnit, texUnit);
  this->hi->table->Clear();
}

std::vector<UINTVECTOR4> GLHashTable::GetList() {
  std::vector<UINTVECTOR4> rv;

  std::tr1::shared_ptr<const void> data = hi->table->GetData();
  // might need to use 32bit..
  const uint16_t* ht = static_cast<const uint16_t*>(data.get());

  for(size_t i=0; i < this->hi->size.area(); i+=4) {
    // Our texture is a bunch of 0's, and we need some way to indicate
    // that a given location has a brick there. we use the last element
    // (LOD) to indicate if the brick is 'set'.  Therefore the LOD is
    // offset by 1, so when constructing the brick ID we need to make
    // sure to fix that offset.
    if(ht[i+3]) {
      UINTVECTOR4 brick(ht[i+0], ht[i+1], ht[i+2], ht[i+3]-1);
#if _DEBUG
      MESSAGE("adding brick %llu %llu %llu %llu",
              brick.x, brick.y, brick.z, brick.w);
#endif
      rv.push_back(brick);
    }
  }
  return rv;
}

uint64_t GLHashTable::GetCPUSize() const { return 0; /* fixme */ }
uint64_t GLHashTable::GetGPUSize() const { return 0; /* fixme */ }

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
