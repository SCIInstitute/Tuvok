#version 420 core

// get ray start points & color
layout(binding=0) uniform sampler2D rayStartPoint;
layout(binding=1) uniform sampler2D rayStartColor;
layout(binding=2) uniform sampler1D transferFunction;
// 3 is volume pool metadata (inside volume pool)
// 4 is volume pool (inside volume pool)
// 5 is hastable (inside hash table)

// get pixel coordinates
layout(pixel_center_integer) in vec4 gl_FragCoord;

// volume and view parameter
uniform float sampleRateModifier;
uniform mat4x4 mEyeToModel;

// import VolumePool functions
bool GetBrick(in vec3 normEntryCoords, in uint iLOD, in vec3 direction,
              out vec3 poolEntryCoords, out vec3 poolExitCoords,
              out vec3 normExitCoords, out bool bEmpty, out uvec4 currentBrick);
vec3 TransformToPoolSpace(in vec3 direction,
                          in float sampleRateModifier);
float samplePool(vec3 coords);
uint ComputeLOD(float dist);

// import Compositing functions
vec4 UnderCompositing(vec4 src, vec4 dst);

// helper functions -> may be better of in an external file
void OpacityCorrectColor(float opacity_correction, inout vec4 color) {
  color.a = 1.0 - pow(1.0 - color.a, opacity_correction);
}

float ComputeRayParam(vec3 d, vec3 s, vec3 p) {
    if (d.x != 0) return (p.x-s.x)/d.x;
    if (d.y != 0) return (p.y-s.y)/d.y;
    if (d.z != 0) return (p.z-s.z)/d.z;
    return 1.0;
}


// data from vertex stage
in vec3 vPosInViewCoords;

// go
void main()
{
  // read ray parameters and start color
  ivec2 screenpos      = ivec2(gl_FragCoord.xy);
  vec4 accRayColor     = texelFetch(rayStartColor, screenpos,0);
    
  // fetch ray entry from texture and get ray exit point from vs-shader
  const vec4 entryParam  = texelFetch(rayStartPoint, screenpos,0);
  const vec4 exitParam = vec4((mEyeToModel * vec4(vPosInViewCoords.x, vPosInViewCoords.y, vPosInViewCoords.z, 1.0)).xyz, vPosInViewCoords.z);

  // for clarity, separate postions from depth (stored in w)
  vec3 normEntryPos = entryParam.xyz;
  const float entryDepth = entryParam.w;
  const vec3 normExitCoords = exitParam.xyz;
  const float exitDepth = exitParam.w;

  // compute ray direction
  vec3 direction = normExitCoords-normEntryPos;
  float rayLength = length(direction);
  vec3 voxelSpaceDirection = TransformToPoolSpace(direction, sampleRateModifier);
  float stepSize = length(voxelSpaceDirection);


  // iterate over the bricks along the ray
  vec4 rayResumePos, rayResumeColor;
  float t = 0;
  int iSteps, i;
  bool bOptimalResolution = true;
  vec3 currentPos = normEntryPos;
  uvec4 lastBrick=uvec4(0,0,0,999999999), currentBrick; // TODO

  float voxelSize = 1./128.; // TODO make this one voxel

  if (rayLength > voxelSize) {
    currentPos += voxelSize * direction/rayLength;
    do {

      // compute the current LoD
      float currentDepth = mix(entryDepth, exitDepth, t);
      uint iLOD = ComputeLOD(currentDepth);

      // fetch brick coords at the current position with the 
      // correct resolution. if that brick is not present the
      // call returns false and the coords to a lower resolution brick
      vec3 poolEntryCoords;
      vec3 poolExitCoords;
      vec3 normBrickExitCoords;
      bool bEmpty;
      bool bRequestOK = GetBrick(currentPos, iLOD, voxelSpaceDirection,
                                 poolEntryCoords, poolExitCoords,
                                 normBrickExitCoords, bEmpty, currentBrick);


      if (!bRequestOK && bOptimalResolution) {
        // for the first time in this pass, we got a brick
        // at lower than requested resolution then record this 
        // position
        bOptimalResolution = false;
        rayResumePos   = vec4(currentPos, currentDepth);
        rayResumeColor = accRayColor;
      }

      if (!bEmpty) {
        // prepare the traversal
        iSteps = int(length(poolExitCoords-poolEntryCoords)/stepSize);

        // compute opacity correction factor
        float ocFactor = pow(2,iLOD) / sampleRateModifier;

        // now raycast through this brick
        vec3 currentPoolCoords = poolEntryCoords;
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
          if (accRayColor.a > 0.95)  {
            gl_FragData[0] = accRayColor;
            gl_FragData[1] = vec4(0.0);
            gl_FragData[2] = vec4(0.0);
            return;
          }

          // advance ray
          currentPoolCoords += voxelSpaceDirection;
        }
      }

//     t = ComputeRayParam(direction, normEntryPos, normBrickExitCoords);
      t = length(normEntryPos-normBrickExitCoords)/rayLength;

      
     // global position along the ray
     /* if (t > 0.99 || lastBrick == currentBrick)
        gl_FragData[0] = vec4(1.0, 0.0, 0.0, 1.0);
      else
        gl_FragData[0] = vec4(0.0, 0.0, 0.0, 1.0);
      return;
      */

      if (t > 0.99 || lastBrick == currentBrick) break;
      currentPos = normBrickExitCoords;
      lastBrick = currentBrick;
    } while (true);
  }
  // write out result, i.e. the resume position and color 
  // and the composite color

  // TODO: Change this to use OpenGL 3.0 named variables and bind them
  //       with glBindFragDataLocation(programId, 0, "accRayColor");
  //       in cpu code
  gl_FragData[0] = accRayColor;
  gl_FragData[1] = rayResumeColor;
  gl_FragData[2] = rayResumePos;
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