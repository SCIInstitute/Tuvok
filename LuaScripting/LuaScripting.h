/*
 For more information, please see: http://software.sci.utah.edu

 The MIT License

 Copyright (c) 2012 Scientific Computing and Imaging Institute,
 University of Utah.


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

/**
 \file    LuaScripting.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 21, 2012
 \brief   Interface to the Lua scripting system built for Tuvok.

          To see examples of how to use the system, consult the unit tests at
          the bottom of LuaMemberReg.cpp and LuaScripting.cpp.
 */

#ifndef TUVOK_LUASCRIPTING_H_
#define TUVOK_LUASCRIPTING_H_

#include <functional>
#include <memory>

#ifndef LUASCRIPTING_NO_TUVOK

#include "3rdParty/LUA/lua.hpp"

#else

#include "Lua/lua.hpp"
#include "NoTuvok/LuaTuvokException.h"

#ifdef __clang__
// Place shared_ptr and function in the tr1 namespace.
namespace std { 
  namespace tr1 {
    using std::shared_ptr;
    using std::function;
  }
}
#endif

#endif


#include "LuaCommon.h"
#include "LuaError.h"
#include "LuaFunBinding.h"
#include "LuaStackRAII.h"
#include "LuaScriptingExecHeader.h"

namespace tuvok
{

class LuaProvenance;
class LuaMemberRegUnsafe;
class LuaClassConstructor;
template <class T> class LuaClassRegistration;

/// Usage Note: If you construct any Lua Class instances that retain a
/// shared_ptr reference to this LuaScripting class, be sure to call
/// removeAllRegistrations before deleting LuaScripting.
/// Consider using weak_ptr instead.
class LuaScripting
{
  // TODO: Expose getLuaState function and most of these can go away.
  friend class LuaMemberRegUnsafe;  // For getNewMemberHookID.
  friend class LuaProvenance;       // For obtaining function tables.
  friend class LuaStackRAII;        // For unwinding lua stack during exception
  friend class LuaClassInstanceHook;// For getNewMemberHookID.
  friend class LuaClassConstructor; // For createCallableFuncTable.
  template<class T> friend class LuaClassRegistration;// For notifyOfDeletion.
  friend class LuaClassInstance;    // For obtaining Lua instance.
public:

  LuaScripting();
  virtual ~LuaScripting();

  /// It is highly recommended to call this function as your program is about
  /// to terminate. All it does is clear the provenance history and perform
  /// a complete Lua garbage collection cycle. This is necessary to get rid
  /// of any shared_ptr objects that may be in the provenance system.
  /// If you pass any shared_ptr's that reference LuaScripting, you must call
  /// this function. Otherwise, you will have memory leaks.
  void clean();

  /// Removes all class definitions and function registrations from the system.
  /// This will clean up any lingering classes that may have lingering
  /// shared pointer references.
  void removeAllRegistrations();

  /// Registers a static C++ function with Lua.
  /// Since Lua is compiled as CPP, it is safe to throw exceptions from the
  /// function pointed to by f (since Lua detects that it is being compiled in
  /// cpp, and uses exceptions instead of long jumping).
  /// \param  f         Any function pointer.
  ///                   f's parameters and return value will be handled
  ///                   automatically.
  ///                   The number of parameters allowed in f is limited by
  ///                   the templates in LuaFunBinding.h.
  /// \param  name      Period delimited fully qualified name of f inside of Lua
  ///                   No characters other than those regularly allowed inside
  ///                   C++ functions are allowed, with the exception of periods
  ///                   Example: "renderer.eye"
  /// \param  desc      Description of f.
  /// \param  undoRedo  If true, this function will add itself to the
  ///                   provenance tracking system and undo/redo will be handled
  ///                   automatically. If false, the function will not use
  ///                   undo/redo functionality in the provenance system.
  ///                   You will not want undo/redo on functions that do not
  ///                   modify state (such as getters).
  ///
  /// \return The name parameter. You can use this as an id.
  ///
  /// NOTES: The system is re-entrant, so you may call lua from within
  /// registered functions, or even call other registered functions.
  ///
  /// Automatic Undo/Redo: In order for automatic undo/redo to function
  /// properly with registered functions, the following property needs to be
  /// satisfied: your functions must be idempotent, or a composition
  /// of idempotent functions such that all of the side-effects of the function
  /// are known to the provenance system. This is further explained in the
  /// Lua Scripting paper.
  ///
  /// TO REGISTER MEMBER FUNCTIONS:
  /// Use the LuaMemberReg mediator class. It will clean up for you.
  template <typename FunPtr>
  std::string registerFunction(FunPtr f,
                               const std::string& name,
                               const std::string& desc,
                               bool undoRedo);

  /// Hooks a fully qualified function name with the given function.
  /// All hooks are called directly after the bound Lua function is called,
  /// but before the return values (if any) are sent back to Lua.
  /// \param  f         Function pointer.
  /// \param  name      Fully qualified name to hook.
  //
  /// TO HOOK USING MEMBER FUNCTIONS:
  /// Use the LuaMemberReg mediator class. It will clean up for you.
  template <typename FunPtr>
  void strictHook(FunPtr f, const std::string& name);

