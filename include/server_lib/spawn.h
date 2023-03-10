#pragma once

#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>

namespace server_lib {
class spawn_iml;

// This class is pretty like Python3 subprocess.Popen one.
// It manages IO and lifetime of the child process
class spawn
{
public:
    using args_type = std::vector<std::string>;
    using envs_type = std::map<std::string, std::string>;

    spawn(const args_type& args);
    spawn(const args_type& args, const envs_type& envs);
    spawn(const args_type& args, const envs_type& envs, bool with_path);
    spawn(const args_type& args, bool with_path);
    ~spawn();

    bool poll();
    spawn& wait();
    spawn& communicate(const std::string& input = {});
    spawn& send_signal(int);
    spawn& terminate();
    spawn& kill();
    args_type args() const
    {
        return _args;
    }
    envs_type envs() const
    {
        return _envs;
    }

    std::ostream stdin;
    std::istream stdout;
    std::istream stderr;

    int pid() const;
    int returncode() const;
    // if it was not terminated by signal returns 0
    int sigcode() const;

private:
    std::unique_ptr<spawn_iml> _impl;
    int _returnstatus = -1;
    args_type _args;
    envs_type _envs;
};
using spawn_ptr = std::shared_ptr<spawn>;

// Asynchronous wait for >=1 spawned processes and kill them
// if polling will be destroyed before
class spawn_polling
{
public:
    spawn_polling(const spawn_ptr& = {});
    ~spawn_polling();

    spawn_polling& append(const spawn_ptr&);
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
