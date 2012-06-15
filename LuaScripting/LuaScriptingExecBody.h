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

// This header file is automatically generated.
// Contains definitions for executing bound Lua functions.
 
#ifndef TUVOK_LUAEXECBODY_H_
#define TUVOK_LUAEXECBODY_H_


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
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
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
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
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
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 7) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    executeFunctionOnStack(7, 0);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 8) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    executeFunctionOnStack(8, 0);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 9) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    LuaStrictStack<P9>::push(mL, p9);
    executeFunctionOnStack(9, 0);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
  void LuaScripting::cexec(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 10) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P10>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    LuaStrictStack<P9>::push(mL, p9);
    LuaStrictStack<P10>::push(mL, p10);
    executeFunctionOnStack(10, 0);
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
    lua_pop(mL, 1);
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
    lua_pop(mL, 1);
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
    lua_pop(mL, 1);
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
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
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
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
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
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 7) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    executeFunctionOnStack(7, 1);
    T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 8) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    executeFunctionOnStack(8, 1);
    T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 9) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    LuaStrictStack<P9>::push(mL, p9);
    executeFunctionOnStack(9, 1);
    T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
    lua_pop(mL, 1);
    return ret;
  }
  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
  T LuaScripting::cexecRet(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    prepForExecution(name);
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    int ftable = lua_gettop(mL);
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 10) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P10>(mL, name, ttable, check_pos++);
    lua_pop(mL, 1);
  #endif
    LuaStrictStack<P1>::push(mL, p1);
    LuaStrictStack<P2>::push(mL, p2);
    LuaStrictStack<P3>::push(mL, p3);
    LuaStrictStack<P4>::push(mL, p4);
    LuaStrictStack<P5>::push(mL, p5);
    LuaStrictStack<P6>::push(mL, p6);
    LuaStrictStack<P7>::push(mL, p7);
    LuaStrictStack<P8>::push(mL, p8);
    LuaStrictStack<P9>::push(mL, p9);
    LuaStrictStack<P10>::push(mL, p10);
    executeFunctionOnStack(10, 1);
    T ret = LuaStrictStack<T>::get(mL, lua_gettop(mL));
    lua_pop(mL, 1);
    return ret;
  }
  
  template <typename P1>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
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
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5, p6);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
    int ftable = lua_gettop(mL);
    int pos = 0;
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 7) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
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
    LuaStrictStack<P7>::push(mL, p7);
    resetFunDefault(pos++, ftable);
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5, p6, p7);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
    int ftable = lua_gettop(mL);
    int pos = 0;
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 8) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
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
    LuaStrictStack<P7>::push(mL, p7);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P8>::push(mL, p8);
    resetFunDefault(pos++, ftable);
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5, p6, p7, p8);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
    int ftable = lua_gettop(mL);
    int pos = 0;
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 9) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
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
    LuaStrictStack<P7>::push(mL, p7);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P8>::push(mL, p8);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P9>::push(mL, p9);
    resetFunDefault(pos++, ftable);
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5, p6, p7, p8, p9);
    setTempProvDisable(false);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
  void LuaScripting::setDefaults(const std::string& name, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10, bool call)
  {
    LuaStackRAII _a = LuaStackRAII(mL, 0);
    if (getFunctionTable(name) == false)
      throw LuaNonExistantFunction("Can't find function");
    int ftable = lua_gettop(mL);
    int pos = 0;
  #ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    lua_getfield(mL, ftable, TBL_MD_NUM_PARAMS);
    if (lua_tointeger(mL, -1) != 10) throw LuaUnequalNumParams("Unequal params");
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
    Tuvok_luaCheckParam<P7>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P8>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P9>(mL, name, ttable, check_pos++);
    Tuvok_luaCheckParam<P10>(mL, name, ttable, check_pos++);
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
    LuaStrictStack<P7>::push(mL, p7);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P8>::push(mL, p8);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P9>::push(mL, p9);
    resetFunDefault(pos++, ftable);
    LuaStrictStack<P10>::push(mL, p10);
    resetFunDefault(pos++, ftable);
    lua_pop(mL, 1);
    setTempProvDisable(true);
    if (call) cexec(name, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
    setTempProvDisable(false);
  }
  
#endif

