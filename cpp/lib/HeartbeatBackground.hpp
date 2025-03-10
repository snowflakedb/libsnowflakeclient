//
// Created by hyu on 12/22/16.
//

#ifndef SNOWFLAKE_HEARTBEATBACKGROUND_HPP
#define SNOWFLAKE_HEARTBEATBACKGROUND_HPP

#include "snowflake/BaseClasses.hpp"
#include <thread>
#include <condition_variable>
#include <list>
#include <vector>
#include <map>
#include "Mutex.hpp"
#include "../include/snowflake/client.h"
#include "../include/snowflake/SFURL.hpp"

namespace Snowflake
{
    namespace Client
    {

        class HeartbeatBackground : public ::Snowflake::Client::Singleton<HeartbeatBackground>, private ::Snowflake::Client::DoNotCopy
        {
        public:
            HeartbeatBackground();

            /**
             * destructor
             */
            ~HeartbeatBackground();

            /**
             * Add a pointer to the conenction to the hearbeat list
             */
            void addConnection(SF_CONNECT* connection);

            /**
             * Remove the connection object from heartbeat list
             * @param connection
             */
            void removeConnection(SF_CONNECT* connection);

        private:
            /** worker thread that is doing heartbeat*/
            std::thread* m_worker = NULL;

            /** Queue of connections that need to heartbeat, mapped by session Id*/
            std::map<std::string, SF_CONNECT*> m_connections;

            uint64 m_master_token_validation_time;
            long m_heart_beat_interval;


            /** global lock */
            Mutex m_lock;

            /** condition variable */
            std::condition_variable_any m_cv;

            /** thread function that is doing actual heartbeat */
            void heartBeatAll();

            // send out queued heartbeat request, put request needs session renew
            // into renew queue.
            void sendQueuedHeartBeatReq(std::vector<SF_CONNECT*>& heartBeatQueue,
                std::vector<SF_CONNECT*>* renewQueue);

            /** flags indicating whether worker thread should end or not */
            bool m_workerEnded = false;
        };
    }
}

#endif //SNOWFLAKE_HEARTBEATBACKGROUND_HPP
