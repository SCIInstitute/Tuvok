#version 420 core

// get ray start points & color
layout(binding=0) uniform sampler2D rayStartPoint;
layout(binding=1) uniform sampler2D rayStartColor;
layout(binding=2) uniform sampler1D transferFunction;

// get pixel coordinates
layout(pixel_center_integer) in vec4 gl_FragCoord;

// volume and view parameter
uniform float sampleRateModifier;
uniform float LoDFactor;
uniform float levelZeroWorldSpaceError;
uniform vec3 volumeAspect;

uniform mat4x4 mEyeToModel;
in vec3 vEyePos;


// import VolumePool functions
bool GetBrick(in vec3 normEntryCoords, in uint iLOD, in vec3 direction,
              out vec3 poolEntryCoods, out vec3 poolExitCoords,
              out vec3 normExitCoods);
vec3 TransformToPoolSpace(in vec3 direction, in vec3 volumeAspect, 
                          in float sampleRateModifier);
float samplePool(vec3 coords);

// import Compositing functions
vec4 UnderCompositing(vec4 src, vec4 dst);

// helper functions -> may be better of in an external file

bool InsideDomain(vec3 pos) {
  return pos.x > 0.0 && pos.x < 1.0 && 
         pos.y > 0.0 && pos.y < 1.0 && 
         pos.z > 0.0 && pos.z < 1.0;
}
void OpacityCorrectColor(float opacity_correction, inout vec4 color) {
  color.a = 1.0 - pow(1.0 - color.a, opacity_correction);
}

uint ComputeLOD(float dist) {
  return uint(log(LoDFactor*dist/levelZeroWorldSpaceError) / log(2.0));
}

// go
void main()
{
  // read ray parameters and start color
  ivec2 screenpos = ivec2(gl_FragCoord.xy);
  vec4 accRayColor = texelFetch(rayStartColor, screenpos,0);
  vec4 entryCoords = texelFetch(rayStartPoint, screenpos,0);
  vec4 exitCoods = vec4((mEyeToModel * vec4(vEyePos.x, vEyePos.y, vEyePos.z, 1.0)).xyz, vEyePos.z);
  vec3 direction = exitCoods.xyz-entryCoords.xyz;
  float rayLength = length(direction);
  
  vec3 voxelSpaceDirection = TransformToPoolSpace(direction, volumeAspect, sampleRateModifier);
  float stepSize = length(voxelSpaceDirection);
  bool bOptimalResolution = true;
  vec4 rayResumePos, rayResumeColor;

  // iterate over the bricks along the ray
  float t = 0;
  uint iSteps, i;
  vec4 currentPos = entryCoords;
  do {
    // compute the current LoD
    uint iLOD = ComputeLOD(currentPos.w);
    vec3 poolEntryCoods;
    vec3 poolExitCoords;
    vec3 normExitCoods;
    // fetch brick coords at the current position with the 
    // correct resolution. if that brick is not present the
    // call returns false and the coords to a lower resolution brick
    bool bRequestOK = GetBrick(currentPos.xyz, iLOD, voxelSpaceDirection,
                               poolEntryCoods, poolExitCoords,
                               normExitCoods);

    // if (for the first tiem in this pass) we got a brick
    // at lower then requested resolution then record this 
    // position
    if (!bRequestOK && !bOptimalResolution) {
      bOptimalResolution = false;
      rayResumePos   = currentPos;
      rayResumeColor = accRayColor;
    }

    // prepare the traversal
    iSteps = uint(length(poolExitCoords-poolEntryCoods)/stepSize);

    // where are we on the brick once we exit the next brick
    t = length(normExitCoods-entryCoords.xyz)/rayLength;

    // found an empty brick, so proceed to the next right away
    if (iSteps == 0) continue;
    
    // compute opacity correction factor
    float ocFactor = pow(2,iLOD) / sampleRateModifier;

    // now raycast through this brick
    vec3 currentPoolCoords = poolEntryCoods;
    for (i=0;i<iSteps;++i) {
      // fetch volume
      float data = samplePool(currentPoolCoords);

      // apply TF
      vec4 color = texture(transferFunction, data);

      // apply oppacity correction
      OpacityCorrectColor(ocFactor, color);

      // compositing
      accRayColor = UnderCompositing(color, accRayColor);

      // early ray termination
      if (accRayColor.a > 0.95)  break;

      // advance ray
      currentPoolCoords += voxelSpaceDirection;
    }

    // if we have not terminated early and we also have not
    // yet left the volume, then continue
    currentPos.xyz = normExitCoods;    
    currentPos.w   = entryCoords.w + t * (exitCoods.w, entryCoords.w);
  } while (i < iSteps-1 && t < 1);

  // write out result, i.e. the resume position and color 
  // and the composite color
  gl_FragData[0] = rayResumePos;
  gl_FragData[1] = rayResumeColor;
  gl_FragData[2] = accRayColor;
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