//
// Created by hyu on 12/22/16.
//

#include "HeartbeatBackground.hpp"
#include "../lib/heart_beat_background.h"
#include "../lib/client_int.h"
#include "../lib/connection.h"
#include "../logger/SFLogger.hpp"
#include "curl_desc_pool.h"
#include "../include/snowflake/SFURL.hpp"

extern "C" {
    using namespace Snowflake::Client;

    void start_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        _mutex_lock(&sf->mutex_heart_beat);
        if (!sf->is_heart_beat_on)
        {
            CXX_LOG_TRACE("sf::start_heart_beat_for_this_session::Add the connection to heartbeatSync list");
            HeartbeatBackground& bg = HeartbeatBackground::getInstance();
            bg.addConnection(sf);
            sf->is_heart_beat_on = SF_BOOLEAN_TRUE;
        }
        else
        {
            CXX_LOG_TRACE("sf::Connection::startHeartBeatForThisSessionSync::Heartbeat already enabled for this session");
        }
        _mutex_unlock(&sf->mutex_heart_beat);
    }

    void stop_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        _mutex_lock(&sf->mutex_heart_beat);
        if (sf->is_heart_beat_on)
        {
            CXX_LOG_TRACE("sf::start_heart_beat_for_this_session::Add the connection to heartbeatSync list");
            HeartbeatBackground& bg = HeartbeatBackground::getInstance();
            bg.removeConnection(sf);
            sf->is_heart_beat_on = SF_BOOLEAN_FALSE;
        }
        else
        {
            CXX_LOG_TRACE("sf::Connection::startHeartBeatForThisSessionSync::Heartbeat already enabled for this session");
        }
        _mutex_unlock(&sf->mutex_heart_beat);
    }

} // extern "C"


namespace Snowflake
{
    namespace Client
    {
        HeartbeatBackground::HeartbeatBackground()
        {
        }

        HeartbeatBackground::~HeartbeatBackground()
        {
            {
                MutexGuard m_guard(m_lock);
                m_workerEnded = true;
            }
            // wake up worker thread and let it ended
            m_cv.notify_all();
            // join
            if (m_worker != NULL)
            {
                m_worker->join();
                delete m_worker;
            }
        }

        void HeartbeatBackground::addConnection(SF_CONNECT* connection)
        {
            bool needToNotify = false;
            {
                // gained the lock first
                MutexGuard m_guard(m_lock);

                m_connections[connection->session_id] = connection;
                if (m_worker == NULL)
                {
                    this->m_master_token_validation_time = connection->master_token_validation_time;
                    this->m_heart_beat_interval = connection->client_session_keep_alive_heartbeat_frequency;
                    CXX_LOG_TRACE("sf::HeartbeatBackground::addConnection:: start a new thread for heartbeatSync");
                    m_worker = new std::thread(&HeartbeatBackground::heartBeatAll, this);
                }
                else
                {
                    if (connection->master_token_validation_time < this->m_master_token_validation_time)
                    {
                        CXX_LOG_TRACE("sf::HeartbeatBackground::addConnection:: Master token validity time lower to %ld", connection->master_token_validation_time);
                        // update the validation time
                        this->m_master_token_validation_time = connection->master_token_validation_time;

                        needToNotify = true;
                    }
                }
            }
            if (needToNotify)
            {
                m_cv.notify_all();
            }
        }

        void HeartbeatBackground::removeConnection(SF_CONNECT* connection)
        {
            MutexGuard m_guard(m_lock);
            m_connections.erase(connection->session_id);
        }

        void HeartbeatBackground::sendQueuedHeartBeatReq(
            std::vector<SF_CONNECT*>& HeartBeatQueue,
            std::vector<SF_CONNECT*>* renewQueue)
        {
            for (size_t i = 0; i < HeartBeatQueue.size(); i++)
            {
                bool ret = true;
                SF_CONNECT* conn = HeartBeatQueue[i];
                const std::string& sid = conn->session_id;
                char requestid[SF_UUID4_LEN], requestgid[SF_UUID4_LEN];
                uuid4_generate(requestid);
                uuid4_generate(requestgid);
                SFURL& url = SFURL::getServerURLSync(conn).path(HEART_BEAT_URL).addQueryParam("requestId", requestid).addQueryParam("request_guid", requestgid);
                int maxRetryCount = get_login_retry_count(conn);
                int8 retried_count = 0;
                int64 elapsedTime = 0;

                std::string destination = url.toString();
                void* curl_desc;
                CURL* curl;
                curl_desc = get_curl_desc_from_pool(destination.c_str(), conn->proxy, conn->no_proxy);
                curl = get_curl_from_desc(curl_desc);

                CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::sending heartbeat for session %s", sid.c_str());
                SF_HEADER* httpExtraHeaders = sf_header_create();
                httpExtraHeaders->renew_session = SF_BOOLEAN_TRUE;
                httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
                if (!create_header(conn, httpExtraHeaders, &conn->error)) {
                    CXX_LOG_TRACE("sf::HeartBeatBackground::sendQueuedHeartBeatReq::Failed to create the header for the request HeartBeat");
                    ret = false;
                }

                cJSON* resp_data = NULL;
                    if (ret && curl_post_call(conn, curl, (char*)destination.c_str(), httpExtraHeaders, NULL,
                        &resp_data, &conn->error, 0, maxRetryCount, get_retry_timeout(conn), &elapsedTime,
                        &retried_count, NULL, SF_BOOLEAN_TRUE))
                    {
                       sf_bool success = SF_BOOLEAN_FALSE;
                       if (json_copy_bool(&success, resp_data, "success") == SF_JSON_ERROR_NONE) {
                           cJSON* data = snowflake_cJSON_GetObjectItem(resp_data, "data");
                           char* code = snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(data, "code"));
                           if ((renewQueue) && strcmp(code, SESSION_TOKEN_EXPIRED_CODE) == 0)
                           {
                               CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Session token expired will retry later sessionId: %s", sid.c_str());
                               renewQueue->emplace_back(HeartBeatQueue[i]);
                           }
                           else
                           {
                               CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Sending heartbeat failed with Session Id: %s and errorcode: %d", sid.c_str(), code);
                           }
                       }
                       else
                       {
                           CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Session token did NOT expire. No token update. Session Id: %s", sid.c_str());
                       }
                    }
                    else 
                    {
                        CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::Encountered error when heartbeat sync");
                        ret = false;
                    }
                sf_header_destroy(httpExtraHeaders);
                free_curl_desc(curl_desc);
                snowflake_cJSON_Delete(resp_data);
            }
        }

