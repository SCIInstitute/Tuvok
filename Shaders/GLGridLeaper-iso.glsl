#version 420 core

// get ray start points & color
layout(binding=0) uniform sampler2D rayStartPoint;
layout(binding=1) uniform sampler2D rayStartNormal;
// 2 is unused
// 3 is volume pool metadata (inside volume pool)
// 4 is volume pool (inside volume pool)
// 5 is hastable (inside hash table)
// 6 is unused
// 7 is hastable for working set tracking (inside hash table)

// get pixel coordinates
layout(pixel_center_integer) in vec4 gl_FragCoord;

// volume and view parameter
uniform float sampleRateModifier;
uniform mat4x4 mEyeToModel;
uniform mat4x4 mModelToEye;
uniform mat4x4 mModelViewIT;

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
layout(location=0) out vec4 rayHitPos;
layout(location=1) out vec4 rayHitNormal;
layout(location=2) out vec4 rayResumePos;
layout(location=3) out vec4 rayResumeNormal;

#ifdef DEBUG
layout(location=3) out vec4 debugFBO;
#endif

// layout (depth_greater) out float gl_FragDepth;

void TerminateRay(bool bOptimalResolution) {
  if (bOptimalResolution) {
    rayResumePos.w = (rayHitPos.a == 0) ? 1000.0 : 499+rayHitPos.a;
  }
  rayResumeNormal = rayHitNormal;
}

bool GetVolumeHit(vec3 currentPoolCoords, out vec4 color);
vec3 GetVolumeNormal(vec3 currentPoolCoords, vec3 sampleDelta);
vec3 RefineIsosurface(in vec3 vRayDir, in vec3 vCurrentPos);


// go
void main()
{
  // read ray parameters and start color
  ivec2 screenpos = ivec2(gl_FragCoord.xy);

  // fetch ray entry from texture and get ray exit point from vs-shader
  rayResumePos = texelFetch(rayStartPoint, screenpos,0);

  rayHitPos    = vec4(0.0);
  rayHitNormal = vec4(0.0);

  // if the ray has terminated without finding an
  // isosurface already, exit early
  if (floor(rayResumePos.w) == 1000) return;

  // if the ray has terminated already finding an 
  // isosurface, copy and exit early
  if (floor(rayResumePos.w) == 500) {
    rayHitPos = mModelToEye * vec4(rayResumePos.xyz, 1.0);
    rayHitPos.a = rayResumePos.w-floor(rayResumePos.w)+1.0;
    rayHitNormal = texelFetch(rayStartNormal, screenpos,0);
    rayResumeNormal = rayHitNormal;
    return;
  }

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
      }

      if (!bEmpty && lastBrickCoords != brickCoords) {
        // compute the number of steps it takes to

        // leave the brick
        int iSteps = int(ceil(length(poolExitCoords-poolEntryCoords)/stepSize));
        
        // or leave the entire voluem (min = whatever comes first)
        iSteps = min(iSteps, int(ceil(length((normExitCoords-currentPos)*normToPoolScale)/stepSize)));

        // now raycast through this brick
        vec3 currentPoolCoords = poolEntryCoords;
        for (int i=0;i<iSteps;++i) {

          // execute sampling function
          vec4 color;
          bool bHit = GetVolumeHit(currentPoolCoords, color);

          // ray termination
          if (bHit) {
            currentPoolCoords = RefineIsosurface(voxelSpaceDirection, currentPoolCoords);
            rayHitPos = mModelToEye * vec4(currentPos = (currentPoolCoords-normToPoolTrans)/normToPoolScale, 1);
            rayHitPos.a = color.r+1.0;
            rayHitNormal = mModelViewIT * vec4(GetVolumeNormal(currentPoolCoords, sampleDelta),0.0);
            rayHitNormal.a = floor(color.g*512.0)+color.b;
            TerminateRay(bOptimalResolution);
            return;
          } else {
            rayHitPos = vec4(0.0);
          }

          // advance ray
          currentPoolCoords += voxelSpaceDirection;
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