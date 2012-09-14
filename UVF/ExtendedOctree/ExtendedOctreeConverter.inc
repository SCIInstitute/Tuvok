template<class T, bool bComputeMedian>
void ExtendedOctreeConverter::DownsampleBricktoBrick(
  ExtendedOctree &tree, T* pData, const UINT64VECTOR3& targetSize, T* pSourceData,
  const UINT64VECTOR4& sourceCoords, const UINT64VECTOR3& targetOffset)
{
  uint64_t iCompCount = tree.m_iComponentCount;

  const UINT64VECTOR3& sourceSize = tree.ComputeBrickSize(sourceCoords);
  GetBrick((uint8_t*)pSourceData, tree, sourceCoords);

  const uint64_t evenSizeX = (sourceSize.x-2*m_iOverlap)/2;
  const uint64_t evenSizeY = (sourceSize.y-2*m_iOverlap)/2;
  const uint64_t evenSizeZ = (sourceSize.z-2*m_iOverlap)/2;

  // process inner even-sized area
  for (uint64_t z = 0;z<evenSizeZ;z++) {
    for (uint64_t y = 0;y<evenSizeY;y++) {
      T *p0 = pSourceData + iCompCount* (
                             (2*0+m_iOverlap)
                          +  (2*y+m_iOverlap)*sourceSize.x
                          +  (2*z+m_iOverlap)*sourceSize.x*sourceSize.y
                          );
      T *p1 = p0 + iCompCount * sourceSize.x*sourceSize.y;
      T *p2 = p0 + iCompCount * sourceSize.x;
      T *p3 = p1 + iCompCount * sourceSize.x;

      T *p4 = p0+iCompCount;
      T *p5 = p1+iCompCount;
      T *p6 = p2+iCompCount;
      T *p7 = p3+iCompCount;
      T* pTargetData = pData +
          iCompCount * (
            (0+m_iOverlap + targetOffset.x)
          + (y+m_iOverlap+targetOffset.y)*targetSize.x
          + (z+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
         );

      for (uint64_t x = 0;x<evenSizeX;x++) {
        for (uint32_t c = 0;c<iCompCount;c++) {
          T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
            *(p0+c), *(p1+c), *(p2+c), *(p3+c),
            *(p4+c), *(p5+c), *(p6+c), *(p7+c));
          *(pTargetData+c) = filtered;
        }
        p0+=2*iCompCount;
        p1+=2*iCompCount;
        p2+=2*iCompCount;
        p3+=2*iCompCount;
        p4+=2*iCompCount;
        p5+=2*iCompCount;
        p6+=2*iCompCount;
        p7+=2*iCompCount;
        pTargetData+=iCompCount;
      }
    }
  }

  // process odd boundaries (if any)

  // plane at the end of the x-axis
  if (sourceSize.x%2) {
    for (uint64_t z = 0;z<evenSizeZ;z++) {
      for (uint64_t y = 0;y<evenSizeY;y++) {
        T *p0 = pSourceData + iCompCount* (
                               (2*(evenSizeX)+m_iOverlap)
                            +  (2*y+m_iOverlap)*sourceSize.x
                            +  (2*z+m_iOverlap)*sourceSize.x*sourceSize.y
        );
        T *p1 = p0 + iCompCount * sourceSize.x*sourceSize.y;
        T *p2 = p0 + iCompCount * sourceSize.x;
        T *p3 = p1 + iCompCount * sourceSize.x;

        T* pTargetData = pData + iCompCount * (
             (evenSizeX +m_iOverlap + targetOffset.x)
           + (y+m_iOverlap+targetOffset.y)*targetSize.x
           + (z+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
        );

        for (uint32_t c = 0;c<iCompCount;c++) {
          T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
            *(p0+c), *(p1+c), *(p2+c), *(p3+c)
          );
          *(pTargetData+c) = filtered;
        }
      }
    }
  }

  // plane at the end of the y-axis
  if (sourceSize.y%2) {
    for (uint64_t z = 0;z<evenSizeZ;z++) {
      T *p0 = pSourceData + iCompCount* (
                              (2*0+m_iOverlap)
                          +  (2*(evenSizeY)+m_iOverlap)*sourceSize.x
                          +  (2*z+m_iOverlap)*sourceSize.x*sourceSize.y
      );
      T *p1 = p0 + iCompCount * sourceSize.x*sourceSize.y;

      T *p4 = p0+iCompCount;
      T *p5 = p1+iCompCount;
      T* pTargetData = pData + iCompCount * (
           (0+m_iOverlap + targetOffset.x)
         + (evenSizeY+m_iOverlap+targetOffset.y)*targetSize.x
         + (z+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
      );

      for (uint64_t x = 0;x<evenSizeX;x++) {
        for (uint32_t c = 0;c<iCompCount;c++) {
          T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
            *(p0+c), *(p1+c), *(p4+c), *(p5+c)
          );
          *(pTargetData+c) = filtered;
        }
        p0+=2*iCompCount;
        p1+=2*iCompCount;
        p4+=2*iCompCount;
        p5+=2*iCompCount;
        pTargetData+=iCompCount;
      }
    }
  }

  // plane at the end of the z-axis
  if (sourceSize.z%2) {
    for (uint64_t y = 0;y<evenSizeY;y++) {
      T *p0 = pSourceData + iCompCount* (
                              (2*0+m_iOverlap)
                          +  (2*y+m_iOverlap)*sourceSize.x
                          +  (2*(evenSizeZ)+m_iOverlap)*sourceSize.x*sourceSize.y
      );
      T *p2 = p0 + iCompCount * sourceSize.x;

      T *p4 = p0+iCompCount;
      T *p6 = p2+iCompCount;
      T* pTargetData = pData + iCompCount * (
           (0+m_iOverlap + targetOffset.x)
         +  (y+m_iOverlap+targetOffset.y)*targetSize.x
         +  (evenSizeZ+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
      );

      for (uint64_t x = 0;x<evenSizeX;x++) {
        for (uint32_t c = 0;c<iCompCount;c++) {
          T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
            *(p0+c), *(p2+c), *(p4+c), *(p6+c)
          );
          *(pTargetData+c) = filtered;
        }
        p0+=2*iCompCount;
        p2+=2*iCompCount;
        p4+=2*iCompCount;
        p6+=2*iCompCount;
        pTargetData+=iCompCount;
      }
    }
  }

  // line at the end of the x/y-axes
  if (sourceSize.x%2 && sourceSize.y%2) {
    for (uint64_t z = 0;z<evenSizeZ;z++) {
      T *p0 = pSourceData + iCompCount* (
                             (2*(evenSizeX)+m_iOverlap)
                          +  (2*(evenSizeY)+m_iOverlap)*sourceSize.x
                          +  (2*z+m_iOverlap)*sourceSize.x*sourceSize.y
                          );
      T *p1 = p0 + iCompCount * sourceSize.x*sourceSize.y;

      T* pTargetData = pData + iCompCount * (
           (evenSizeX +m_iOverlap + targetOffset.x)
         +  (evenSizeY+m_iOverlap+targetOffset.y)*targetSize.x
         +  (z+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
      );

      for (uint32_t c = 0;c<iCompCount;c++) {
        T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
          *(p0+c), *(p1+c)
        );
        *(pTargetData+c) = filtered;
      }
    }
  }

  // line at the end of the y/z-axes
  if (sourceSize.y%2 && sourceSize.z%2) {
    T *p0 = pSourceData + iCompCount* (
                            (2*0+m_iOverlap)
                        +  (2*(evenSizeY)+m_iOverlap)*sourceSize.x
                        +  (2*(evenSizeZ)+m_iOverlap)*sourceSize.x*sourceSize.y
                        );
    T *p4 = p0+iCompCount;
    T* pTargetData = pData + iCompCount * (
         (0+m_iOverlap + targetOffset.x)
       + (evenSizeY+m_iOverlap+targetOffset.y)*targetSize.x
       + (evenSizeZ+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
    );

    for (uint64_t x = 0;x<evenSizeX;x++) {
      for (uint32_t c = 0;c<iCompCount;c++) {
        T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
          *(p0+c), *(p4+c));
        *(pTargetData+c) = filtered;
      }
      p0+=2*iCompCount;
      p4+=2*iCompCount;
      pTargetData+=iCompCount;
    }
  }

  // line at the end of the x/z-axes
  if (sourceSize.x%2 && sourceSize.z%2) {
    for (uint64_t y = 0;y<evenSizeY;y++) {
      T *p0 = pSourceData + iCompCount* (
                             (2*(evenSizeX)+m_iOverlap)
                          +  (2*y+m_iOverlap)*sourceSize.x
                          +  (2*(evenSizeZ)+m_iOverlap)*sourceSize.x*sourceSize.y
                          );
      T *p2 = p0 + iCompCount * sourceSize.x;

      T* pTargetData = pData + iCompCount * (
           (evenSizeX+m_iOverlap + targetOffset.x)
         + (y+m_iOverlap+targetOffset.y)*targetSize.x
         + (evenSizeZ+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
      );
      for (uint32_t c = 0;c<iCompCount;c++) {
        T filtered = VolumeTools::Filter<T, double, bComputeMedian>(
          *(p0+c), *(p2+c)
        );
        *(pTargetData+c) = filtered;
      }
    }
  }

  // single voxel at the x/y/z corner
  if (sourceSize.x%2 && sourceSize.y%2 && sourceSize.z%2) {
    T *p0 = pSourceData + iCompCount* (
                            (2*(evenSizeX)+m_iOverlap)
                        +  (2*(evenSizeY)+m_iOverlap)*sourceSize.x
                        +  (2*(evenSizeZ)+m_iOverlap)*sourceSize.x*sourceSize.y
                        );
    T* pTargetData = pData + iCompCount * (
         (evenSizeX+m_iOverlap + targetOffset.x)
       +  (evenSizeY+m_iOverlap+targetOffset.y)*targetSize.x
       +  (evenSizeZ+m_iOverlap+targetOffset.z)*targetSize.x*targetSize.y
    );
    for (uint32_t c = 0;c<iCompCount;c++) {
      *(pTargetData+c) = *p0;
	  }
  }
}

template<class T, bool bComputeMedian>
void ExtendedOctreeConverter::DownsampleBrick(
  ExtendedOctree &tree, bool bClampToEdge, const UINT64VECTOR4& vBrickCoords,
  T* pData, T* pSourceData)
{

  const UINT64VECTOR3& vTargetBricksize = tree.ComputeBrickSize(vBrickCoords);
  const uint64_t iUncompressedBrickSize = vTargetBricksize.volume() *
                                          tree.GetComponentTypeSize() *
                                          tree.GetComponentCount();

  if (!bClampToEdge) {
    memset(pData,0,size_t(iUncompressedBrickSize));
  }

  const TOCEntry t = {
    (tree.m_vTOC.end()-1)->m_iLength + (tree.m_vTOC.end()-1)->m_iOffset,
    iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize,
    UINTVECTOR2(0,0)
  };
  tree.m_vTOC.push_back(t);

  const UINT64VECTOR4 bricksInLowerLevel = tree.GetBrickCount(vBrickCoords.w-1);

  const bool bHasBrickRight  = vBrickCoords.x*2+1 < bricksInLowerLevel.x;
  const bool bHasBrickBottom = vBrickCoords.y*2+1 < bricksInLowerLevel.y;
  const bool bHasBrickBack   = vBrickCoords.z*2+1 < bricksInLowerLevel.z;

  const UINT64VECTOR3 splitPos(
    uint64_t(ceil((tree.m_iBrickSize.x-2*m_iOverlap)/2.0)),
    uint64_t(ceil((tree.m_iBrickSize.y-2*m_iOverlap)/2.0)),
    uint64_t(ceil((tree.m_iBrickSize.z-2*m_iOverlap)/2.0))
  );

  // read up to eight bricks and filter them into the one brick
  UINT64VECTOR4 sourceIndex(vBrickCoords.x*2, vBrickCoords.y*2, vBrickCoords.z*2,
                            vBrickCoords.w-1);
  UINT64VECTOR3 targetOffset(0,0,0);
  DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                            pSourceData, sourceIndex,
                                            targetOffset);

  if (bHasBrickRight) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2+1, vBrickCoords.y*2,
                                vBrickCoords.z*2, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(splitPos.x,0,0);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickBottom) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2, vBrickCoords.y*2+1,
                                vBrickCoords.z*2, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(0,splitPos.y,0);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickBack) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2, vBrickCoords.y*2,
                                vBrickCoords.z*2+1, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(0,0,splitPos.z);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickRight && bHasBrickBottom) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2+1, vBrickCoords.y*2+1,
                                vBrickCoords.z*2, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(splitPos.x,splitPos.y,0);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickRight && bHasBrickBack) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2+1, vBrickCoords.y*2,
                                vBrickCoords.z*2+1, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(splitPos.x,0,splitPos.z);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickBottom && bHasBrickBack) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2, vBrickCoords.y*2+1,
                                vBrickCoords.z*2+1, vBrickCoords.w-1);
    targetOffset = UINT64VECTOR3(0,splitPos.y,splitPos.z);
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  if (bHasBrickRight && bHasBrickBottom && bHasBrickBack) {
    sourceIndex = UINT64VECTOR4(vBrickCoords.x*2+1, vBrickCoords.y*2+1,
                                vBrickCoords.z*2+1, vBrickCoords.w-1);
    targetOffset = splitPos;
    DownsampleBricktoBrick<T, bComputeMedian>(tree, pData, vTargetBricksize,
                                              pSourceData, sourceIndex,
                                              targetOffset);
  }

  SetBrick((uint8_t*)pData, tree, vBrickCoords);
}