  /// Sets the undo function for the registered function specified by 'name'.
  /// The default undo function is to re-execute the last command executed.
  /// E.G. If you execute "fun(1.5)", then "fun(5.5)", then execute an undo,
  ///      the undo will execute fun(1.5) to undo fun(5.5).
  /// If there is no prior command, then the command is called with 'default'
  /// values. Default values are determined by the LuaStrictStack class, or
  /// by the user specifying them using setDefault.
  /// E.G. If an undo is executed again on the example given above, fun(0.0)
  ///      will be executed to undo fun(1.5). 0.0 is the default for a float.
  ///      To reset defaults on a per function basis, use the setDefaults
  ///      family of functions.
  /// \param  f       Function to call. If the function does not have the
  ///                 same signature of the function registered at 'name',
  ///                 a runtime exception will be thrown if RTTI is enabled.
  /// \param  name    The fully qualified name of the function that you wish to
  ///                 reset the undo function for.
  template <typename FunPtr>
  void setUndoFun(FunPtr f, const std::string& name);

  /// Use this function to ensure that no functions are called on undo.
  /// The last executed parameters table is still updated upon undo.
  /// Use this function if you don't know what arguments
  /// the function takes and you would like to nullify its undo/redo
  /// operations. It is used to nullify undo on constructors / destructors
  /// of lua instance classes.
  /// All composited functions are called.
  /// XXX: Could be replaced with a function hook that ignores the function's
  ///      arguments (does not enforce strict argument compliance).
  void setNullUndoFun(const std::string& name);

  /// Sets the redo function for the registered function specified by name.
  /// The default redo is just to re-execute the function with the same args.
  /// The f and name parameters are the exact same as setUndoFun.
  template <typename FunPtr>
  void setRedoFun(FunPtr f, const std::string& name);

  /// Use this function to ensure that no functions are called on redo.
  /// The last executed parameters table is still updated upon redo.
  void setNullRedoFun(const std::string& name);

  /// Registers a Lua class. This method was ultimately chosen due to 3 reasons:
  /// 1) Depending on what registration functions we use, we can potentially
  ///    determine what functions are associated with a particular class
  ///    without having to create an instance of the class.
  /// 2) Gets rid of the pointer stack / id stacks that we setup to support
  ///    function registration in the second method.
  /// 3) Can register classes that know nothing about LuaScripting.
  ///    Particularly useful for not breaking encapsulation. A practical
  ///    example of this is to register the Dataset IO class without IO
  ///    knowing about Tuvok.
  /// \param fp         Pointer to a function that will construct the class and
  ///                   return a pointer to it.
  /// \param className  Fully qualified class name.
  /// \param desc       Class description.
  /// \param callback   Callback issued directly after the constructor is called
  ///                   This callback is tightly bound to the return type of
  ///                   FunPtr. If FunPtr has the return type of T*, then the
  ///                   function signature the callback is:
  ///                   callback(LuaClassRegistration<T>& reg, T* cls);
  /// @{
  template <typename CLS, typename T, typename FunPtr>
  void registerClass(T* t, FunPtr fp, const std::string& className,
                     const std::string& desc,
                     typename LuaClassRegCallback<CLS>::Type callback);

  template <typename CLS, typename FunPtr>
  void registerClassStatic(FunPtr fp, const std::string& className,
                           const std::string& desc,
                           typename LuaClassRegCallback<CLS>::Type callback);
  /// @}


  /// Retrieves the LuaClassInstance given the object pointer.
  /// Only use the pointer that was returned from the class constructor.
  LuaClassInstance getLuaClassInstance(void* p);

  /// Executes a command.
  ///
  /// Example: exec("provenance.undo()")
  ///   or     exec("myFunc(34, "string", ...)")
  void exec(const std::string& cmd);

  /// Executes a command and returns 1 value.
  ///
  /// Example: T a = executeRet<T>("myFunc()")
  template <typename T>
  T execRet(const std::string& cmd);

  /// The following functions allow you to call a function using C++ types.
  /// These function are more efficient than the exec functions given above.
  /// The general form of these functions is given in the below example
  ///
  /// Example: cexec("myFunc", a, b, c, d, ...)
  ///@{
  void cexec(const std::string& cmd);

  // Include the cexec function prototypes.
  TUVOK_LUA_CEXEC_FUNCTIONS
  ///@}

  /// The following functions allow you to call a function using C++ types.
  /// Unlike the functions above, these functions also return the execution
  /// result of the function.
  ///
  /// NOTE: You must match the return type of the function exactly, otherwise
  ///       a runtime exception will be thrown.
  ///
  /// Example: T a = cexecRet<T>("myFunc", p1, p2, ...)
  ///@{
  template <typename T>
  T cexecRet(const std::string& cmd);

  // Include the cexecRet function prototypes.
  TUVOK_LUA_CEXEC_RET_FUNCTIONS
  ///@}

  /// The following functions allow you to specify default parameters to use
  /// for registered functions.
  /// This is so you can specify different undo/redo defaults (such as turning
  /// lighting ON by default).
  /// NOTE: You should call this directly after registering the function (name).
  ///       Waiting to set defaults until after the function has been called
  ///       results in undefined behavior on the undo/redo stack.
  /// \param  name        Fully qualified name of the function whose defaults
  ///                     you would like to set.
  /// \param  callFunc    If true, the function will be called for you with the
  ///                     given parameters.
  /// \param  call        If true, the function indicated by (name) will be
  ///                     called with the given parameters. This call will
  ///                     not be logged with the provenance system.
  ///
  /// Example: setDefaults("myFunc", p1, p2, ... , false)
  ///@{