        void HeartbeatBackground::heartBeatAll()
        {
            while (true)
            {
                // Queue heartbeat request for each connection
                std::vector<SF_CONNECT*> HeartBeatQueue;
                // Collect heartbeat requests need session renew into a separated queue
                // which needs locking during renew
                std::vector<SF_CONNECT*> renewQueue;
                HeartBeatQueue.clear();
                renewQueue.clear();

                // only holds the lock until we get all heartbeat request queued.
                // release the lock when sending them
                // collect heartbeat request from connection instead of queue connections
                // and send heartbeat request from there since when sending heartbeat/
                // the lock is released for performance reason and a queued connection could
                // be closed and destroyed during that
                {
                    MutexUnique guard(m_lock);

                    // For debug purpose only force heartbeat iterval to 1 second
                    // https://github.com/snowflakedb/snowflake-sdks-drivers-issues-teamwork/issues/368
#ifdef HEARTBEAT_DEBUG
                    m_heart_beat_interval = 1;
#endif
                    CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::HeartBeat interval: %ld", m_heart_beat_interval);
                    std::chrono::duration<long> heartBeatDuration = std::chrono::duration<long>(m_heart_beat_interval);
                    // wait on either being notified by main thread or timeout(which is the desired heartbeatSync interval)
                    m_cv.wait_for(guard, heartBeatDuration);
                    if (m_workerEnded)
                    {
                        // notified by main thread that worker thread should exit
                        return;
                    }
                    else
                    {
                        CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::queue heartbeat requests...");
                        for (std::map<std::string, SF_CONNECT*>::const_iterator itr = m_connections.begin(); itr != m_connections.end(); itr++)
                        {
                            HeartBeatQueue.emplace_back(itr->second);
                        }
                    }
                }

                // For debug purpose only sleep before sending heartbeat so the test case
                // (Concurrent Connection in ConnectionLatestTest) can get chance to close
                // connections
#ifdef HEARTBEAT_DEBUG
                simba_sleep(3000);
#endif

                CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::Worker thread start heartbeating.");
                sendQueuedHeartBeatReq(HeartBeatQueue, &renewQueue);

                // For debug purpose only forcely renew session each time and resend
                // heartbeat as well
#ifdef HEARTBEAT_DEBUG
                renewQueue.clear();
                renewQueue.insert(renewQueue.begin(), HeartBeatQueue.begin(), HeartBeatQueue.end());
#endif

                // renew session
                if (renewQueue.size() > 0)
                {
                    CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::%d connections need retry with session renew", renewQueue.size());
                    HeartBeatQueue.clear();
                    // get lock during renew. the reason is that:
                    // 1. session renew needs more connection related implementation and it
                    //    would be too heavy to bring it here
                    // 2. hopefully there won't be too many connections need renew since
                    //    by default it's per 4 hours
                    // 3. We need to update session token in the connection which needs lock
                    //    anyway
                    MutexUnique guard(m_lock);
                    for (size_t i = 0; i < renewQueue.size(); i++)
                    {
                        // check if the connection still in the heartbeat queue first
                        // since it could be closed during heartbeat request sending with lock released
                        std::map<std::string, SF_CONNECT*>::iterator itr =  m_connections.find(renewQueue[i]->session_id);
                        if (itr != m_connections.end())
                        {
                            CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::retry on session %s with session renew",renewQueue[i]->session_id);
                            try
                            {
                                HeartBeatQueue.emplace_back(itr->second);
                            }
                            catch (...)
                            {
                                CXX_LOG_INFO("sf::HeartbeatBackground::heartBeatAll::session renew failed for session id: %s",renewQueue[i]->session_id);
                            }
                        }
                        CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::give up retry since session is closed: %s", renewQueue[i]->session_id);
                    }
                }

                // resend heartbeat after renew session, no further session renew needed
                CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::resend heartbeat for %d of connections after session renew", HeartBeatQueue.size());
                sendQueuedHeartBeatReq(HeartBeatQueue, NULL);
            }
        }
    }
}