template<class T, bool bComputeMedian>
void ExtendedOctreeConverter::ComputeHierarchy(ExtendedOctree &tree,
                                               bool bClampToEdge) {
  // if the conversion to size_t actually clamps things, this code is b0rked.
  assert(static_cast<uint64_t>(static_cast<size_t>(tree.m_iBrickSize.volume() *
         tree.m_iComponentCount)) == tree.m_iBrickSize.volume() *
         tree.m_iComponentCount &&
         "conversion to size_t changes data value; brick too large.");

  T* pTempDataSource = new T[size_t(tree.m_iBrickSize.volume() *
                                    tree.m_iComponentCount)];
  T* pTempDataTarget = new T[size_t(tree.m_iBrickSize.volume() *
                                    tree.m_iComponentCount)];
  for (size_t LoD = 1;LoD<tree.m_vLODTable.size();LoD++) {
    UINT64VECTOR3 bricksInThisLoD = tree.GetBrickCount(LoD);
    for (uint64_t z = 0;z<bricksInThisLoD.z;z++) {
      for (uint64_t y = 0;y<bricksInThisLoD.y;y++) {
        for (uint64_t x = 0;x<bricksInThisLoD.x;x++) {
          DownsampleBrick<T, bComputeMedian>(tree, bClampToEdge,
                                             UINT64VECTOR4(x,y,z, LoD),
                                             pTempDataSource, pTempDataTarget);
        }
      }
    }
    // fill overlaps in this LoD
    FillOverlap(tree, LoD, bClampToEdge);

    // apply compression to this LoD
    Compress(tree, size_t(tree.m_vTOC.size()-bricksInThisLoD.volume()));
  }
  delete [] pTempDataSource;
  delete [] pTempDataTarget;
}

template<class T> BrickStatVec
ExtendedOctreeConverter::ComputeBrickStats(uint8_t* pData, uint64_t iLength,
                                           size_t iComponentCount) {
  BrickStatVec minmax(iComponentCount);

  for (size_t i = 0;i<iComponentCount;++i) {
    minmax[i] = BrickStats<double>(std::numeric_limits<double>::max(),
                                  -std::numeric_limits<double>::max());
  }

  size_t iElemCount = size_t(iLength / sizeof(T));

  if (iElemCount >= iComponentCount) {
    T* pElements = (T*)pData;

    for (size_t i = 0;i<iElemCount;i+=iComponentCount) {
      for (size_t c = 0;c<iComponentCount;++c) {
        minmax[c].minScalar = std::min<double>(double(*pElements),
                                               minmax[c].minScalar);
        minmax[c].maxScalar = std::max<double>(double(*pElements),
                                               minmax[c].maxScalar);
        ++pElements;
      }
    }
  }

  return minmax;
}