  // Here is an example function prototype for setDefaults.
  //  template <typename P1>
  //  void setDefaults(const std::string& name,
  //                   P1 p1, bool call);

  // Include setDefaults function prototypes.
  TUVOK_LUA_SETDEFAULTS_FUNCTIONS
  ///@}

  /// TODO: Build the set optional parameters functions.
  ///       Possible signature:
  /// void setOptional(std::string id, int arg, T paramValue);
  /// Lets you specify optional parameters for registered functions.
  /// All optional parameters must be grouped at the end of the parameter list.
  /// Optional parameters are different from defaults in that defaults deal
  /// with undoing the very first call. Whereas, optional parameters
  /// indicate that if certain parameters aren't specified, the parameters
  /// should be filled with the given optional values.

  /// Sets additional parameter information (such as name and description).
  /// \param fqname   Fully qualified name of the function.
  /// \param paramID  Zero based. 0 represents the first parameter, n represents
  ///                 the (n-1)'th parameter.
  /// \param name     The name of the parameter.
  /// \param desc     The description.
  void addParamInfo(const std::string& fqname, int paramID,
                    const std::string& name, const std::string& desc);

  /// Sets additional return value information.
  void addReturnInfo(const std::string& fqname, const std::string& desc);

  /// Default: Provenance is enabled. Disabling provenance will disable undo/
  /// redo.
  bool isProvenanceEnabled() const;
  void enableProvenance(bool enable);

  LuaClassInstance::IDType getCurGlobalInstID() const{return mGlobalInstanceID;}
  void incrementGlobalInstID()    {mGlobalInstanceID++;}

  /// Function description returned from getFuncDescs().
  struct FunctionDesc
  {
    std::string funcName;   ///< Name of the function.
    std::string funcDesc;   ///< Description of the function provided by the
                            ///< registrar.
    std::string paramSig;   ///< Function parameter signature, only parameters.
    std::string funcSig;    ///< Full function signature.
    std::string funcFQName; ///< Fully qualified function name.
  };

  /// Return all function descriptions.
  std::vector<FunctionDesc> getAllFuncDescs() const;

  // Names for data stored in a function's encapsulating table.
  // Exposed for unit testing purposes.
  static const char* TBL_MD_DESC;         ///< Description
  static const char* TBL_MD_SIG;          ///< Signature
  static const char* TBL_MD_SIG_NO_RET;   ///< No name signature without return
  static const char* TBL_MD_SIG_NAME;     ///< Signature with name
  static const char* TBL_MD_NUM_EXEC;     ///< Number of executions
  static const char* TBL_MD_QNAME;        ///< Fully qualified function name
  static const char* TBL_MD_FUN_PDEFS;    ///< Function parameter defaults
  static const char* TBL_MD_FUN_LAST_EXEC;///< Parameters from last execution
  static const char* TBL_MD_HOOKS;        ///< Static function hooks table
  static const char* TBL_MD_HOOK_INDEX;   ///< Static function hook index
  static const char* TBL_MD_MEMBER_HOOKS; ///< Class member function hook table
  static const char* TBL_MD_CPP_CLASS;    ///< Light user data to LuaScripting
  static const char* TBL_MD_STACK_EXEMPT; ///< True if undo/redo stack exempt
  static const char* TBL_MD_PROV_EXEMPT;  ///< True if provenance exempt.
  static const char* TBL_MD_NUM_PARAMS;   ///< Number of parameters accepted.
  static const char* TBL_MD_UNDO_FUNC;    ///< Non-nil if undo hook present.
  static const char* TBL_MD_REDO_FUNC;    ///< Non-nil if redo hook present.
  static const char* TBL_MD_NULL_UNDO;    ///< If true, no undo function
                                          ///< is called.
  static const char* TBL_MD_NULL_REDO;    ///< If true, no redo function
                                          ///< is called.
  static const char* TBL_MD_PARAM_DESC;   ///< Additional parameter descriptions
                                          ///< table.

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  static const char* TBL_MD_TYPES_TABLE;  ///< type_info userdata table.
#endif

  static const char* PARAM_DESC_NAME_SUFFIX;
  static const char* PARAM_DESC_INFO_SUFFIX;

  static const char* SYSTEM_NOP_COMMAND;  ///< The no-op command.

  /// Sets a flag in the Lua registry indicating that an exception is expected
  /// that will cause the lua stack to be unbalanced in internal functions.
  /// This is to avoid debug log output, more than anything else.
  void setExpectedExceptionFlag(bool expected);

  /// Please do not use this function in production code. It is used for
  /// testing purposes.
  LuaProvenance* getProvenanceSys() const {return mProvenance.get();}

  /// Used for testing purposes only.
  LuaClassInstance::IDType getCurrentClassInstID() {return mGlobalInstanceID;}

  /// Retrieves the last created instance ID
  LuaClassInstance::IDType getLastCreatedInstID(){return mGlobalInstanceID - 1;}

  /// Used by friend class LuaProvenance.
  /// Any public use of this function should be for testing purposes only.
  lua_State* getLuaState() const {return mL;}

