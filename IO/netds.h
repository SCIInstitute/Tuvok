#ifndef NETDATASET_H
#define NETDATASET_H
#include <stdlib.h>
#include "sockhelp.h"
#include <vector>
#include <array>
#include <string>
using std::vector;
using std::string;
using SOCK::PlainTypeInfo;
using std::shared_ptr;

namespace NETDS {

    struct DSMetaData {
        size_t lodCount;
        vector<unsigned> layouts; //3 unsigned per LOD

        size_t brickCount; //total count of bricks

        //For the keys... Reconstruct using BrickKey(0, lods[i], idxs[i]);
        vector<size_t> lods; //(size == brickCount)
        vector<size_t> idxs; //(size == brickCount)

        //For the brickMD
        vector<float> md_centers;  //(size == brickCount * 3)
        vector<float> md_extents;  //(size == brickCount * 3)
        vector<uint32_t> md_n_voxels;  //(size == brickCount * 3)

        //To find out the type of data inside the set
        struct PlainTypeInfo typeInfo;

        //brick zero
        //void* brickZero;
    };

    struct BatchInfo {
        size_t batchSize;
        //For the keys... Reconstruct using BrickKey(0, lods[i], idxs[i]);
        vector<size_t> lods;
        vector<size_t> idxs;
        vector<size_t> brickSizes; //Not in bytes, but in elements of the type
        bool moreDataComing;
    };

    struct RotateInfo {
        size_t brickCount;
        vector<size_t> lods;
        vector<size_t> idxs;
    };

    struct MMInfo {
        size_t brickCount;
        vector<size_t> lods;
        vector<size_t> idxs;
        vector<double> minScalars;
        vector<double> maxScalars;
        vector<double> minGradients;
        vector<double> maxGradients;
    };

    bool openFile(const string& filename, DSMetaData& out_meta, size_t minmaxMode, std::array<size_t, 3> bSize, uint32_t width, uint32_t height);
    void closeFile(const string& filename);
    bool listFiles(vector<string>& resultBuffer);
    void shutdownServer();

    //For requesting single bricks
    bool getBrick(const size_t lod, const size_t bidx, vector<uint8_t>& resultBuffer);
    bool getBrick(const size_t lod, const size_t bidx, vector<uint16_t>& resultBuffer);
    bool getBrick(const size_t lod, const size_t bidx, vector<uint32_t>& resultBuffer);

    //For requesting multiple bricks at once
    bool getBricks(const size_t brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint8_t>>& resultBuffer);
    bool getBricks(const size_t brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint16_t>>& resultBuffer);
    bool getBricks(const size_t brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint32_t>>& resultBuffer);

    /// the client uses these to know the metadata to load.
    //void setClientMetaData(DSMetaData d); //being set automatically with an openFile call
    struct DSMetaData clientMetaData();

    //calculates minMax values on the server and returns them. Then saves it internally
    bool calcMinMax(MMInfo& result);
    //void setMinMaxInfo(MMInfo info);
    shared_ptr<MMInfo> getMinMaxInfo();
    void clearMinMaxValues();

    //Initiates a sending of bricks that the client might need!
    //WARNING! call setBatchSize first!!!
    //RotateInfo is similar to BatchInfo, but contains the keys for ALL rendered bricks
    void rotate(const float m[16]);
    shared_ptr<RotateInfo> getLastRotationKeys(); //can be NULL
    void clearRotationKeys();

    void setBatchSize(size_t maxBatchSize);
    size_t batchSize();
    bool readBrickBatch(BatchInfo &out_info, vector<vector<uint8_t>>& resultBuffer);
    bool readBrickBatch(BatchInfo &out_info, vector<vector<uint16_t>>& resultBuffer);
    bool readBrickBatch(BatchInfo &out_info, vector<vector<uint32_t>>& resultBuffer);
}


#endif
