#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace server_lib
{
class spawn;
using spawn_ptr = std::shared_ptr<spawn>;
class spawn_iml;

class spawn 
{
public:
    
    spawn(const char* const argv[], const char* const envp[], bool with_path = false);
    ~spawn();
    
    int child_pid() const;
    void send_eof();    
    int wait();
    bool async_wait(int &);

    std::ostream stdin;
    std::istream stdout;

    class pool
    {
    public:
        pool();
        ~pool();

        pool &addend(const spawn_ptr &);
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

private:
    std::unique_ptr<spawn_iml> _impl;
};

}