  /// Notifies the scripting system of an object's deletion.
  /// Use this function in the object's destructor if you believe deleteClass
  /// was not called on the object (E.G. the object is GUI window, and the user
  /// clicked the close button, and you have no control over the deletion of
  /// the object).
  /// It is safe to call this function repeatedly. You may also call the
  /// function even though deleteClass was called. deleteClass is reentrant for
  /// the same class.
  void notifyOfDeletion(LuaClassInstance inst);

  /// Allows you to temporarily disable the provenance system. This will not
  /// wipe any provenance records, but will allow you to make lua calls without
  /// having them logged by the system.
  /// This function routes to mProvenance->setDisableProvTemporarily(...)
  void setTempProvDisable(bool disable);

  /// This function should be used sparingly, and only for those functions that
  /// do not modify state nor return internal state in some way.
  /// Turns out that we want functions that get called every frame to be
  /// provenance exempt as well. Otherwise the provenance record is useless
  /// and grows too fast (which is a performance concern).
  /// E.G. Debug logging functions should be provenance exempt.
  void setProvenanceExempt(const std::string& fqName);

  /// Returns true if we are running in verbose mode.
  bool isVerbose()    {return mVerboseMode;}

  /// Returns a list of possible completions to the given command.
  std::vector<std::string> completeCommand(const std::string& fqName);

  /// Retrieves the commands 'path'.
  /// E.G. getCmdPath("iv3d.ren.two.get") = "iv3d.ren.two"
  std::string getCmdPath(std::string fqName);

  /// All commands between these two function calls will be grouped together
  /// so that one provenance.undo() command will undo/redo all of them.
  /// @{
  void beginCommandGroup();
  void endCommandGroup();
  /// @}

  /// Verbose print. Prints a message using log.info only if verbose is enabled.
  void vPrint(const char* fmt, ...);

private: // Used by LuaMemberRegUnsafe

  /// Just calls provenance begin/end command.
  void beginCommand();
  void endCommand();

  /// Exec failure.
  void logExecFailure(const std::string& failure);

  /// Stack expectations: the function table at tableIndex, and the parameters
  /// required to call the function directly after the table on the stack.
  /// There must be no other values on the stack above tableIndex other than the
  /// table and the parameters to call the function.
  void doHooks(lua_State* L, int tableIndex, bool provExempt);

  /// Returns true if the function is provenance exempt.
  /// Used to tell whether or not we should log hooks later on.
  bool doProvenanceFromExec(lua_State* L,
                            std::shared_ptr<LuaCFunAbstract> funParams,
                            std::shared_ptr<LuaCFunAbstract> emptyParams);

private:

  template <typename FunPtr>
  void strictHookInternal(FunPtr f,
                          const std::string& name,
                          bool registerUndo,
                          bool registerRedo);

  /// Clears out all function's last exec tables. This is done when the
  /// provenance system is cleared. This even clears every class function's
  /// last exec table -- all tables must be cleared in order to get rid of
  /// dangling shared_ptrs.
  void clearAllLastExecTables();
  void clearLastExecFromTable();  ///< Expects recurse table to be on stack top.

  /// Pushes the table associated with the LuaClassInstance to the top of the
  /// stack. Returns false if it could not find the class instance table.
  bool getClassTable(LuaClassInstance inst);

  /// Ensures the function is not added to the undo/redo stack. Examples include
  /// the undo and redo functions themselves.
  void setUndoRedoStackExempt(const std::string& funcName);

  /// Prepare function for execution (places function on the top of the stack).
  void prepForExecution(const std::string& fqName);

  /// Execute the function on the top of the stack. Works excatly like lua_call.
  void executeFunctionOnStack(int nparams, int nret);

  /// Unregisters the function associated with the fully qualified name.
  void unregisterFunction(const std::string& fqName);

  /// Resets default for function. Value to set as default is assumed to be on
  /// the top of the stack. It is popped off the stack.
  /// \param  argumentPos     Function argument position (e.g. f(1,2,3), 1 is at
  ///                         argument pos 0, 2 at pos 1, and 3 at pos 2).
  /// \param  ftableStackPos  Stack position of the function table.
  void resetFunDefault(int argumentPos, int ftableStackPos);

  /// Unregisters all functions registered using this LuaScripting instance.
  /// Should NEVER be called outside LuaScripting's destructor.
  void unregisterAllFunctions();

  /// Returns a new member hook ID.
  std::string getNewMemberHookID();

  /// Recursive helper function for unregisterAllFunctions().
  /// Expects that the table from which to remove functions is on the top of the
  /// stack.
  /// \param  parentTable If 0, the table is remove from globals if it is a
  ///                     function.
  /// \param  tableName   Name of the table on the top of the stack.
  void removeFunctionsFromTable(int parentTable, const char* tableName);

  /// Returns true if the table at stackIndex is a registered function.
  /// Makes no guarantees that it is a function registered by this class.
  bool isRegisteredFunction(int stackIndex) const;

  /// Returns true if the function table at stackIndex was create by this
  /// LuaScripting class. For now, we create our own unique Lua instance, so
  /// we are guaranteed this, but this is forward thinking for the future
  /// when we need to share the Lua state.
  bool isOurRegisteredFunction(int stackIndex) const;

