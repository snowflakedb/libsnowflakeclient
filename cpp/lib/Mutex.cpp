#include "Mutex.hpp"
#include "../lib/mutex.h"
#include "../logger/SFLogger.hpp"

extern "C" {
    void create_recursive_mutex(void** mutex, uint64_t id)
    {
        try {
            *mutex = (void*) new Snowflake::Client::RecursiveMutex(id);
        }
        catch (...)
        {
            log_error("sf::RecursiveMutex::Fail to create the recursive mutex");
        }
    }

    void free_recursive_mutex(void** mutex)
    {
        try {
            delete static_cast<Snowflake::Client::RecursiveMutex*>(*mutex);
            *mutex = NULL;
        }
        catch (...)
        {
            log_error("sf::RecursiveMutex::Fail to free the recursive mutex");
        }
    }
} // extern "C"

namespace Snowflake::Client
{
    /**
     * Thin wrapper to redefine a mutex to instrument the lock method
     *
     */
    Mutex::Mutex()
    {}

    /**
     * Lock a mutex
     */
    void Mutex::lock()
    {
        // invoke parent logic
        m_mutex.lock();
    }

    void Mutex::unlock()
    {
        m_mutex.unlock();
    }

    RecursiveMutex::RecursiveMutex(uint64_t id)
        : m_id(id)
    {
        // name of this mutex
        const char* mutexName = "sf_mutex";

        // trace to get the correlate OS reference with our internal mutex,
        CXX_LOG_TRACE("sf::RecursiveMutex,/%s mutex=%lu", mutexName, m_id);
    }

    /**
     * Lock a recursive mutex
     */
    void RecursiveMutex::lock()
    {
        // invoke parent logic
        m_recursiveMutex.lock();
    }

    void RecursiveMutex::unlock()
    {
        m_recursiveMutex.unlock();
    }

} // namespace Snowflake::Client