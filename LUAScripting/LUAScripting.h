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
 \file    LUAScripting.h
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

#ifndef EXTERNAL_UNIT_TESTING
#include "3rdParty/LUA/lua.hpp"
#include <assert.h>

#include <iostream>
#include <tr1/memory>
#include <string>
#endif

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAStackRAII.h"

namespace tuvok
{

class LuaProvenance;
class LuaMemberRegUnsafe;

class LuaScripting
{
  friend class LuaMemberRegUnsafe;  // For getNewMemberHookID.
  friend class LuaProvenance;       // For obtaining function tables.
  friend class LuaStackRAII;        // For unwinding lua stack during exception
  friend class LuaClassInstanceHook;// For getNewMemberHookID.
public:

  LuaScripting();
  virtual ~LuaScripting();

  /// Registers a static C++ function with LUA.
  /// Since Lua is compiled as CPP, it is safe to throw exceptions from the
  /// function pointed to by f (since Lua detects that it is being compiled in
  /// cpp, and uses exceptions instead of long jumping).
  /// \param  f         Any function pointer.
  ///                   f's parameters and return value will be handled
  ///                   automatically.
  ///                   The number of parameters allowed in f is limited by
  ///                   the templates in LUAFunBinding.h.
  /// \param  name      Period delimited fully qualified name of f inside of LUA
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
  /// NOTES: The system is re-entrant, so you may call lua from within
  /// registered functions, or even call other registered functions.
  ///
  /// TO REGISTER MEMBER FUNCTIONS:
  /// Use the LUAMemberReg mediator class. It will clean up for you.
  /// functions.
  template <typename FunPtr>
  void registerFunction(FunPtr f,
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

//  /// Lua Class Instance Construction
//  ///
//  /// Use this method to begin constructing a class.
//  /// Add functions to the class using the returned LuaInstanceReg instance.
//  ///
//  /// The static constructor function pointer should construct and return an
//  /// instance of the class <T>.
//  /// This constructor will be bound into Lua at <fqName>.new . Also, the
//  /// table at <fqName> will be made executable, and will call this
//  /// constructor function.
//  template <typename T, typename FunPtr>
//  LuaClassInstanceReg constructClass(const std::string& fqName,
//                                     FunPtr constructor,
//                                     const std::string& classDesc);


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
  /// These function are faster compared to the exec functions given above.
  ///
  /// Example: exec("myFunc", a, b, c, d, ...)
  ///@{
  void cexec(const std::string& cmd);

  template <typename P1>
  void cexec(const std::string& cmd, P1 p1);

  template <typename P1, typename P2>
  void cexec(const std::string& cmd, P1 p1, P2 p2);

  template <typename P1, typename P2, typename P3>
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3);

  template <typename P1, typename P2, typename P3, typename P4>
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4);

  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5);

  template <typename P1, typename P2, typename P3, typename P4, typename P5,
            typename P6>
  void cexec(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6);
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

  template <typename T, typename P1>
  T cexecRet(const std::string& cmd, P1 p1);

  template <typename T, typename P1, typename P2>
  T cexecRet(const std::string& cmd, P1 p1, P2 p2);

  template <typename T, typename P1, typename P2, typename P3>
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3);

  template <typename T, typename P1, typename P2, typename P3, typename P4>
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4);

  template <typename T, typename P1, typename P2, typename P3, typename P4,
            typename P5>
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5);

  template <typename T, typename P1, typename P2, typename P3, typename P4,
            typename P5, typename P6>
  T cexecRet(const std::string& cmd, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6);
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
  ///@{
  template <typename P1>
  void setDefaults(const std::string& name,
                   P1 p1, bool call);

  template <typename P1, typename P2>
  void setDefaults(const std::string& name,
                   P1 p1, P2 p2, bool call);

  template <typename P1, typename P2, typename P3>
  void setDefaults(const std::string& name,
                   P1 p1, P2 p2, P3 p3, bool call);

  template <typename P1, typename P2, typename P3, typename P4>
  void setDefaults(const std::string& name,
                   P1 p1, P2 p2, P3 p3, P4 p4, bool call);

  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  void setDefaults(const std::string& name,
                   P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, bool call);

  template <typename P1, typename P2, typename P3, typename P4, typename P5,
            typename P6>
  void setDefaults(const std::string& name,
                   P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, bool call);
  ///@}

  /// Default: Provenance is enabled. Disabling provenance will disable undo/
  /// redo.
  bool isProvenanceEnabled();
  void enableProvenance(bool enable);

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

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  static const char* TBL_MD_TYPES_TABLE;  ///< type_info userdata table.
#endif

  /// Sets a flag in the Lua registry indicating that an exception is expected
  /// that will cause the lua stack to be unbalanced in internal functions.
  /// This is to avoid debug log output, more than anything else.
  void setExpectedExceptionFlag(bool expected);

