#include <server_lib/spawn.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <server_lib/asserts.h>
#include <memory>
#include <functional>

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
    int child_pid = -1;

    spawn_iml(std::ostream& stdin_r,
              std::istream& stdout_r,
              std::istream& stderr_r,
              const char* const argv[],
              const char* const envp[],
              bool with_path)
        : _stdin(stdin_r)
        , _stdout(stdout_r)
        , _stderr(stderr_r)
    {
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
                if (envp != 0)
                    result = execvpe(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
                else
                    result = execvp(argv[0], const_cast<char* const*>(argv));
            }
            else
            {
                if (envp != 0)
                    result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
                else
                    result = execv(argv[0], const_cast<char* const*>(argv));
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

    void send_eof() { _write_buf->close(); }

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

spawn::spawn(const char* const argv[])
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    const char* envp[] = { NULL };
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, argv, envp, false);
}

spawn::spawn(const char* const argv[], const char* const envp[])
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, argv, envp, false);
}

spawn::spawn(const char* const argv[], const char* const envp[], bool with_path)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, argv, envp, with_path);
}

spawn::spawn(const char* const argv[], bool with_path)
    : stdin(NULL)
    , stdout(NULL)
    , stderr(NULL)
{
    const char* envp[] = { NULL };
    _impl = std::make_unique<spawn_iml>(stdin, stdout, stderr, argv, envp, with_path);
}

spawn::~spawn()
{
    int status = -1;
    if (!async_wait(status))
        wait();
}

int spawn::child_pid() const
{
    return _impl->child_pid;
}

void spawn::send_eof()
{
    _impl->send_eof();
}

int spawn::wait()
{
    return _impl->wait();
}

bool spawn::async_wait(int& status)
{
    bool finished = true;
    _impl->async_wait(finished, status);
    return finished;
}

spawn_waiting_pool::spawn_waiting_pool()
{
    _break.store(false);
    _pool_size.store(0);
    _thread = std::make_unique<std::thread>(std::bind(&spawn_waiting_pool::wait, this));
}

spawn_waiting_pool::~spawn_waiting_pool()
{
    _break.store(true);
    _thread->join();
    _thread.reset();
}

spawn_waiting_pool& spawn_waiting_pool::addend(const spawn_ptr& spawn)
{
    std::lock_guard<std::mutex> lock(_pool_protector);
    _pool.push_back(spawn);
    std::atomic_fetch_add<size_t>(&_pool_size, 1);
    return *this;
}

void spawn_waiting_pool::wait()
{
    while (!_break)
    {
        std::unique_lock<std::mutex> lock(_pool_protector);
        decltype(_pool) pool = std::move(_pool);
        lock.unlock();
        for (const auto& spawn : pool)
        {
            int status = -1;
            if (_break || !spawn->async_wait(status))
            {
                lock.lock();
                _pool.push_back(spawn);
                lock.unlock();
            }
            else
            {
                std::atomic_fetch_sub<size_t>(&_pool_size, 1);
            }
        }
    }

    std::lock_guard<std::mutex> lock(_pool_protector);
    for (const auto& spawn : _pool)
    {
        kill(spawn->child_pid(), SIGKILL);
    }
    _pool.clear();
}

} // namespace server_lib
#endif // SERVER_LIB_PLATFORM_LINUX
