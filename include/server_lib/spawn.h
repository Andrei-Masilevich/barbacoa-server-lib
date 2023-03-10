#pragma once

#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace server_lib {
class spawn_iml;

// Like Python subprocess the following class
// manage IO and lifetime of child process
class spawn
{
public:
    spawn(const char* const argv[]);
    spawn(const char* const argv[], const char* const envp[]);
    spawn(const char* const argv[], const char* const envp[], bool with_path);
    spawn(const char* const argv[], bool with_path);
    ~spawn();

    int child_pid() const;
    void send_eof();
    int wait();
    bool async_wait(int&);

    std::ostream stdin;
    std::istream stdout;
    std::istream stderr;

private:
    std::unique_ptr<spawn_iml> _impl;
};
using spawn_ptr = std::shared_ptr<spawn>;

// Wait for >=1 spawned processes and kill them
// if pool will be destroyed before
class spawn_waiting_pool
{
public:
    spawn_waiting_pool();
    ~spawn_waiting_pool();

    spawn_waiting_pool& addend(const spawn_ptr&);
    bool empty() const
    {
        return _pool_size.load() == 0;
    }
    size_t count() const
    {
        return _pool_size.load();
    }

private:
    void wait();

    std::unique_ptr<std::thread> _thread;
    std::vector<spawn_ptr> _pool;
    std::mutex _pool_protector;
    std::atomic_bool _break;
    std::atomic_size_t _pool_size;
};

} // namespace server_lib

#endif // SERVER_LIB_PLATFORM_LINUX