private:

  /// Used by friend class LuaProvenance.
  lua_State* getLUAState()  {return mL;}

  /// This function should be used sparingly, and only for those functions that
  /// do not modify state nor return internal state in some way.
  /// I could not think of any functions outside the purview of LuaScripting
  /// that would need to be provenance exempt, so it will remain private for
  /// now.
  /// E.G. Debug logging functions should be provenance exempt.
  void setProvenanceExempt(const std::string& fqName);

  /// This function exists because mProvenance is an incomplete type when this
  /// header is being compiled. It just routes to
  /// mProvenance->setDisableProvTemporarily(...)
  void setTempProvDisable(bool disable);

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

  /// Stack expectations: the function table at tableIndex, and the parameters
  /// required to call the function directly after the table on the stack.
  /// There must be no other values on the stack above tableIndex other than the
  /// table and the parameters to call the function.
  void doHooks(lua_State* L, int tableIndex, bool provExempt);

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
  bool getFunctionTable(const std::string& fqName);

  /// Creates a callable LUA table. classInstance can be NULL.
  /// Leaves the table on the top of the LUA stack.
  void createCallableFuncTable(lua_CFunction proxyFunc,
                               void* realFuncToCall);

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
  void bindClosureTableWithFQName(const std::string& fqName, int closureIndex);

  /// Retrieves the unqualified name given the qualified name.
  static std::string getUnqualifiedName(const std::string& fqName);

  /// Recursive function helper for getAllFuncDescs.
  void getTableFuncDefs(std::vector<LuaScripting::FunctionDesc>& descs) const;

  /// LUA panic function. LUA calls this when an unrecoverable error occurs
  /// in the interpreter.
  static int luaPanic(lua_State* L);

  /// Customized memory allocator called from within LUA.
  static void* luaInternalAlloc(void* ud, void* ptr, size_t osize,
                                size_t nsize);

  /// Returns true if the function is provenance exempt.
  /// Used to tell whether or not we should log hooks later on.
  bool doProvenanceFromExec(lua_State* L,
                            std::tr1::shared_ptr<LuaCFunAbstract> funParams,
                            std::tr1::shared_ptr<LuaCFunAbstract> emptyParams);

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

  /// Exec failure.
  void logExecFailure(const std::string& failure);

  /// Prints all currently registered functions using log.info.
  void printFunctions();

  /// Prints all currently registered functions using log.info.
  void printHelp();

  /// Just calls provenance begin/end command.
  void beginCommand();
  void endCommand();

  /// Lua Registry Values
  ///@{
  /// Expected exception flag. Affects LuaStackRAII, and the logging of errors.
  static const char* REG_EXPECTED_EXCEPTION_FLAG;
  ///@}


  /// The one true LUA state.
  lua_State*                        mL;

  /// List of registered modules/functions in LUA's global table.
  /// Used to iterate through all registered functions.
  std::vector<std::string>          mRegisteredGlobals;

  /// Index used to assign a unique ID to classes that wish to register
  /// hooks.
  int                               mMemberHookIndex;

  std::auto_ptr<LuaProvenance>      mProvenance;
  std::auto_ptr<LuaMemberRegUnsafe> mMemberReg;

  /// These structures were created in order to handle void return types easily
  ///@{
  template <typename FunPtr, typename Ret>
  struct LuaCallback
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 1); // 1 return value.

      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));

      Ret r;
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
                    lua_touserdata(L, lua_upvalueindex(3)));

        std::tr1::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
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
      LuaStackRAII _a = LuaStackRAII(L, 0);

      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
                    lua_touserdata(L, lua_upvalueindex(3)));

        std::tr1::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
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