  /// Returns true if the given fully qualified function name would be stored
  /// in lua globlas.
  bool isGlobalFunction(std::string fqName);

  /// Retrieves the function table given the fully qualified function name.
  /// Places the function table at the top of the stack. If the function fails
  /// to find the function table, false is returned and nothing is pushed
  /// onto the stack.
  /// Note: You can use this function as a generic way of grabbing tables
  ///       at a particular fully qualified name.
  bool getFunctionTable(const std::string& fqName);

  /// Creates a callable Lua table. classInstance can be NULL.
  /// Leaves the table on the top of the Lua stack.
  void createCallableFuncTable(lua_CFunction proxyFunc, void* realFuncToCall);

  /// Populates the table at the given index with the given function metadata.
  void populateWithMetadata(const std::string& name,
                            const std::string& description,
                            const std::string& signature,
                            const std::string& signatureWithName,
                            const std::string& sigNoReturn,
                            int tableIndex);

  /// Creates the defaults and last exec tables and places them inside the
  /// table given at tableIndex.
  /// Expects that the parameters are at the top of the stack.
  void createDefaultsAndLastExecTables(int tableIndex, int numParams);

  /// Binds the closure given at closureIndex to the fully qualified name (fq).
  /// In reality, you can use this function to bind any lua value to the
  /// indicated function name.
  void bindClosureTableWithFQName(const std::string& fqName, int closureIndex);

  /// Retrieves the unqualified name given the qualified name.
  static std::string getUnqualifiedName(const std::string& fqName);

  /// Recursive function helper for getAllFuncDescs.
  void getTableFuncDefs(std::vector<LuaScripting::FunctionDesc>& descs) const;

  /// Retrieve the function description from the given table.
  FunctionDesc getFunctionDescFromTable(int table) const;

  /// Lua panic function. Lua calls this when an unrecoverable error occurs
  /// in the interpreter.
  static int luaPanic(lua_State* L);

  /// Customized memory allocator called from within Lua.
  static void* luaInternalAlloc(void* ud, void* ptr, size_t osize,
                                size_t nsize);

  /// Expects the function table to be given at funTableIndex
  /// Copies the defaults table to the last exec table (used for undo/redo).
  void copyDefaultsTableToLastExec(int funTableIndex);

  /// Expects parameters to start at paramStartIndex. The table to receive the
  /// parameters should be at tableIndex.
  /// Do NOT use psuedo indices for tableIndex or paramStartIndex.
  void copyParamsToTable(int tableIndex, int paramStartIndex, int numParams);

  /// Registers our functions with the scripting system.
  void registerScriptFunctions();

  /// Logs info.
  void logInfo(std::string log);

  /// Logs a warning.
  void logWarn(std::string log);

  /// Logs a failure.
  void logError(std::string log);

  /// Prints all currently registered functions using log.info.
  void printFunctions();

  /// Prints all currently registered functions using log.info.
  void printHelp();

  /// Prints help for the given class instance table.
  std::string getClassHelp(int tableIndex);

  /// Prints help about a specific function given by func.
  void infoHelp(LuaTable table);

  /// Retrieves info about the given table as a string.
  std::string getInfoHelp(LuaTable table);

  /// Delete's a lua class instance. Does not delete a class definition.
  /// Class definitions are permanent.
  void deleteLuaClassInstance(LuaClassInstance inst);

  /// Retrieves a new class instance ID. This function is modified by
  /// setNextTempClassInstRange.
  int getNewClassInstID();

  /// Removes all class instances from the system table.
  void deleteAllClassInstances();

  /// Cleans up class constructors stored in mRegisteredClasses.
  void cleanupClassConstructors();

  /// Deletes the class instance in the table. The table should be removed
  /// and no longer called after this function is executable.
  void destroyClassInstanceTable(int tableIndex);

  /// Retrieves the class' unique ID.
  int getClassUniqueID(LuaClassInstance inst);

  /// Retrieves the class with the given Unique ID.
  LuaClassInstance getClassWithUniqueID(LuaClassInstance::IDType ID);

  /// Used when redoing class instance creation. Ensures that the same id is
  /// used when creating the classes. Starts getNewClassInstID at low, and
  /// when current > high, we move to where we were when creating instance IDs.
  /// \param  low   The lowest instance ID to return when using
  ///               getNewClassInstID.
  /// \param  high  The highest instance
  void setNextTempClassInstRange(LuaClassInstance::IDType low,
                                 LuaClassInstance::IDType high);

  /// Returns true if the path specified points to a class instance.
  bool isLuaClassInstance(int tableIndex);

  /// Registers Lua utility functions, such as dir and os.capture.
  void registerLuaUtilityFunctions();

  /// Enable / disable verbose mode for the scripting system.
  void enableVerboseMode(bool enable);

  /// Performs a correction search given the table in which to search, and the
  /// name within the table. If the name is unique, it will be the only value
  /// in the returned vector of strings.
  std::vector<std::string> performCorrection(int tbl,
                                             const std::string& name);

  /// Lua Registry Values
  ///@{
  /// Expected exception flag. Affects LuaStackRAII, and the logging of errors.
  static const char* REG_EXPECTED_EXCEPTION_FLAG;
  ///@}


