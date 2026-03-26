#include "HeartbeatBackground.hpp"
#include "../lib/heart_beat_background.h"
#include "../lib/client_int.h"
#include "../lib/connection.h"
#include "../logger/SFLogger.hpp"
#include "curl_desc_pool.h"
#include "../include/snowflake/SFURL.hpp"
#include "../lib/snowflake_util.h"
#include <algorithm>

extern "C" {
    using namespace Snowflake::Client;

    void start_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        if (sf->mutex_tokens) {
            try
            {
                log_trace("sf::HeartbeatBackground::start_heart_beat_for_this_session::Add the connection to heartbeatSync list");
                HeartbeatBackground& bg = HeartbeatBackground::getInstance();
                bg.addConnection(sf);
                sf->is_heart_beat_on = SF_BOOLEAN_TRUE;
            }
            catch (...)
            {
                log_error("sf::HeartbeatBackground::start_heart_beat_for_this_session::Exception occurred when starting heartbeat for this session");
            }
        }
        else
        {
            log_trace("sf::HeartbeatBackground::start_heart_beat_for_this_session::The mutex for token is not initialized, skip starting heartbeat for this session");
        }
    }

    void stop_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        if (sf->mutex_tokens) {
            try
            {
                log_trace("sf::HeartbeatBackground::stop_heart_beat_for_this_session::Stop the heartbeat for this session");
                HeartbeatBackground& bg = HeartbeatBackground::getInstance();
                bg.removeConnection(sf);
                sf->is_heart_beat_on = SF_BOOLEAN_FALSE;
            }
            catch (...)
            {
                log_error("sf::HeartbeatBackground::stop_heart_beat_for_this_session::Exception occurred when stopping heartbeat for this session");
            }
        }
        else
        {
            log_trace("sf::HeartbeatBackground::stop_heart_beat_for_this_session::The mutex for token is not initialized, The heartbeat was skipped and do not need to stop");
        }
    }

    sf_bool renew_session_sync(SF_CONNECT* sf)
    {
        sf_bool ret = SF_BOOLEAN_TRUE;
        void* curl_desc = get_curl_desc_from_pool(RENEW_SESSION_URL, sf->proxy, sf->no_proxy);
        CURL* curl = get_curl_from_desc(curl_desc);
        SF_ERROR_STRUCT* err = &sf->error;

        RecursiveMutexGuard guard(*static_cast<Snowflake::Client::RecursiveMutex*>(sf->mutex_tokens));
        if (!renew_session(curl, sf, err))
        {
            CXX_LOG_TRACE("sf::HeartbeatBackground::renew_session_sync::Failed to renew session");
            ret = SF_BOOLEAN_FALSE;
        }
        free_curl_desc(curl_desc);
        return ret;
    }

    void test_heartbeat(SF_CONNECT* sf) {

        HeartbeatBackground& bg = HeartbeatBackground::getInstance();
        bg.mockHeartBeat(sf);
    }

} // extern "C"

namespace Snowflake::Client
{

    HeartbeatBackground::HeartbeatBackground() {}

    HeartbeatBackground::~HeartbeatBackground()
    {
        {
            MutexGuard m_guard(m_lock);
            m_workerEnded = true;
        }

        // wake up worker thread and let it ended
        m_cv.notify_all();
        if (m_worker && m_worker->joinable())
        {
            try {
                m_worker->join();
            }
            catch (const std::exception& e)
            {
                CXX_LOG_ERROR("sf::HeartbeatBackground::~HeartbeatBackground::Exception occurred when joining the heartbeat worker thread, err: %s", e.what());
            }
        }
    }

