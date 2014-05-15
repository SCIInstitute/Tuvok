#ifndef CALLPERFORMER_H
#define CALLPERFORMER_H
#include "IO/uvfDataset.h"
#include <vector>
using tuvok::UVFDataset;
using std::vector;

class CallPerformer
{
    UVFDataset *ds;

public:
    CallPerformer();
    ~CallPerformer();

    //File handling
    vector<std::string> listFiles();
    void openFile(const char* filename);
    void closeFile(const char* filename);

    template <class T>
    void brick_request(const size_t lod, const size_t bidx, vector<T>& data) {
        if(true) { //just testing some stuff
            uint8_t test1 = 34;
            uint8_t test2 = 255;
            uint8_t test3 = 78;

            data.push_back(test1);
            data.push_back(test2);
            data.push_back(test3);
            return;
        }

        if(ds == NULL) {
            printf("DataSource is can't retrieve brick!!\n");
        }
        else {
            tuvok::BrickKey key(0, lod, bidx);
            ds->GetBrick(key, data);
        }
    }
};

#endif // CALLPERFORMER_H
