#include "Mutex.hpp"
#include "../lib/mutex.h"
#include "../logger/SFLogger.hpp"

extern "C" {
    void create_recursive_mutex(void** mutex, uint64_t id)
    {
        *mutex = (void*) new Snowflake::Client::RecursiveMutex(id);
    }

    void free_recursive_mutex(void** mutex)
    {
        delete static_cast<Snowflake::Client::RecursiveMutex*>(*mutex);
    }
} // extern "C"

namespace Snowflake
{
    namespace Client
    {
        /**
         * Thin wrapper to redefine a mutex to instrument the lock method
         *
         */
        Mutex::Mutex()
        {
        }

        /**
         * Lock a mutex
         */
        void Mutex::lock()
        {
            // invoke parent logic
            std::mutex::lock();
        }

        RecursiveMutex::RecursiveMutex(uint64_t id)
            : m_id(id)
        {
            // name of this mutex
            const char* mutexName = "sf_mutex";

            // trace to get the correlate OS reference with our internal mutex,
            CXX_LOG_TRACE("sf::RecursiveMutex,/%s/%u mutex=%p", mutexName, m_id);
        }

        /**
         * Lock a recursive mutex
         */
        void RecursiveMutex::lock()
        {
            // invoke parent logic
            std::recursive_mutex::lock();
        }

    } // namespace Client
} // namespace Snowflak