template <typename T>
T LuaScripting::execRet(const std::string& cmd)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  std::string retCmd = "return " + cmd;
  luaL_loadstring(mL, retCmd.c_str());
  lua_call(mL, 0, LUA_MULTRET);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}


#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS

template <typename T>
void Tuvok_luaCheckParam(lua_State* L, const std::string& name,
                         int typesTable, int check_pos)
{
  LuaStackRAII _a = LuaStackRAII(L, 0);

  lua_pushinteger(L, check_pos++);
  lua_gettable(L, typesTable);
  if (LSS_compareToTypeOnStack<T>(L, -1) == false)
  {
    // Test the one special case (const char* --> std::string)
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

    std::ostringstream os;
    os << "Invalid argument at position " << check_pos;
    os << " for call to function " << name;
    throw LuaInvalidType(os.str().c_str());
  }
  lua_pop(L, 1);
}

#endif


template <typename P1>
void LuaScripting::cexec(const std::string& name, P1 p1)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 1) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  executeFunctionOnStack(1, 0);
}
template <typename P1, typename P2>
void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 2) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  executeFunctionOnStack(2, 0);
}
template <typename P1, typename P2, typename P3>
void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 3) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  executeFunctionOnStack(3, 0);
}
template <typename P1, typename P2, typename P3, typename P4>
void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 4) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  executeFunctionOnStack(4, 0);
}
template <typename P1, typename P2, typename P3, typename P4, typename P5>
void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4,
                         P5 p5)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 5) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  LuaStrictStack<P5>::push(mL, p5);
  executeFunctionOnStack(5, 0);
}
template <typename P1, typename P2, typename P3, typename P4, typename P5,
          typename P6>
void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4,
                         P5 p5, P6 p6)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 6) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P6>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  LuaStrictStack<P5>::push(mL, p5);
  LuaStrictStack<P6>::push(mL, p6);
  executeFunctionOnStack(6, 0);
}

template <typename T>
T LuaScripting::cexecRet(const std::string& name)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
  executeFunctionOnStack(0, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1>
T LuaScripting::cexecRet(const std::string& name, P1 p1)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 1) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  executeFunctionOnStack(1, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1, typename P2>
T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 2) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  executeFunctionOnStack(2, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1, typename P2, typename P3>
T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 3) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  executeFunctionOnStack(3, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1, typename P2, typename P3, typename P4>
T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 4) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  executeFunctionOnStack(4, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1, typename P2, typename P3, typename P4,
          typename P5>
T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4,
                         P5 p5)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 5) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  LuaStrictStack<P5>::push(mL, p5);
  executeFunctionOnStack(5, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}
template <typename T, typename P1, typename P2, typename P3, typename P4,
          typename P5, typename P6>
T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4,
                         P5 p5, P6 p6)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(name);
#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  int ftable = lua_gettop(mL);
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 6) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P6>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif
  LuaStrictStack<P1>::push(mL, p1);
  LuaStrictStack<P2>::push(mL, p2);
  LuaStrictStack<P3>::push(mL, p3);
  LuaStrictStack<P4>::push(mL, p4);
  LuaStrictStack<P5>::push(mL, p5);
  LuaStrictStack<P6>::push(mL, p6);
  executeFunctionOnStack(6, 1);
  T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
  lua_pop(mL, 1); // Pop return value.
  return ret;
}


template <typename P1>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 1) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1);
  setTempProvDisable(false);
}

template <typename P1, typename P2>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, P2 p2, bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 2) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P2>::push(mL, p2);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1, p2);
  setTempProvDisable(false);
}