  /// The one true Lua state.
  lua_State*                        mL;

  /// List of registered modules/functions in Lua's global table.
  /// Used to iterate through all registered functions.
  std::vector<std::string>          mRegisteredGlobals;

  /// List of registered classes. Needed for cleanup purposes.
  std::vector<std::string>          mRegisteredClasses;

  /// Index used to assign a unique ID to classes that wish to register
  /// hooks.
  int                               mMemberHookIndex;

  /// Current global instance ID that will be used to create new Lua classes.
  LuaClassInstance::IDType          mGlobalInstanceID;
  bool                              mGlobalTempInstRange;
  LuaClassInstance::IDType          mGlobalTempInstLow;
  LuaClassInstance::IDType          mGlobalTempInstHigh;
  LuaClassInstance::IDType          mGlobalTempCurrent;

  std::unique_ptr<LuaProvenance>      mProvenance;
  std::unique_ptr<LuaMemberRegUnsafe> mMemberReg;
  std::unique_ptr<LuaClassConstructor>mClassCons;

  bool                              mVerboseMode;

  /// These structures were created in order to handle void return types easily
  ///@{
  template <typename FunPtr, typename Ret>
  struct LuaCallback
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 0, 1); // 1 return value.

      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));

      typename LuaStrictStack<Ret>::Type r;
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
                    lua_touserdata(L, lua_upvalueindex(3)));

        std::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        // Fill execParams. Function parameters start at index 2.
        execParams->pullParamsFromStack(L, 2);

        // Obtain reference to LuaScripting in order to invoke provenance.
        // See createCallableFuncTable for justification on pulling an
        // instance of LuaScripting out of Lua.
        bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

        // We are NOT a hook. Our parameters start at index 2 (because the
        // callable table is at the first index). We will want to call all
        // hooks associated with our table.
        ss->beginCommand();
        try
        {
          r = LuaCFunExec<FunPtr>::run(L, 2, fp);
        }
        catch (std::exception& e)
        {
          ss->endCommand();
          ss->logExecFailure(e.what());
          throw;
        }
        catch (...)
        {
          ss->endCommand();
          ss->logExecFailure("");
          throw;
        }
        ss->endCommand();

        // Call registered hooks.
        // Note: The first parameter on the stack (not on the top, but
        // on the bottom) is the table associated with the function.
        ss->doHooks(L, 1, provExempt);
      }
      else
      {
        // The catching of hook failures is done in doHooks.
        r = LuaCFunExec<FunPtr>::run(L, 1, fp);
      }

      LuaStrictStack<Ret>().push(L, r);
      return 1;
    }
  };

  template <typename FunPtr>
  struct LuaCallback <FunPtr, void>
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 0, 0);

      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
                    lua_touserdata(L, lua_upvalueindex(3)));

        std::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        // Fill execParams. Function parameters start at index 2.
        execParams->pullParamsFromStack(L, 2);

        bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

        ss->beginCommand();
        try
        {
          LuaCFunExec<FunPtr>::run(L, 2, fp);
        }
        catch (std::exception& e)
        {
          ss->logExecFailure(e.what());
          ss->endCommand();
          throw;
        }
        catch (...)
        {
          ss->logExecFailure("");
          ss->endCommand();
          throw;
        }
        ss->endCommand();

        ss->doHooks(L, 1, provExempt);
      }
      else
      {
        // The catching of hook failures is done in doHooks.
        LuaCFunExec<FunPtr>::run(L, 1, fp);
      }

      return 0;
    }
  };
  ///@}

};

} // namespace tuvok

// We need to place this here to resolve a circular reference introduced by the
// functions below.
#include "LuaClassConstructor.h"

