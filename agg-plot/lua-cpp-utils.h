#ifndef LUA_CPP_UTILS_H
#define LUA_CPP_UTILS_H

#include <exception>
#include <new>

#include "defs.h"
#include "lua-defs.h"
__BEGIN_DECLS
#include "lua.h"
__END_DECLS

#include "gs-types.h"

inline void* operator new(size_t nbytes, lua_State *L, enum gs_type_e tp)
{
  void* p = lua_newuserdata(L, nbytes);
  gs_set_metatable (L, tp);
  return p;
}

template <class T>
T* push_new_object (lua_State *L, enum gs_type_e tp)
{
  try
    {
      return new(L, tp) T();
    }
  catch (std::bad_alloc&)
    {
      luaL_error (L, "out of memory");
    }

  return 0;
}

template <class T, class init_type>
T* push_new_object (lua_State *L, enum gs_type_e tp, init_type& init)
{
  try
    {
      return new(L, tp) T(init);
    }
  catch (std::bad_alloc&)
    {
      luaL_error (L, "out of memory");
     }

  return 0;
}

template <class T>
int object_free (lua_State *L, int index, enum gs_type_e tp)
{
  T *obj = (T *) gs_check_userdata (L, index, tp);
  obj->~T();
  return 0;
}

template <class T>
T* object_check (lua_State *L, int index, enum gs_type_e tp)
{
  return (T *) gs_check_userdata (L, index, tp);
}

#endif
