#ifndef AGG_PARSE_TRANS_H
#define AGG_PARSE_TRANS_H

extern "C" {
#include "lua.h"
}

#include "agg_color_rgba.h"

#include "scalable.h"
#include "drawable.h"

class agg_spec_error {
public:
  enum err_e {
    invalid_tag = 0,
    invalid_spec,
    invalid_object,
    generic_error
  };
  
  agg_spec_error(enum err_e err) : m_code(err) {};
  agg_spec_error() : m_code(generic_error) {};

  const char * message() const { return m_msg[(int) m_code]; };

private:
  err_e m_code;

  static const char *m_msg[];
};

extern drawable * parse_graph_args (lua_State *L, agg::rgba8& color);

#endif
