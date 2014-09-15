#include "callperformer.h"
#include "dirent.h"
#include "uvfDataset.h"
#include <limits.h>
#include "DebugOut/debug.h"
#include "GLGridLeaper.h"
#include "ContextIdentification.h"
#include "BatchContext.h"
#include "LuaScripting/TuvokSpecific/LuaDatasetProxy.h"
#include "RenderRegion.h"
#include "Renderer/GL/QtGLContext.h"
#include "Renderer/GL/GLStateManager.h"
using tuvok::DynamicBrickingDS;
using tuvok::UVFDataset;
using std::string;

#define SHADER_PATH "Shaders"

DECLARE_CHANNEL(dataset);
DECLARE_CHANNEL(renderer);
DECLARE_CHANNEL(file);

CallPerformer::CallPerformer()
:maxBatchSize(defaultBatchSize)
{
}

CallPerformer::~CallPerformer() {
    invalidateRenderer();
}

void CallPerformer::invalidateRenderer() {
    std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();
    if (rendererInst.isValid(ss)) {
        ss->cexec(rendererInst.fqName() + ".cleanup");
        tuvok::Controller::Instance().ReleaseVolumeRenderer(rendererInst);
        rendererInst.invalidate();
    }
    if (dsInst.isValid(ss)) {
        dsInst.invalidate();
    }
}

DynamicBrickingDS* CallPerformer::getDataSet() {
    std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();
    if(!dsInst.isValid(ss))
        return NULL;

    tuvok::LuaDatasetProxy* ds = dsInst.getRawPointer<tuvok::LuaDatasetProxy>(ss);
    return (DynamicBrickingDS*)ds->getDataset();
}

AbstrRenderer* CallPerformer::getRenderer() {
    std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();
    if(!rendererInst.isValid(ss))
        return NULL;

    return rendererInst.getRawPointer<tuvok::AbstrRenderer>(ss);
}

//File handling
vector<string> CallPerformer::listFiles() {
    const char* folder = getenv("IV3D_FILES_FOLDER");
    if(folder == NULL) {
        folder = "./";
    }

    vector<string> retVector;
    string extension = ".uvf";

    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir (folder)) != NULL) {
        printf("\nFound the following files in folder %s:\n", folder);
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {

            //No directories
            if(ent->d_type != DT_DIR) {
                string fname = ent->d_name;

                //printf("%s\n", fname.c_str());

                //Only files ending in .uvf
                if (fname.find(extension, (fname.length() - extension.length())) != string::npos) {
                    string tmp_string = ent->d_name;
                    printf("%s,\n", tmp_string.c_str());
                    retVector.push_back(tmp_string);
                }
            }
        }
        printf("End of list!\n");
        closedir (dir);
    } else {
        /* could not open directory */
        perror("Could not open folder for uvf files!");
        abort();
    }

    return retVector;
}

std::shared_ptr<tuvok::BatchContext> createContext(uint32_t width, uint32_t height,
                                            int32_t color_bits,
                                            int32_t depth_bits,
                                            int32_t stencil_bits,
                                            bool double_buffer, bool visible)
{
  std::shared_ptr<tuvok::BatchContext> ctx(
      tuvok::BatchContext::Create(width,height, color_bits,depth_bits,stencil_bits,
                           double_buffer,visible));
  if(!ctx->isValid() || ctx->makeCurrent() == false)
  {
    std::cerr << "Could not utilize context.";
    return std::shared_ptr<tuvok::BatchContext>();
  }
  else {
      char* glVersion = (char*)glGetString(GL_VERSION);
      printf("Created GL-Context with version: %s\n", glVersion);
  }

  return ctx;
}