template <typename P1, typename P2, typename P3>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, P2 p2, P3 p3, bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 3) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P2>::push(mL, p2);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P3>::push(mL, p3);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1, p2, p3);
  setTempProvDisable(false);
}

template <typename P1, typename P2, typename P3, typename P4>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, P2 p2, P3 p3, P4 p4, bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 4) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P2>::push(mL, p2);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P3>::push(mL, p3);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P4>::push(mL, p4);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1, p2, p3, p4);
  setTempProvDisable(false);
}

template <typename P1, typename P2, typename P3, typename P4, typename P5>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 5) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P2>::push(mL, p2);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P3>::push(mL, p3);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P4>::push(mL, p4);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P5>::push(mL, p5);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1, p2, p3, p4, p5);
  setTempProvDisable(false);
}

template <typename P1, typename P2, typename P3, typename P4, typename P5,
          typename P6>
void LuaScripting::setDefaults(const std::string& name,
                               P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6,
                               bool call)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  getFunctionTable(name);
  int ftable = lua_gettop(mL);
  int pos = 0;

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
  if (lua_tointeger(mL, -1) != 6) throw LuaUnequalNumParams("Unequal params");
  lua_pop(mL, 1);

  lua_getfield(mL, ftable, LuaScripting::TBL_MD_TYPES_TABLE);
  int ttable = lua_gettop(mL);
  int check_pos = 0;
  Tuvok_luaCheckParam<P1>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P2>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P3>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P4>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P5>(mL, name, ttable, check_pos++);
  Tuvok_luaCheckParam<P6>(mL, name, ttable, check_pos++);
  lua_pop(mL, 1);
#endif

  LuaStrictStack<P1>::push(mL, p1);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P2>::push(mL, p2);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P3>::push(mL, p3);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P4>::push(mL, p4);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P5>::push(mL, p5);
  resetFunDefault(pos++, ftable);
  LuaStrictStack<P6>::push(mL, p6);
  resetFunDefault(pos++, ftable);

  lua_pop(mL, 1); // Remove function table.

  setTempProvDisable(true);
  if (call) cexec(name, p1, p2, p3, p4, p5, p6);
  setTempProvDisable(false);
}

template <typename FunPtr>
void LuaScripting::registerFunction(FunPtr f, const std::string& name,
                                    const std::string& desc, bool undoRedo)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Idea: Build a 'callable' table.
  // Its metatable will have a __call metamethod that points to the C
  // function closure.

  // We do this because all metatables are unique per-type which eliminates
  // the possibilty of using a metatable on the function closure itself.
  // The only exception to this rule is the table type itself.
  int initStackTop = lua_gettop(mL);

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

  assert(initStackTop == lua_gettop(mL));
}

template <typename FunPtr>
void LuaScripting::strictHook(FunPtr f, const std::string& name)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

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

  // Generate a hook descriptor (we make it a name as opposed to a natural
  // number because we want Lua to use the hash table implementation behind
  // the scenes, not the array table).
  lua_getfield(mL, funcTable, TBL_MD_HOOK_INDEX);
  std::ostringstream os;
  int hookIndex = lua_tointeger(mL, -1);
  os << "h" << hookIndex;
  lua_pop(mL, 1);
  lua_pushinteger(mL, hookIndex + 1);
  lua_setfield(mL, funcTable, TBL_MD_HOOK_INDEX);

  // Push closure
  lua_CFunction proxyFunc = &LuaCallback<FunPtr, typename
          LuaCFunExec<FunPtr>::returnType>::exec;
  lua_pushlightuserdata(mL, reinterpret_cast<void*>(f));
  lua_pushboolean(mL, 1);  // We ARE a hook. This affects the stack, and
                           // whether we want to perform provenance.
  // As a hook, we don't need to add mScriptSystem to the closure, since
  // we don't perform provenance on hooks.
  lua_pushcclosure(mL, proxyFunc, 2);

  // Associate closure with hook table.
  lua_setfield(mL, hookTable, os.str().c_str());

  lua_pop(mL, 2);  // Remove the function table and the hooks table.
}

} /* namespace tuvok */
#endif /* LUASCRIPTING_H_ */
