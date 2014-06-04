#ifndef CALLPERFORMER_H
#define CALLPERFORMER_H
#include "DynamicBrickingDS.h"
#include <vector>
//using tuvok::AbstrRenderer;
using tuvok::DynamicBrickingDS;
using std::vector;

class CallPerformer
{
    //AbstrRenderer *renderer;
    DynamicBrickingDS *ds;

public:
    CallPerformer();
    ~CallPerformer();

    //File handling
    vector<std::string> listFiles();
    void openFile(const char* filename);
    void closeFile(const char* filename);

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