    void HeartbeatBackground::addConnection(SF_CONNECT* connection)
    {
        bool needToNotify = false;
        {
            // gained the lock first
            MutexGuard m_guard(m_lock);
            if (connection->is_heart_beat_on)
            {
                CXX_LOG_TRACE("sf::HeartbeatBackground::addConnection::Heartbeat already enabled for this session");
                return;
            }

            m_connections[connection->session_id] = connection;
            connection->is_heart_beat_on = SF_BOOLEAN_TRUE;

            uint64 heartBeatInterval = calculateHeartBeatInterval(connection);

            if (m_worker == NULL)
            {
                this->m_heart_beat_interval_in_secs = heartBeatInterval;
                CXX_LOG_TRACE("sf::HeartbeatBackground::addConnection:: start a new thread for heartbeatSync  with interval of %ld", heartBeatInterval);
                m_worker = std::make_unique<std::thread>(&HeartbeatBackground::heartBeatAll, this);
            }
            else
            {
                if (heartBeatInterval < this->m_heart_beat_interval_in_secs)
                {
                    CXX_LOG_TRACE("sf::HeartbeatBackground::addConnection:: Heartbeat interval lower to %ld", heartBeatInterval);
                    // update the validation time
                    this->m_heart_beat_interval_in_secs = heartBeatInterval;

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
        if(!connection->is_heart_beat_on)
        {
            CXX_LOG_TRACE("sf::HeartbeatBackground::removeConnection::Heartbeat already disabled for this session");
            return;
        }
        m_connections.erase(connection->session_id);
        connection->is_heart_beat_on = SF_BOOLEAN_FALSE;

        //reset the heartBeat Interval

        if (!m_connections.empty())
        {
            long minInterval = SF_DEFAULT_CLIENT_SESSION_ALIVE_HEARTBEAT_FREQUENCY;
            for (const auto& session : m_connections)
            {
                long frequency = calculateHeartBeatInterval(session.second);
                minInterval = minInterval < frequency ? minInterval : frequency;
                minInterval = minInterval < frequency ? minInterval : frequency;
            }
            m_heart_beat_interval_in_secs = minInterval;
        }
    }

    long HeartbeatBackground::calculateHeartBeatInterval(SF_CONNECT* connection)
    {
        long tokenTime = connection->master_token_validation_time / 4;
        long frequency = connection->client_session_keep_alive_heartbeat_frequency;

        return tokenTime < frequency ? tokenTime : frequency;
    }

    void HeartbeatBackground::sendQueuedHeartBeatReq(
        std::vector<heartbeatReq>& HeartBeatQueue,
        std::vector<heartbeatReq>* renewQueue)
    {
        for (size_t i = 0; i < HeartBeatQueue.size(); i++)
        {
            heartbeatReq conn = HeartBeatQueue[i];
            const std::string& destination = conn.heartBeatURL;
            const std::string& sid = conn.sessionId;
            SF_HEADER* httpExtraHeaders = conn.httpExtraHeaders.get();

            const char* proxy = conn.proxy.c_str();
            const char* noProxy = conn.noProxy.c_str();

            void* curl_desc = get_curl_desc_from_pool(destination.c_str(), proxy, noProxy);
            CURL* curl = get_curl_from_desc(curl_desc);

            int8 retried_count = 0;
            int64 elapsedTime = 0;
            cJSON* resp_data = NULL;
            SF_ERROR_STRUCT err;

            CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::sending heartbeat for session %s", conn.sessionId.c_str());

            if (http_perform(curl, POST_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, &resp_data,
                NULL, NULL, conn.retryTimeout, conn.networkTimeout, SF_BOOLEAN_FALSE, &err,
                conn.isInsecuremode, conn.isOcspOpen, &conn.crlConfig,
                conn.retryCurlCount, 0, conn.maxRetryCount, &elapsedTime, &retried_count, NULL, SF_BOOLEAN_FALSE,
                proxy, noProxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE))
            {
                sf_bool success = SF_BOOLEAN_FALSE;
                if (json_copy_bool(&success, resp_data, "success") == SF_JSON_ERROR_NONE && !success) {
                    cJSON* codeItem = snowflake_cJSON_GetObjectItem(resp_data, "code");
                    const char* code = codeItem ? codeItem->valuestring : "";
                    if ((renewQueue) && strcmp(code, SESSION_TOKEN_EXPIRED_CODE) == 0)
                    {
                        CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Session token expired will retry later sessionId: %s", sid.c_str());
                        renewQueue->emplace_back(HeartBeatQueue[i]);
                    }
                    else
                    {
                        CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Sending heartbeat failed with Session Id: %s and errorcode: %s. Remove it from the HearBeat queue", sid.c_str(), code);
                    }
                }
                else
                {
                    CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Session token did NOT expire. No token update. Session Id: %s", sid.c_str());
                }
            }
            else
            {
                CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::Encountered error when heartbeat sync");
            }

            free_curl_desc(curl_desc);
            snowflake_cJSON_Delete(resp_data);
        }
    }

    void HeartbeatBackground::heartBeatAll()
    {
        while (true)
        {
            // Queue heartbeat request for each connection
            std::vector<heartbeatReq> HeartBeatQueue;
            // Collect heartbeat requests need session renew into a separated queue
            // which needs locking during renew
            std::vector<heartbeatReq> renewQueue;
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
                long heartBeatInterval = m_heart_beat_interval_in_secs;

                // For debug purpose only force heartbeat interval to 1 second
                // https://github.com/snowflakedb/snowflake-sdks-drivers-issues-teamwork/issues/368
#ifdef HEARTBEAT_DEBUG
                heartBeatInterval = 1;
#endif
                CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::HeartBeat interval: %ld", heartBeatInterval);
                std::chrono::duration<long> heartBeatDuration = std::chrono::duration<long>(heartBeatInterval);
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
                        HeartBeatQueue.emplace_back(this->genHeartBeatReq(itr->second));
                    }
                }
            }
            // For debug purpose only sleep before sending heartbeat.
#ifdef HEARTBEAT_DEBUG
            sf_sleep_ms(3000);
#endif

            CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::Worker thread start heartbeating.");
            sendQueuedHeartBeatReq(HeartBeatQueue, &renewQueue);

            // For debug purpose only forcely renew session each time and resend heartbeat as well
#ifdef HEARTBEAT_DEBUG
            renewQueue.clear();
            renewQueue.insert(renewQueue.begin(), HeartBeatQueue.begin(), HeartBeatQueue.end());
#endif

            // renew session
            renewSession(HeartBeatQueue, renewQueue);
        }
    }

