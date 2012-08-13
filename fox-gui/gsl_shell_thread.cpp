#include <pthread.h>
#include <stdio.h>

#include "gsl_shell_thread.h"

extern "C" void * luajit_eval_thread (void *userdata);

void *
luajit_eval_thread (void *userdata)
{
    gsl_shell_thread* eng = (gsl_shell_thread*) userdata;
    eng->lock();
    eng->init();
    eng->run();
    pthread_exit(NULL);
    return NULL;
}

gsl_shell_thread::gsl_shell_thread():
    m_status(starting), m_redirect(4096), m_request(no_request)
{
}

gsl_shell_thread::~gsl_shell_thread()
{
    m_redirect.stop();
}

void gsl_shell_thread::start()
{
    m_redirect.start();

    pthread_attr_t attr[1];

    pthread_attr_init (attr);
    pthread_attr_setdetachstate (attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create (&m_thread, attr, luajit_eval_thread, (void*)this))
    {
        fprintf(stderr, "error creating thread");
    }
}

gsl_shell_thread::thread_cmd_e
gsl_shell_thread::process_request()
{
    thread_cmd_e cmd;

    if (m_request == gsl_shell_thread::exit_request)
    {
        cmd = thread_cmd_exit;
    }
    else if (m_request == gsl_shell_thread::restart_request)
    {
        this->close();
        this->init();
        restart_callback();
        cmd = thread_cmd_continue;
    }
    else
    {
        cmd = thread_cmd_exec;
    }

    m_request = gsl_shell_thread::no_request;
    return cmd;
}

void
gsl_shell_thread::run()
{
    thread_cmd_e cmd = thread_cmd_continue;

    while (cmd != thread_cmd_exit)
    {
        m_eval.lock();
        m_status = ready;

        this->unlock();
        m_eval.wait();
        this->lock();

        before_eval();

        m_status = busy;
        m_eval.unlock();

        cmd = process_request();

        if (cmd == thread_cmd_exec)
        {
            // here m_line_pending cannot be modified by the other thread
            // because we declared above m_status to "busy" befor unlocking m_eval
            const char* line = m_line_pending.cstr();
            m_eval_status = this->exec(line);

            fputc(eot_character, stdout);
            fflush(stdout);

            cmd = process_request();
        }
    }

    this->close();
    this->unlock();
    quit_callback();
}

void
gsl_shell_thread::set_request(gsl_shell_thread::request_e req)
{
    m_eval.lock();
    m_request = req;
    m_eval.signal();
    m_eval.unlock();
    sched_yield();
}

void
gsl_shell_thread::input(const char* line)
{
    pthread::auto_lock lock(m_eval);

    if (m_status == ready)
    {
        m_line_pending = line;
        m_eval.signal();
    }
}

int
gsl_shell_thread::read(char* buffer, unsigned buffer_size)
{
    return m_redirect.read(buffer, buffer_size);
}
