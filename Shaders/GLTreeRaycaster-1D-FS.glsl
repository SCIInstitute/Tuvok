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
uniform float fTransScale;

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

// data from vertex stage
in vec3 vPosInViewCoords;

// outputs into render targets
layout(location=0) out vec4 accRayColor;
layout(location=1) out vec4 rayResumeColor;
layout(location=2) out vec4 rayResumePos;

// go
void main()
{
  // read ray parameters and start color
  ivec2 screenpos = ivec2(gl_FragCoord.xy);
  accRayColor     = texelFetch(rayStartColor, screenpos,0);
    
  // fetch ray entry from texture and get ray exit point from vs-shader
  rayResumePos = texelFetch(rayStartPoint, screenpos,0);

  if (rayResumePos.w > 0) {
    rayResumeColor = accRayColor;
    return;
  }

  const vec4 exitParam = vec4((mEyeToModel * vec4(vPosInViewCoords.x, vPosInViewCoords.y, vPosInViewCoords.z, 1.0)).xyz, vPosInViewCoords.z);

  // for clarity, separate postions from depth (stored in w)
  vec3 normEntryPos = rayResumePos.xyz;
  const float entryDepth = rayResumePos.w;
  const vec3 normExitCoords = exitParam.xyz;
  const float exitDepth = exitParam.w;

  // compute ray direction
  vec3 direction = normExitCoords-normEntryPos;
  float rayLength = length(direction);
  vec3 voxelSpaceDirection = TransformToPoolSpace(direction, sampleRateModifier);
  float stepSize = length(voxelSpaceDirection);

  /*
  float depth = 0;
  vec3 cp = normEntryPos; 
  //accRayColor = vec4(normExitCoords, 1.0);
  //accRayColor = rayResumeColor = vec4(1.0, 0.0, 0.0, 0.5);

  while (length(cp-normEntryPos) < rayLength) {
    float data = samplePool(cp);
    vec4 color = texture(transferFunction, data);
    accRayColor = UnderCompositing(color, accRayColor);
    cp += voxelSpaceDirection;
  }

  //accRayColor = vec4(rayLength, 0.0,0.0, 1.0);
  
  return;
  */
  

  // iterate over the bricks along the ray
  rayResumeColor = accRayColor;
  float t = 0;
  bool bOptimalResolution = true;
  vec3 currentPos = normEntryPos;  

  float voxelSize = .25/8192.; // TODO make this a quater of one voxel (or smaller)
    // fetch brick coords at the current position with the 
    // correct resolution. if that brick is not present the
    // call returns false and the coords to a lower resolution brick
    vec3 poolEntryCoords;
    vec3 poolExitCoords;
    vec3 normBrickExitCoords;
    bool bEmpty;

  if (rayLength > voxelSize) {
    uvec4 lastBrick=uvec4(0,0,0,1000), currentBrick=uvec4(0,0,0,0);
    for(uint j = 0;j<100;++j) { // j is just for savety, stop traversal after 100 bricks
      // compute the current LoD
      float currentDepth = mix(entryDepth, exitDepth, t);
      uint iLOD = ComputeLOD(currentDepth);

      bool bRequestOK = GetBrick(currentPos+voxelSize*direction/rayLength,
                                 iLOD, voxelSpaceDirection,
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
        int iSteps = int(length(poolExitCoords-poolEntryCoords)/stepSize);

        // compute opacity correction factor
        float ocFactor = pow(2,iLOD) / sampleRateModifier;

        // now raycast through this brick
        vec3 currentPoolCoords = poolEntryCoords;
        for (int i=0;i<iSteps;++i) {
          // fetch volume
          float data = samplePool(currentPoolCoords);

          // apply TF
          vec4 color = texture(transferFunction, data*fTransScale);

          // apply oppacity correction
          OpacityCorrectColor(ocFactor, color);

          // compositing
          accRayColor = UnderCompositing(color, accRayColor);

          // early ray termination
          if (accRayColor.a > 0.99) {
            if (bOptimalResolution) {
              rayResumePos.w = 1;
              rayResumeColor = accRayColor;
            }
            return;
          }

          // advance ray
          currentPoolCoords += voxelSpaceDirection;
        }
      }

     // global position along the ray
      t = length(normEntryPos-normBrickExitCoords)/rayLength;

//      if (t > 0.99 || lastBrick == currentBrick)
//        accRayColor = vec4(1.0, 0.0, 0.0, 1.0);
//      else
//        accRayColor = vec4(0.0, 0.0, 0.0, 1.0);
//      return;      
     

      if (t > 0.99 || lastBrick == currentBrick) {
        if (bOptimalResolution) {
          rayResumePos.w = 1;
          rayResumeColor = accRayColor;
        }
        return;
      }
      currentPos = normBrickExitCoords;
      lastBrick = currentBrick;
    }
  }
  

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