    heartbeatReq HeartbeatBackground::genHeartBeatReq(SF_CONNECT* connection)
    {
        CXX_LOG_TRACE("sf::HeartbeatBackground::genHeartBeatReq::generate heartbeat request to the server");
        char requestid[SF_UUID4_LEN], requestgid[SF_UUID4_LEN];
        uuid4_generate(requestid);
        uuid4_generate(requestgid);

        std::string protocol = connection->protocol;
        std::string host = connection->host;
        std::string port = connection->port;
        SFURL url = SFURL(protocol, host, port).path(HEART_BEAT_URL)
            .addQueryParam("requestId", requestid).addQueryParam("request_guid", requestgid);

        SF_HEADER* httpExtraHeaders = sf_header_create();
        httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;

        {
            RecursiveMutexGuard guard(*static_cast<Snowflake::Client::RecursiveMutex*>(connection->mutex_tokens));
            if (!create_header(connection, httpExtraHeaders, &connection->error)) {
                CXX_LOG_TRACE("sf::HeartBeatBackground::genHeartBeatReq::Failed to create the header for the request HeartBeat");
            }
        }
        std::string sessionId = connection->session_id;
        std::string destination = url.toString();

        return heartbeatReq(connection, destination, httpExtraHeaders);
    }

    void HeartbeatBackground::renewSession(std::vector<heartbeatReq>& heartBeatQueue,
        std::vector<heartbeatReq>& renewQueue)
    {
        if (renewQueue.size() > 0)
        {
            CXX_LOG_TRACE("sf::HeartbeatBackground::renewSession::%zu connections need retry with session renew", renewQueue.size());
            heartBeatQueue.clear();
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
                std::map<std::string, SF_CONNECT*>::iterator itr = m_connections.find(renewQueue[i].sessionId);
                if (itr != m_connections.end())
                {
                    CXX_LOG_TRACE("sf::HeartbeatBackground::renewSession::retry on session %s with session renew", renewQueue[i].sessionId.c_str());

                    if (renew_session_sync(itr->second))
                    {
                        heartBeatQueue.emplace_back(this->genHeartBeatReq(itr->second));
                    }
                    else
                    {
                        CXX_LOG_INFO("sf::HeartbeatBackground::renewSession::session renew failed for session id: %s", renewQueue[i].sessionId.c_str());
                        SF_CONNECT* failedConn = itr->second;
                        m_connections.erase(itr);
                        failedConn->is_heart_beat_on = SF_BOOLEAN_FALSE;
                    }
                }
                else
                {
                    CXX_LOG_TRACE("sf::HeartbeatBackground::renewSession::give up retry since session is closed: %s", renewQueue[i].sessionId.c_str());
                } 
            }
        }

        // resend heartbeat after renew session, no further session renew needed
        CXX_LOG_TRACE("sf::HeartbeatBackground::renewSession::resend heartbeat for %zu of connections after session renew", heartBeatQueue.size());
        sendQueuedHeartBeatReq(heartBeatQueue, NULL);
    }

    void HeartbeatBackground::mockHeartBeat(SF_CONNECT* sf) {
        std::vector<heartbeatReq> HeartBeatQueue;
        std::vector<heartbeatReq> renewQueue;

        HeartBeatQueue.clear();
        heartbeatReq hb = genHeartBeatReq(sf);
        HeartBeatQueue.emplace_back(hb);
        sendQueuedHeartBeatReq(HeartBeatQueue, NULL);
        renewQueue.clear();
        renewQueue.insert(renewQueue.begin(), HeartBeatQueue.begin(), HeartBeatQueue.end());
        renewSession(HeartBeatQueue, renewQueue);
        HeartBeatQueue.clear();
    }

} // namespace Snowflake::Client