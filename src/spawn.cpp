#include <server_lib/spawn.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <server_lib/asserts.h>
#include <memory>
#include <functional>

#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ext/stdio_filebuf.h>

namespace server_lib {
namespace /*impl*/
{
    class cpipe
    {
    private:
        int _fd[2];

    public:
        const inline int read_fd() const { return _fd[0]; }
        const inline int write_fd() const { return _fd[1]; }
        explicit cpipe()
        {
            SRV_ASSERT(pipe(_fd) == 0, "Failed to create pipe");
        }
        void close()
        {
            ::close(_fd[0]);
            ::close(_fd[1]);
        }
        ~cpipe() { close(); }
    };
} // namespace

class spawn_iml
{
private:
    cpipe _write_pipe;
    cpipe _read_stdout_pipe;
    cpipe _read_stderr_pipe;
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char>> _write_buf = NULL;
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char>> _read_stdout_buf = NULL;
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char>> _read_stderr_buf = NULL;
    std::ostream& _stdin;
    std::istream& _stdout;
    std::istream& _stderr;

public:
    pid_t child_pid = -1;

    using args_type = spawn::args_type;
    using envs_type = spawn::envs_type;

    spawn_iml(std::ostream& stdin_r,
              std::istream& stdout_r,
              std::istream& stderr_r,
              const args_type& args,
              const envs_type& envs,
              bool with_path)
        : _stdin(stdin_r)
        , _stdout(stdout_r)
        , _stderr(stderr_r)
    {
        SRV_ASSERT(!args.empty());
        char** pargv = nullptr;
        {
            pargv = decltype(pargv)(alloca((args.size() + 1) * sizeof(char*)));
            decltype(pargv) parg = pargv;
            for (const auto& arg : args)
            {
                *parg = (char*)(alloca((arg.size() + 1) * sizeof(char)));
                memcpy(*parg, arg.c_str(), arg.size());
                (*parg)[arg.size()] = 0;
                parg++;
            }
            *parg = NULL;
        }
        char** penvv = nullptr;
        if (!envs.empty())
        {
            penvv = decltype(penvv)(alloca((envs.size() + 1) * sizeof(char*)));
            decltype(penvv) penv = penvv;
            for (const auto& env : envs)
            {
                size_t sz = env.first.size();
                sz += env.second.size();
                sz += 1;
                *penv = (char*)(alloca((sz + 1) * sizeof(char)));
                char* pstr = *penv;
                memcpy(pstr, env.first.c_str(), env.first.size());
                pstr += env.first.size();
                pstr[0] = '=';
                pstr++;
                memcpy(pstr, env.second.c_str(), env.second.size());
                (*penv)[sz] = 0;
                penv++;
            }
            *penv = NULL;
        }

        child_pid = fork();
        SRV_ASSERT(child_pid != -1, "Failed to start child process");
        if (child_pid == 0)
        {
            dup2(_write_pipe.read_fd(), STDIN_FILENO);
            dup2(_read_stdout_pipe.write_fd(), STDOUT_FILENO);
            dup2(_read_stderr_pipe.write_fd(), STDERR_FILENO);
            _write_pipe.close();
            _read_stdout_pipe.close();
            _read_stderr_pipe.close();
            int result = -1;
            if (with_path)
            {
                if (penvv)
                    result = execvpe(pargv[0], const_cast<char* const*>(pargv), const_cast<char* const*>(penvv));
                else
                    result = execvp(pargv[0], const_cast<char* const*>(pargv));
            }
            else
            {
                if (penvv)
                    result = execve(pargv[0], const_cast<char* const*>(pargv), const_cast<char* const*>(penvv));
                else
                    result = execv(pargv[0], const_cast<char* const*>(pargv));
            }
            SRV_ASSERT(result != -1, "Failed to execute child process");
        }
        else
        {
            close(_write_pipe.read_fd());
            close(_read_stdout_pipe.write_fd());
            close(_read_stderr_pipe.write_fd());
            _write_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>(new __gnu_cxx::stdio_filebuf<char>(_write_pipe.write_fd(), std::ios::out));
            _read_stdout_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>(new __gnu_cxx::stdio_filebuf<char>(_read_stdout_pipe.read_fd(), std::ios::in));
            _read_stderr_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>(new __gnu_cxx::stdio_filebuf<char>(_read_stderr_pipe.read_fd(), std::ios::in));
            _stdin.rdbuf(_write_buf.get());
            _stdout.rdbuf(_read_stdout_buf.get());
            _stderr.rdbuf(_read_stderr_buf.get());
        }
    }

