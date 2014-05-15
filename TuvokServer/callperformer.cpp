#include "callperformer.h"
#include "dirent.h"

CallPerformer::CallPerformer()
:ds(NULL)
{
}

CallPerformer::~CallPerformer() {
    delete ds;
}

//File handling
vector<char*> CallPerformer::listFiles() {
    const char* folder = getenv("IV3D_FILES_FOLDER");
    if(folder == NULL) {
        folder = "./";
    }

    vector<char*> retVector;

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (folder)) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            char *tmp_string = new char[ent->d_namlen];
            strcpy(tmp_string, ent->d_name);
            retVector.push_back(tmp_string);
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror("Could not open folder for uvf files!");
        abort();
    }

    return retVector;
}

void CallPerformer::openFile(const char* filename) {
    const char* folder = getenv("IV3D_FILES_FOLDER");
    if(folder == NULL) {
        folder = "./";
    }

    std::string effectiveFilename = folder;
    effectiveFilename.append(filename);
    printf("Effective path: %s\n", effectiveFilename.c_str());
    ds = new UVFDataset(effectiveFilename, 256, false);
}

void CallPerformer::closeFile(const char* filename) {
    (void)filename; //TODO: maybe keep it around to see, which file to close... currently not planned
    delete ds;
    ds = NULL;
}
