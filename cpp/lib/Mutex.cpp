/*
 * File:   Mutex.cpp
 * Author: tcruanes
 *
 * Copyright (c) 2013-2019 Snowflake Computing
 *
 * Created on August 1, 2013, 10:41 PM
 */

#include "Mutex.hpp"
#include "../logger/SFLogger.hpp"

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

        /**
         * Thin wrapper to redefine a recursive mutex to instrument the lock method
         *
         * @param id
         *   mutex id
         */
        RecursiveMutex::RecursiveMutex(std::uint64_t id)
            : m_id(id)
        {
            // name of this mutex
            const char* mutexName = "sf_mutex";

            // trace to get the correlate OS reference with our internal mutex,
            //CXX_LOG_TRACE("sf:Mutex:Mutex: /%s/%u mutex=%p", mutexName, m_id);
        }

        /**
         * Lock a recursive mutex
         */
        void RecursiveMutex::lock()
        {
            // invoke parent logic
            std::recursive_mutex::lock();
        }
    }
}