namespace tuvok
{

template <typename CLS, typename T, typename FunPtr>
void LuaScripting::registerClass(
    T* t, FunPtr fp, const std::string& className,
    const std::string& desc,
    typename LuaClassRegCallback<CLS>::Type callback)
{
  // Register f as the function _new. This function will be called to construct
  // a pointer to the class we want to create. This function is guaranteed
  // to be a static function.
  // Construct 'constructor' table.
  lua_State* L = getLuaState();
  LuaStackRAII _a(L, 0, 0);

  // Register 'new'. This will automatically create the necessary class table.
  std::string fqFunName = className;
  fqFunName += ".new";
  mClassCons->registerMemberConstructor<CLS>(t, fp, fqFunName, desc, true,
                                             callback);
  setNullUndoFun(fqFunName); // We do not want to exe new when undoing
                             // All child undo items are still executed.

  // Since we did the registerConstructor above we will have our function
  // table present.
  if (getFunctionTable(className) == false)
    throw LuaFunBindError("Unable to find table we just built!");

  // Pop function table off stack.
  lua_pop(L, 1);

  // Populate the 'new' function's table with the class definition function,
  // and the full factory name.
  std::ostringstream retNewFun;
  retNewFun << "return " << className << ".new";
  luaL_dostring(mL, retNewFun.str().c_str());

  lua_pushstring(mL, className.c_str());
  lua_setfield(mL, -2, LuaClassConstructor::CONS_MD_FACTORY_NAME);

  // Pop the new function table.
  lua_pop(mL, 1);

  // Store the class in the 'registered class' stack.
  // This stack lets us get rid of the callback pointers stored inside of the
  // 'new' function in the class constructor.
  mRegisteredClasses.push_back(className);
}

template <typename CLS, typename FunPtr>
void LuaScripting::registerClassStatic(
    FunPtr fp, const std::string& className,
    const std::string& desc,
    typename LuaClassRegCallback<CLS>::Type callback)
{
  // Register f as the function _new. This function will be called to construct
  // a pointer to the class we want to create. This function is guaranteed
  // to be a static function.
  // Construct 'constructor' table.
  lua_State* L = getLuaState();
  LuaStackRAII _a(L, 0, 0);

  // Register 'new'. This will automatically create the necessary class table.
  std::string fqFunName = className;
  fqFunName += ".new";
  mClassCons->registerConstructor<CLS>(fp, fqFunName, desc, true, callback);
  setNullUndoFun(fqFunName); // We do not want to exe new when undoing
                             // All child undo items are still executed.

  // Since we did the registerConstructor above we will have our function
  // table present.
  if (getFunctionTable(className) == false)
    throw LuaFunBindError("Unable to find table we just built!");

  // Pop function table off stack.
  lua_pop(L, 1);

  // Populate the 'new' function's table with the class definition function,
  // and the full factory name.
  std::ostringstream retNewFun;
  retNewFun << "return " << className << ".new";
  luaL_dostring(mL, retNewFun.str().c_str());

  lua_pushstring(mL, className.c_str());
  lua_setfield(mL, -2, LuaClassConstructor::CONS_MD_FACTORY_NAME);

  // Pop the new function table.
  lua_pop(mL, 1);

  // Store the class in the 'registered class' stack.
  // This stack lets us get rid of the callback pointers stored inside of the
  // 'new' function in the class constructor.
  mRegisteredClasses.push_back(className);
}

template <typename T>
T LuaScripting::execRet(const std::string& cmd)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0, 0);

  std::string retCmd = "return " + cmd;
  luaL_loadstring(mL, retCmd.c_str());
  lua_call(mL, 0, LUA_MULTRET);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}

template <typename T>
T LuaScripting::cexecRet(const std::string& name)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0, 0);
  prepForExecution(name);
  executeFunctionOnStack(0, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS

template <typename T>
void Tuvok_luaCheckParam(lua_State* L, const std::string& name,
                         int typesTable, int check_pos)
{
  LuaStackRAII _a = LuaStackRAII(L, 0, 0);

  lua_pushinteger(L, check_pos++);
  lua_gettable(L, typesTable);
  if (LSS_compareToTypeOnStack<T>(L, -1) == false)
  {
    // Test the first special case (const char* --> std::string)
    if ((   LSS_compareToTypeOnStack<std::string>(L, -1)
         || LSS_compareToTypeOnStack<const char*>(L, -1)))
    {
      if (   LSS_compareTypes<std::string, T>()
          || LSS_compareTypes<const char*, T>())
      {
        lua_pop(L, 1);
        return;
      }
    }

    // Test another special case (const wchar_t* --> std::wstring)
    if ((   LSS_compareToTypeOnStack<std::wstring>(L, -1)
         || LSS_compareToTypeOnStack<const wchar_t*>(L, -1)))
    {
      if (   LSS_compareTypes<std::wstring, T>()
          || LSS_compareTypes<const wchar_t*, T>())
      {
        lua_pop(L, 1);
        return;
      }
    }

    if (    LSS_compareToTypeOnStack<float>(L, -1)
         || LSS_compareToTypeOnStack<double>(L, -1)
         )
    {
      if (   LSS_compareTypes<float, T>()
          || LSS_compareTypes<double, T>()
          )
      {
        lua_pop(L, 1);
        return;
      }
    }

    if (     LSS_compareToTypeOnStack<int>(L, -1)
        ||   LSS_compareToTypeOnStack<unsigned int>(L, -1)
        )
    {
      if (   LSS_compareTypes<int, T>()
          || LSS_compareTypes<unsigned int, T>()
          )
      {
        lua_pop(L, 1);
        return;
      }
    }

    std::ostringstream os;
    os << "Invalid argument at position " << check_pos;
    os << " for call to function " << name;
    throw LuaInvalidType(os.str().c_str());
  }
  lua_pop(L, 1);
}

#endif

// Include cexec/cexecRet/setDefaults function bodies
#include "LuaScriptingExecBody.h"

template <typename FunPtr>
std::string LuaScripting::registerFunction(FunPtr f, const std::string& name,
                                           const std::string& desc,
                                           bool undoRedo)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0, 0);

  // Idea: Build a 'callable' table.
  // Its metatable will have a __call metamethod that points to the C
  // function closure.

  // We do this because all metatables are unique per-type which eliminates
  // the possibilty of using a metatable on the function closure itself.
  // The only exception to this rule is the table type itself.
  
  // Create a callable function table and leave it on the stack.
  lua_CFunction proxyFunc = &LuaCallback<FunPtr, typename
      LuaCFunExec<FunPtr>::returnType>::exec;
  createCallableFuncTable(proxyFunc, reinterpret_cast<void*>(f));

  int tableIndex = lua_gettop(mL);

  // Add function metadata to the table.
  std::string sig = LuaCFunExec<FunPtr>::getSignature("");
  std::string sigWithName = LuaCFunExec<FunPtr>::getSignature(
      getUnqualifiedName(name));
  std::string sigNoRet = LuaCFunExec<FunPtr>::getSigNoReturn("");
  populateWithMetadata(name, desc, sig, sigWithName, sigNoRet, tableIndex);

  // Push default values for function parameters onto the stack.
  LuaCFunExec<FunPtr> defaultParams = LuaCFunExec<FunPtr>();
  lua_checkstack(mL, LUAC_MAX_NUM_PARAMS + 2);
  defaultParams.pushParamsToStack(mL);
  int numFunParams = lua_gettop(mL) - tableIndex;
  createDefaultsAndLastExecTables(tableIndex, numFunParams);
  lua_pushinteger(mL, numFunParams);
  lua_setfield(mL, tableIndex, TBL_MD_NUM_PARAMS);

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  // Generate the type table (buildTypeTable places table on top of the stack).
  LuaCFunExec<FunPtr>::buildTypeTable(mL);
  lua_setfield(mL, tableIndex, TBL_MD_TYPES_TABLE);