    void send_eof()
    {
        _write_buf->close();
    }

    int wait()
    {
        int status;
        waitpid(child_pid, &status, 0);
        return status;
    }

    pid_t async_wait(bool& finished, int& status)
    {
        pid_t result = waitpid(child_pid, &status, WNOHANG);
        if (result != -1)
        {
            finished = (result == child_pid);
        }
        return result;
    }
};

spawn::spawn(const args_type& args)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _args = args;
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, _args, envs_type {}, false);
}

spawn::spawn(const args_type& args, const envs_type& envs)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _args = args;
    _envs = envs;
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, _args, _envs, false);
}

spawn::spawn(const args_type& args, const envs_type& envs, bool with_path)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _args = args;
    _envs = envs;
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, _args, _envs, with_path);
}

#define SET_ARGS(args)


spawn::spawn(const args_type& args, bool with_path)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _args = args;
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, _args, envs_type {}, with_path);
}

spawn::~spawn()
{
    if (-1 == _returnstatus)
    {
        if (!poll())
            wait();
    }
}

spawn& spawn::wait()
{
    if (-1 == _returnstatus)
    {
        _returnstatus = _impl->wait();
    }
    return *this;
}

bool spawn::poll()
{
    bool finished = true;
    if (-1 == _returnstatus)
    {
        int status = -1;
        _impl->async_wait(finished, status);
        if (finished)
        {
            _returnstatus = status;
        }
    }
    return finished;
}

spawn& spawn::communicate(const std::string& input)
{
    if (!input.empty())
    {
        stdin << input;
    }
    _impl->send_eof();
    wait();
    return *this;
}

spawn& spawn::send_signal(int sig)
{
    ::kill(_impl->child_pid, sig);
    wait();
    return *this;
}

spawn& spawn::terminate()
{
    return send_signal(SIGTERM);
}

spawn& spawn::kill()
{
    return send_signal(SIGKILL);
}

int spawn::pid() const
{
    return static_cast<int>(_impl->child_pid);
}

int spawn::returncode() const
{
    if (_returnstatus < 0)
        return _returnstatus;

    if (WIFEXITED(_returnstatus))
        return WEXITSTATUS(_returnstatus);

    return sigcode();
}

int spawn::sigcode() const
{
    if (_returnstatus < 0)
        return _returnstatus;

    if (WIFSIGNALED(_returnstatus))
        return WTERMSIG(_returnstatus);

    return 0;
}

spawn_polling::spawn_polling(const spawn_ptr& spawn)
{
    _break.store(false);
    _pool_size.store(0);
    if (spawn)
        append(spawn);
    _thread = std::make_unique<std::thread>(std::bind(&spawn_polling::wait, this));
}

spawn_polling::~spawn_polling()
{
    _break.store(true);
    _thread->join();
    _thread.reset();
}

spawn_polling& spawn_polling::append(const spawn_ptr& spawn)
{
    if (!_break)
    {
        std::lock_guard<std::mutex> lock(_pool_protector);
        _pool.push_back(spawn);
        std::atomic_fetch_add<size_t>(&_pool_size, 1);
    }
    return *this;
}

void spawn_polling::wait()
{
    while (!_break)
    {
        std::unique_lock<std::mutex> lock(_pool_protector);
        decltype(_pool) pool = std::move(_pool);
        lock.unlock();
        for (const auto& spawn : pool)
        {
            bool break_ = _break.load();
            if (break_ || !spawn->poll())
            {
                if (!break_)
                    lock.lock();
                _pool.push_back(spawn);
                if (!break_)
                    lock.unlock();
            }
            else
            {
                std::atomic_fetch_sub<size_t>(&_pool_size, 1);
            }
        }
    }

    for (const auto& spawn : _pool)
    {
        spawn->kill();
    }
    _pool.clear();
}

} // namespace server_lib
#endif // SERVER_LIB_PLATFORM_LINUX
