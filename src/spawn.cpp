#include <server_lib/spawn.h>
#include <server_lib/asserts.h>
#include <memory>
#include <functional>

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ext/stdio_filebuf.h> 

namespace server_lib
{
namespace /*impl*/
{
    class cpipe {
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
}

class spawn_iml
{
private:
    cpipe _write_pipe;
    cpipe _read_pipe;
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > _write_buf = NULL; 
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > _read_buf = NULL;
    std::ostream &_stdin;
    std::istream &_stdout;
public:
    int child_pid = -1;

   spawn_iml(std::ostream &stdin_r,
            std::istream &stdout_r,
            const char* const argv[], 
            const char* const envp[], 
            bool with_path): 
            _stdin(stdin_r), _stdout(stdout_r) {
        child_pid = fork();
        SRV_ASSERT(child_pid != -1, "Failed to start child process"); 
        if (child_pid == 0)
        {
            dup2(_write_pipe.read_fd(), STDIN_FILENO);
            dup2(_read_pipe.write_fd(), STDOUT_FILENO);
            _write_pipe.close(); _read_pipe.close();
            int result = -1;
            if (with_path) {
                if (envp != 0) result = execvpe(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
                else result = execvp(argv[0], const_cast<char* const*>(argv));
            }
            else {
                if (envp != 0) result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
                else result = execv(argv[0], const_cast<char* const*>(argv));
            }
            SRV_ASSERT(result != -1, "Failed to execute child process");
        }
        else 
        {
            close(_write_pipe.read_fd());
            close(_read_pipe.write_fd());
            _write_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(_write_pipe.write_fd(), std::ios::out));
            _read_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(_read_pipe.read_fd(), std::ios::in));
            _stdin.rdbuf(_write_buf.get());
            _stdout.rdbuf(_read_buf.get());
        }
    }
    
    void send_eof() { _write_buf->close(); }
    
    int wait() {
        int status;
        waitpid(child_pid, &status, 0);
        return status;
    }

    bool async_wait(int &status)
    {
        pid_t result = waitpid(child_pid, &status, WNOHANG);
        SRV_ASSERT(result != -1);

        return result == child_pid;
    }
};

spawn::spawn(const char* const argv[], const char* const envp[], bool with_path): stdin(NULL), stdout(NULL)
{
    _impl = std::make_unique<spawn_iml>(stdin, stdout, argv, envp, with_path);
}

spawn::~spawn(){ /* for linker to resolve spawn_impl only */ }
    
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

bool spawn::async_wait(int &status)
{
    return _impl->async_wait(status);
}

spawn::pool::pool()
{
    _break.store(false);
    _thread = std::make_unique<std::thread>(std::bind(&spawn::pool::wait, this));
}

spawn::pool::~pool()
{
    _break.store(true);
    _thread->join();
    _thread.reset();
}

spawn::pool &spawn::pool::addend(const spawn_ptr &spawn)
{
    std::lock_guard<std::mutex> lock(_pool_protector);
    _pool.push_back(spawn);
    _pool_size.fetch_add(1);
    return *this;
}

void spawn::pool::wait()
{
    while(!_break)
    {
        std::unique_lock<std::mutex> lock(_pool_protector);
        auto pool = std::move(_pool);
        lock.unlock();
        for(const auto &spawn: pool)
        {
            int status = -1;
            if (!spawn->async_wait(status))
            {
                lock.lock();
                _pool.push_back(spawn);
                lock.unlock();
            }else
            {
                _pool_size.fetch_sub(1);
            }
        }
    }
}

}
