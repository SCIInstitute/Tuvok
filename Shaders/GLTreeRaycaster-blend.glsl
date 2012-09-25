#version 420 core

// get ray start points & color
layout(binding=0) uniform sampler2D rayStartPoint;
layout(binding=1) uniform sampler2D rayStartColor;
// 2 is transfer function (inside render method shader)
// 3 is volume pool metadata (inside volume pool)
// 4 is volume pool (inside volume pool)
// 5 is hastable (inside hash table)

#ifdef DEBUG
layout(binding=6) uniform sampler2D debugColor;
// 7 is hastable for working set tracking (inside hash table)
#endif

// get pixel coordinates
layout(pixel_center_integer) in vec4 gl_FragCoord;

// volume and view parameter
uniform float sampleRateModifier;
uniform mat4x4 mEyeToModel;

// import VolumePool functions
bool GetBrick(in vec3 normEntryCoords, inout uint iLOD, in vec3 direction,
              out vec3 poolEntryCoords, out vec3 poolExitCoords,
              out vec3 normExitCoords, out bool bEmpty,
              out vec3 normToPoolScale, out vec3 normToPoolTrans, out uvec4 brickCoords);
vec3 TransformToPoolSpace(in vec3 direction,
                          in float sampleRateModifier);
vec3 GetSampleDelta();
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

#ifdef DEBUG
layout(location=3) out vec4 debugFBO;
#endif

// layout (depth_greater) out float gl_FragDepth;

void TerminateRay(bool bOptimalResolution) {
  if (bOptimalResolution) {
    rayResumePos.w = 1000;
    rayResumeColor = accRayColor;
  }
}

vec4 ComputeColorFromVolume(vec3 currentPoolCoords, vec3 modelSpacePosition, vec3 sampleDelta);

// go
void main()
{
  // read ray parameters and start color
  ivec2 screenpos = ivec2(gl_FragCoord.xy);
  accRayColor     = texelFetch(rayStartColor, screenpos,0);
  rayResumeColor  = accRayColor;

#ifdef DEBUG
  debugFBO        = texelFetch(debugColor, screenpos,0);
  debugFBO.w      = 1;
#endif
    
  // fetch ray entry from texture and get ray exit point from vs-shader
  rayResumePos = texelFetch(rayStartPoint, screenpos,0);

  // if the ray has terminated already, exit early
  if (rayResumePos.w == 1000) return;

  vec4 exitParam = vec4((mEyeToModel * vec4(vPosInViewCoords.x, vPosInViewCoords.y, vPosInViewCoords.z, 1.0)).xyz, vPosInViewCoords.z);

  // for clarity, separate postions from depth (stored in w)
  vec3 normEntryPos = rayResumePos.xyz;
  float entryDepth = rayResumePos.w;
  vec3 normExitCoords = exitParam.xyz;
  float exitDepth = exitParam.w;

  // compute ray direction
  vec3 direction = normExitCoords-normEntryPos;
  float rayLength = length(direction);
  vec3 voxelSpaceDirection = TransformToPoolSpace(direction, sampleRateModifier);
  vec3 sampleDelta = GetSampleDelta();
  float stepSize = length(voxelSpaceDirection);

  // iterate over the bricks along the ray
  float t = 0;
  bool bOptimalResolution = true;

  float voxelSize = 0.125/2000.; // TODO make this a quater of one voxel (or smaller)
  vec3 currentPos = normEntryPos;

  // fetch brick coords at the current position with the 
  // correct resolution. if that brick is not present the
  // call returns false and the coords to a lower resolution brick
  vec3 poolEntryCoords;
  vec3 poolExitCoords;
  vec3 normBrickExitCoords;
  bool bEmpty;
  uvec4 brickCoords;
  uvec4 lastBrickCoords = uvec4(0,0,0,9999);

  if (rayLength > voxelSize) {
    for(uint j = 0;j<100;++j) { // j is just for savety, stop traversal after 100 bricks
      // compute the current LoD
      float currentDepth = mix(entryDepth, exitDepth, t);

      uint iLOD = ComputeLOD(currentDepth);

      vec3 normToPoolScale;
      vec3 normToPoolTrans;
      bool bRequestOK = GetBrick(currentPos,
                                 iLOD, direction,
                                 poolEntryCoords, poolExitCoords,
                                 normBrickExitCoords, bEmpty,
                                 normToPoolScale, normToPoolTrans, brickCoords);

      if (!bRequestOK && bOptimalResolution) {
        // for the first time in this pass, we got a brick
        // at lower than requested resolution then record this 
        // position
        bOptimalResolution = false;
        rayResumePos   = vec4(currentPos, currentDepth);
        rayResumeColor = accRayColor;
      }

      if (!bEmpty && lastBrickCoords != brickCoords) {
        // compute the number of steps it takes to

        // leave the brick
        int iSteps = int(ceil(length(poolExitCoords-poolEntryCoords)/stepSize));
        
        // or leave the entire voluem (min = whatever comes first)
        iSteps = min(iSteps, int(ceil(length((normExitCoords-currentPos)*normToPoolScale)/stepSize)));

        // compute opacity correction factor
        //float ocFactor = exp2(float(iLOD)) / sampleRateModifier;
        float ocFactor = 1.0 / sampleRateModifier; // use the nicer "opacity correction" for now

        // now raycast through this brick
        vec3 currentPoolCoords = poolEntryCoords;
        for (int i=0;i<iSteps;++i) {

          // execute sampling function
          vec4 color = ComputeColorFromVolume(currentPoolCoords, (currentPoolCoords-normToPoolTrans)/normToPoolScale, sampleDelta);

#ifdef COLOR_LODS
          switch (iLOD % 3) {
            case 0:  
              color.g = color.b = 0;
              break;
            case 1:
              color.r = color.b = 0;
              break;
            default:
              color.r = color.g = 0;
              break;
          }
#endif

          // apply oppacity correction
          OpacityCorrectColor(ocFactor, color);

          // compositing
          accRayColor = UnderCompositing(color, accRayColor);

          // early ray termination
          if (accRayColor.a > 0.99) {
            TerminateRay(bOptimalResolution);
            return;
          }

          // advance ray
          currentPoolCoords += voxelSpaceDirection;

#ifdef DEBUG
          if (bOptimalResolution) {
            debugFBO.r += 0.01;
            debugFBO.w = 1;
          }
#endif
        }

        currentPos = (currentPoolCoords-normToPoolTrans)/normToPoolScale;
      } else {
        currentPos = normBrickExitCoords+voxelSize*direction/rayLength;
      }

      lastBrickCoords = brickCoords;

     // global position along the ray
      t = length(normEntryPos-normBrickExitCoords)/rayLength;

      if (t > 0.9999) {
        TerminateRay(bOptimalResolution);
        return;
      }

#ifdef DEBUG
      if (bEmpty && bOptimalResolution) {
        debugFBO.g += 0.1;
        debugFBO.w = 1;
      }
#endif

    }
  }

  TerminateRay(bOptimalResolution);
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