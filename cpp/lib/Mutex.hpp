#pragma once
#ifndef SNOWFLAKE_MUTEX_HPP
#define SNOWFLAKE_MUTEX_HPP

#include <mutex>

namespace Snowflake
{
    namespace Client
    {
        /**
         * Thin wrapper on regular mutex
         */
        class Mutex : public std::mutex
        {
        public:

            /**
             * Constructor
             *
             * @param id
             *   mutex unique id
             */
            Mutex();

            /**
             * Create thin wrapper on top of the lock to capture thread contention
             * when profiling
             */
            void lock();

        private:
        };


        /**
         * Thin wrapper on regular mutex
         */
        class RecursiveMutex : public std::recursive_mutex
        {
        public:

            /**
             * Constructor
             *
             * @param klass
             *   mutex class
             *
             * @param info
             *   info about this mutex
             *
             * @param id
             *   mutex unique id
             */
            RecursiveMutex(uint64_t id);

            /**
             * Create thin wrapper on top of the lock to capture thread contention
             * when profiling
             */
            void lock();

        private:

            /** id of this mutex when it is not unique within the wait class */
            uint64_t                m_id;
        };

#if defined(WIN32) || defined(_WIN64)
        /**
         * Mutex guard
         */
        typedef std::lock_guard<Mutex> MutexGuard;

        /**
         * Mutex unique
         */
        typedef std::unique_lock<Mutex> MutexUnique;

        /**
         * Recursive mutex guard
         */
        typedef std::lock_guard<RecursiveMutex> RecursiveMutexGuard;

        /**
         * Recursive mutex unique
         */
        typedef std::unique_lock<RecursiveMutex> RecursiveMutexUnique;
#else
        /**
         * Lock guard
         */
        template <typename M>
        using LockGuard = std::lock_guard<M>;

        /**
         * Unique lock
         */
        template <typename M>
        using UniqueLock = std::unique_lock<M>;

        /**
         * Mutex guard
         */
        typedef LockGuard<Mutex> MutexGuard;

        /**
         * Mutex unique
         */
        typedef UniqueLock<Mutex> MutexUnique;

        /**
         * Recursive mutex guard
         */
        typedef LockGuard<RecursiveMutex> RecursiveMutexGuard;

        /**
         * Recursive mutex unique
         */
        typedef UniqueLock<RecursiveMutex> RecursiveMutexUnique;
#endif

    } // namespace Client
} // namespace Snowflake
#endif  // SNOWFLAKE_MUTEX_HPP
