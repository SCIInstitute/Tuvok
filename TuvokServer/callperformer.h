#ifndef CALLPERFORMER_H
#define CALLPERFORMER_H
#include "DynamicBrickingDS.h"
#include "AbstrRenderer.h"
#include <vector>
using tuvok::AbstrRenderer;
using tuvok::DynamicBrickingDS;
using std::vector;

const size_t defaultBatchSize = 10;

class CallPerformer
{
public:
    AbstrRenderer *renderer; //not set yet
    DynamicBrickingDS *ds;
    size_t maxBatchSize;

    CallPerformer();
    ~CallPerformer();

    //File handling
    vector<std::string> listFiles();
    bool openFile(const char* filename);
    void closeFile(const char* filename);
    void rotate(const float* matrix);
    std::vector<tuvok::BrickKey> getRenderedBrickKeys();

    template <class T>
    void brick_request(const size_t lod, const size_t bidx, vector<T>& data) {

        if(ds == NULL) {
            printf("DataSource is NULL, can't retrieve brick!!\n");
        }
        else {
            tuvok::BrickKey key(0, lod, bidx);
            ds->GetBrick(key, data);
        }
    }
};

#endif // CALLPERFORMER_H
