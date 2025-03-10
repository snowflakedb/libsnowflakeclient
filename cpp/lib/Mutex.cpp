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

    }
}