#endif

  // Install the callable table in the appropriate module based on its
  // fully qualified name.
  bindClosureTableWithFQName(name, tableIndex);

  lua_pop(mL, 1);   // Pop the callable table.

  if (undoRedo == false)  setUndoRedoStackExempt(name);

  return name;
}

template <typename FunPtr>
void LuaScripting::strictHookInternal(
    FunPtr f,
    const std::string& name,
    bool registerUndo,
    bool registerRedo)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0, 0);

  // Need to check the signature of the function that we are trying to bind
  // into the script system.
  if (getFunctionTable(name) == false)
  {
    throw LuaNonExistantFunction("Unable to find function with which to"
                                 "associate a hook.");
  }

  int funcTable = lua_gettop(mL);

  // Check function signatures.
  lua_getfield(mL, funcTable, TBL_MD_SIG_NO_RET);
  std::string sigReg = lua_tostring(mL, -1);
  std::string sigHook = LuaCFunExec<FunPtr>::getSigNoReturn("");
  if (sigReg.compare(sigHook) != 0)
  {
    std::ostringstream os;
    os << "Hook's parameter signature and the parameter signature of the "
          "function to hook must match. Hook's signature: \"" << sigHook <<
          "\"Function to hook's signature: " << sigReg;
    throw LuaInvalidFunSignature(os.str().c_str());
  }
  lua_pop(mL, 1);

  // Obtain hook table.
  lua_getfield(mL, -1, TBL_MD_HOOKS);
  int hookTable = lua_gettop(mL);

  bool regUndoRedo = (registerUndo || registerRedo);
  std::ostringstream os;
  if (!regUndoRedo)
  {
    // Generate a hook descriptor (we make it a name as opposed to a natural
    // number because we want Lua to use the hash table implementation behind
    // the scenes, not the array table).
    lua_getfield(mL, funcTable, TBL_MD_HOOK_INDEX);
    int hookIndex = lua_tointeger(mL, -1);
    os << "h" << hookIndex;
    lua_pop(mL, 1);
    lua_pushinteger(mL, hookIndex + 1);
    lua_setfield(mL, funcTable, TBL_MD_HOOK_INDEX);
  }

  // Push closure
  lua_CFunction proxyFunc = &LuaCallback<FunPtr, typename
          LuaCFunExec<FunPtr>::returnType>::exec;
  lua_pushlightuserdata(mL, reinterpret_cast<void*>(f));
  lua_pushboolean(mL, 1);  // We ARE a hook. This affects the stack, and
                           // whether we want to perform provenance.
  // As a hook, we don't need to add mScriptSystem to the closure, since
  // we don't perform provenance on hooks.
  lua_pushcclosure(mL, proxyFunc, 2);

  if (!regUndoRedo)
  {
    // Associate closure with hook table.
    lua_setfield(mL, hookTable, os.str().c_str());
  }
  else
  {
    std::string fieldToQuery = LuaScripting::TBL_MD_UNDO_FUNC;
    bool isUndo = true;

    if (registerRedo)
    {
      fieldToQuery = LuaScripting::TBL_MD_REDO_FUNC;
      isUndo = false;
    }

    lua_getfield(mL, funcTable, fieldToQuery.c_str());

    if (lua_isnil(mL, -1) == 0)
    {
      if (isUndo) throw LuaUndoFuncAlreadySet("Undo function already set.");
      else        throw LuaRedoFuncAlreadySet("Redo function already set.");
    }

    lua_pop(mL, 1);
    lua_setfield(mL, funcTable, fieldToQuery.c_str());
  }

  lua_pop(mL, 2);  // Remove the function table and the hooks table.
}

template <typename FunPtr>
void LuaScripting::strictHook(FunPtr f, const std::string& name)
{
  strictHookInternal(f, name, false, false);
}

template <typename FunPtr>
void LuaScripting::setUndoFun(FunPtr f, const std::string& name)
{
  // Uses strict hook.
  strictHookInternal(f, name, true, false);
}

template <typename FunPtr>
void LuaScripting::setRedoFun(FunPtr f, const std::string& name)
{
  // Uses strict hook.
  strictHookInternal(f, name, false, true);
}

} /* namespace tuvok */
#endif /* LUASCRIPTING_H_ */
