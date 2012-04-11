
#include <errno.h>
#include <fxkeys.h>

#include "fx_console.h"
#include "gsl_shell_app.h"
#include "gsl_shell_thread.h"
#include "lua_plot_window.h"
#include "fx_plot_window.h"

FXDEFMAP(fx_console) fx_console_map[]={
  FXMAPFUNC(SEL_KEYPRESS, 0, fx_console::on_key_press),
  FXMAPFUNC(SEL_IO_READ, fx_console::ID_LUA_OUTPUT, fx_console::on_lua_output),
};

FXIMPLEMENT(fx_console,FXText,fx_console_map,ARRAYNUMBER(fx_console_map))

char const * const fx_console::prompt = "> ";

fx_console::fx_console(FXComposite *p, FXObject* tgt, FXSelector sel, FXuint opts, FXint x, FXint y, FXint w, FXint h, FXint pl, FXint pr, FXint pt, FXint pb):
  FXText(p, tgt, sel, opts, x, y, w, h, pl, pr, pt, pb),
  m_status(not_ready), m_engine()
{
  FXApp* app = getApp();
  m_lua_io_signal = new FXGUISignal(app, this, ID_LUA_OUTPUT);
  m_lua_io_thread = new lua_io_thread(&m_engine, m_lua_io_signal, &m_lua_io_mutex, &m_lua_io_buffer);
}

fx_console::~fx_console()
{
  delete m_lua_io_thread;
  delete m_lua_io_signal;
}

void fx_console::prepare_input()
{
  appendText(prompt, strlen(prompt));
  m_status = input_mode;
  m_input_begin = getCursorPos();
}

void fx_console::show_errors()
{
  if (m_engine.eval_status() == gsl_shell::eval_error)
    {
      appendText("Error reported: ");
      appendText(m_engine.error_msg());
      appendText("\n");
    }
}

static int
lua_fox_init(lua_State* L, void* _app)
{
  lua_pushlightuserdata(L, _app);
  lua_setfield(L, LUA_REGISTRYINDEX, "__fox_app");

  fox_window_register(L);
  return 0;
}

void fx_console::create()
{
  FXText::create();
  init("Welcome to GSL Shell 2.1\n");
  setFocus();
  m_engine.set_init_func(lua_fox_init, (void*) getApp());
  m_engine.start();
  m_lua_io_thread->start();
}

void fx_console::init(const FXString& greeting)
{
  appendText(greeting);
  prepare_input();
}

long fx_console::on_key_press(FXObject* obj, FXSelector sel, void* ptr)
{
  FXEvent* event = (FXEvent*)ptr;
  if (event->code == KEY_Return && m_status == input_mode)
    {
      FXint pos = getCursorPos();
      FXint line_end = lineEnd(pos), line_start = m_input_begin;
      extractText(m_input, line_start, line_end - line_start);
      appendText("\n");

      this->m_status = output_mode;
      m_engine.input(m_input.text());

      return 1;
    }

  return FXText::onKeyPress(obj, sel, ptr);
}

long fx_console::on_lua_output(FXObject* obj, FXSelector sel, void* ptr)
{
  bool eot = false;

  m_lua_io_mutex.lock();
  FXint len = m_lua_io_buffer.length();
  if (len > 0)
    {
      if (m_lua_io_buffer[len-1] == gsl_shell_thread::eot_character)
	{
	  eot = true;
	  m_lua_io_buffer.trunc(len-1);
	}
    }
  appendText(m_lua_io_buffer);
  m_lua_io_buffer.clear();
  m_lua_io_mutex.unlock();

  if (eot)
    {
      int status = m_engine.eval_status();

      if (status == gsl_shell::incomplete_input)
	{
	  m_status = input_mode;
	}
      else if (status == gsl_shell::exit_request)
	{
	  FXApp* app = getApp();
	  app->handle(this, FXSEL(SEL_COMMAND,FXApp::ID_QUIT), NULL);
	}
      else
	{
	  show_errors();
	  prepare_input();
	}
    }

  return 1;
}

FXint lua_io_thread::run()
{
  char buffer[128];

  while (1)
    {
      int nr = m_engine->read(buffer, 127);
      if (nr < 0)
	{
	  fprintf(stderr, "ERROR on read: %d.\n", errno);
	  break;
	}
      if (nr == 0)
	break;

      buffer[nr] = 0;

      m_io_protect->lock();
      m_io_buffer->append((const FXchar*)buffer);
      m_io_protect->unlock();

      m_io_ready->signal();
    }

  return 0;
}