bool CallPerformer::openFile(const string& filename, const std::vector<size_t>& bSize, size_t minmaxMode) {
    const char* folder = getenv("IV3D_FILES_FOLDER");
    if(folder == NULL) {
        folder = "./";
    }

    string effectiveFilename = folder;
    if(effectiveFilename.back() != '/') {
        effectiveFilename.append("/");
    }
    effectiveFilename.append(filename);
    TRACE(file, "Opening file: %s\n", effectiveFilename.c_str());

    char buff[200];

    //Hook up dataset to renderer
    UINTVECTOR3 maxBS(bSize[0], bSize[1], bSize[2]);
    UINTVECTOR2 res(width, height);

    //Dirty hack because the lua binding cannot work with MATRIX or VECTOR
    sprintf(buff, "{%d, %d, %d}", maxBS.x, maxBS.y, maxBS.z);
    string maxBSStr(buff);
    sprintf(buff, "{%d, %d}", res.x, res.y);
    string resStr(buff);

    //Without Lua
    /*{
        tuvok::MasterController& controller = tuvok::Controller::Instance();

        std::shared_ptr<tuvok::Context> ctx = createContext(width, height, 32, 24, 8, true, false);
        AbstrRenderer* renderer = controller.RequestNewVolumeRenderer(
                    tuvok::MasterController::OPENGL_GRIDLEAPER,
                    true, false, false, false);

        renderer->AddShaderPath(SHADER_PATH);

        renderer->LoadFile(effectiveFilename);
        renderer->LoadRebricked(effectiveFilename, maxBS, minmaxMode);

        renderer->Initialize(ctx);
        renderer->Resize(res);
        renderer->SetRendererTarget(tuvok::AbstrRenderer::RT_HEADLESS);
        renderer->Paint();
    }*/

    //Start Lua scripting
    {
        std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();

        //Create openGL-context
        ss->registerFunction(createContext, "tuvok.createContext",
                             "Creates a rendering context and returns it.", false);
        std::shared_ptr<tuvok::Context> ctx = ss->cexecRet<std::shared_ptr<tuvok::Context>>(
                    "tuvok.createContext",
                    width, height,
                    32, 24, 8,
                    true, false);

        //Create renderer
        rendererInst = ss->cexecRet<tuvok::LuaClassInstance>(
                    "tuvok.renderer.new",
                    tuvok::MasterController::OPENGL_GRIDLEAPER, true, false,
                    false, false);

        if(!rendererInst.isValid(ss)) {
            abort(); //@TODO: properly handle
        }

        string rn = rendererInst.fqName();
        ss->cexec(rn+".addShaderPath", SHADER_PATH);

        ss->cexec(rn+".loadDataset", effectiveFilename.c_str());
        dsInst = ss->cexecRet<tuvok::LuaClassInstance>(rn+".getDataset");

        //Load rebricked
        {
            sprintf(buff, "%s.loadRebricked(\"%s\", %s, %zu)",
                    rn.c_str(),
                    effectiveFilename.c_str(),
                    maxBSStr.c_str(),
                    minmaxMode);
            string loadRebrickedString(buff);
            printf("Load rebricked string: %s\n", loadRebrickedString.c_str());

            //FIXME(file, "This call claims that there would be no such file when using full path on OSX...dafuq");
            if(!ss->execRet<bool>(loadRebrickedString)) {
                return false;
            }
        }

        //Render init
        bool renderInit = ss->cexecRet<bool>(rn+".initialize", ctx);
        if(!renderInit) {
            printf("Could not initalize renderer with given context!!!\n");
            abort();
        }

        ss->exec(rn+".resize("+resStr+")");
        ss->cexec(rn+".setRendererTarget", tuvok::AbstrRenderer::RT_HEADLESS); //From CMDRenderer.cpp... but no idea why
        ss->cexec(rn+".setBlendPrecision", tuvok::AbstrRenderer::EBlendPrecision::BP_8BIT);
        ss->cexec(rn+".paint");
    }

    return true;
}

void CallPerformer::closeFile(const string& filename) {
    (void)filename; /// @TODO: keep it around to see which file to close?

    invalidateRenderer();
}

void CallPerformer::rotate(const float *matrix) {
    std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();
    if(!rendererInst.isValid(ss)) {
        WARN(renderer, "No renderer created! Aborting request.");
        return;
    }

#if 0
    //const FLOATMATRIX4 rotation(matrix);
    //Dirty hack because the lua binding cannot work with MATRIX or VECTOR
    string matString = "{";
    for(size_t i = 0; i < 15; i++) {
        matString += std::to_string(matrix[i]) + ", ";
    }
    matString += std::to_string(matrix[15]);
    matString += "}";

    const string rn = rendererInst.fqName();
    FIXME(renderer, "For some reason we cannot retrieve the renderRegion...");
    tuvok::LuaClassInstance renderRegion = ss->cexecRet<tuvok::LuaClassInstance>(rn+".getFirst3DRenderRegion");
    ss->exec(renderRegion.fqName()+".setRotation4x4("+matString+")");
#else
    AbstrRenderer* ren = rendererInst.getRawPointer<AbstrRenderer>(ss);
    std::shared_ptr<tuvok::RenderRegion3D> rr3d = ren->GetFirst3DRegion();
    ren->SetRotationRR(rr3d.get(), FLOATMATRIX4(matrix));
    const string rn = rendererInst.fqName();
#endif
    ss->cexec(rn+".paint");
}

std::vector<tuvok::BrickKey> CallPerformer::getRenderedBrickKeys() {
    std::shared_ptr<tuvok::LuaScripting> ss = tuvok::Controller::Instance().LuaScript();
    if(!rendererInst.isValid(ss) || !dsInst.isValid(ss)) {
        WARN(renderer, "Renderer or DataSet not initialized! Aborting request.");
        return std::vector<tuvok::BrickKey>(0);
    }

    //Retrieve a list of bricks that need to be send to the client
    AbstrRenderer* tmp_ren = getRenderer();
    const tuvok::GLGridLeaper* glren = dynamic_cast<tuvok::GLGridLeaper*>(tmp_ren);
    assert(glren && "not a grid leaper?  wrong renderer in us?");
    const std::vector<UINTVECTOR4> hash = glren->GetNeededBricks();
    const tuvok::LinearIndexDataset& linearDS = dynamic_cast<const tuvok::LinearIndexDataset&>(*getDataSet());

    std::vector<tuvok::BrickKey> allKeys(0);
    for(UINTVECTOR4 b : hash) {
        //printf("Test: x:%d y:%d z:%d, w:%d\n", b.x, b.y, b.z, b.w);
        allKeys.push_back(linearDS.IndexFrom4D(b, 0));
    }

    return allKeys;
}
