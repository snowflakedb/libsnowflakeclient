#include "HeartbeatBackground.hpp"
#include "../lib/heart_beat_background.h"
#include "../lib/client_int.h"
#include "../lib/connection.h"
#include "../logger/SFLogger.hpp"
#include "curl_desc_pool.h"
#include "../include/snowflake/SFURL.hpp"
#include "../lib/snowflake_util.h"

extern "C" {
    using namespace Snowflake::Client;

    void start_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        try {
            _mutex_lock(&sf->mutex_heart_beat);
            if (!sf->is_heart_beat_on)
            {
                log_trace("sf::HeartbeatBackrgound::start_heart_beat_for_this_session::Add the connection to heartbeatSync list");
                HeartbeatBackground& bg = HeartbeatBackground::getInstance();
                bg.addConnection(sf);
                sf->is_heart_beat_on = SF_BOOLEAN_TRUE;
            }
            else
            {
                log_trace("sf::HeartbeatBackrgound::startHeartBeatForThisSessionSync::Heartbeat already enabled for this session");
            }
            _mutex_unlock(&sf->mutex_heart_beat);
        }
        catch (...)
        {
            log_error("sf::HeartbeatBackrgound::start_heart_beat_for_this_session::Exception occurred when starting heartbeat for this session");
        }
    }

    void stop_heart_beat_for_this_session(SF_CONNECT* sf)
    {
        try {
            _mutex_lock(&sf->mutex_heart_beat);
            if (sf->is_heart_beat_on)
            {
                log_trace("sf::HeartbeatBackrgound::stop_heart_beat_for_this_session::Add the connection to heartbeatSync list");
                HeartbeatBackground& bg = HeartbeatBackground::getInstance();
                bg.removeConnection(sf);
                sf->is_heart_beat_on = SF_BOOLEAN_FALSE;
            }
            else
            {
                log_trace("sf::HeartbeatBackrgound::startHeartBeatForThisSessionSync::Heartbeat already disabled for this session");
            }
            _mutex_unlock(&sf->mutex_heart_beat);
        }
        catch (...)
        {
            log_error("sf::HeartbeatBackrgound::stop_heart_beat_for_this_session::Exception occurred when stopping heartbeat for this session");
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
            CXX_LOG_TRACE("sf::HeartbeatBackrgound::renew_session_sync::Failed to renew session");
            stop_heart_beat_for_this_session(sf);
            ret = SF_BOOLEAN_FALSE;
        }
        free_curl_desc(curl_desc);
        return SF_BOOLEAN_TRUE;
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

    long HeartbeatBackground::calculateHeartBeatInterval(long master_token_validation_time)
    {
        return master_token_validation_time / 4;
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
            SF_ERROR_STRUCT* err = NULL;

            CXX_LOG_TRACE("sf::HeartbeatBackground::sendQueuedHeartBeatReq::sending heartbeat for session %s", conn.sessionId.c_str());

            if (http_perform(curl, POST_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, &resp_data,
                NULL, NULL, conn.retryTimeout, conn.networkTimeout, SF_BOOLEAN_FALSE, err,
                conn.isInsecuremode, conn.isOcspOpen, conn.crlCheck, conn.crlAdvisory, conn.crlAllowNoCrl,
                conn.crlDiskCaching, conn.crlMemoryCaching, conn.crlDownloadTimeout,
                conn.retryCurlCount, 0, conn.maxRetryCount, &elapsedTime, &retried_count, NULL, SF_BOOLEAN_FALSE,
                proxy, noProxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE))
            {
                sf_bool success = SF_BOOLEAN_FALSE;
                if (json_copy_bool(&success, resp_data, "success") == SF_JSON_ERROR_NONE && !success) {
                    char* code = snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp_data, "code"));
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
                long heartBeatInterval = this->calculateHeartBeatInterval(this->m_master_token_validation_time);

                // For debug purpose only force heartbeat iterval to 1 second
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
        CXX_LOG_TRACE("sf::HeartbeatBackground:LgenHeartbeatReq::generate heartbeat request to the server");
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
        if (!create_header(connection, httpExtraHeaders, &connection->error)) {
            CXX_LOG_TRACE("sf::HeartBeatBackground::sendQueuedHeartBeatReq::Failed to create the header for the request HeartBeat");
        }

        std::string sessionId = connection->session_id;
        std::string destination = url.toString();

        return heartbeatReq(connection, destination, httpExtraHeaders);
    }

    void HeartbeatBackground::freeHeartBeatReqQueue(std::vector<heartbeatReq>& HeartBeatQueue)
    {
        HeartBeatQueue.clear();
    }

    void HeartbeatBackground::renewSession(std::vector<heartbeatReq>& heartBeatQueue,
        std::vector<heartbeatReq>& renewQueue)
    {
        if (renewQueue.size() > 0)
        {
            CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::%d connections need retry with session renew", renewQueue.size());
            freeHeartBeatReqQueue(heartBeatQueue);
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
                    CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::retry on session %s with session renew", renewQueue[i].sessionId.c_str());
                    try
                    {
                        renew_session_sync(itr->second);
                        heartBeatQueue.emplace_back(this->genHeartBeatReq(itr->second));
                    }
                    catch (...)
                    {
                        CXX_LOG_INFO("sf::HeartbeatBackground::heartBeatAll::session renew failed for session id: %s", renewQueue[i].sessionId.c_str());
                    }
                }
                CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::give up retry since session is closed: %s", renewQueue[i].sessionId.c_str());
            }
        }

        // resend heartbeat after renew session, no further session renew needed
        CXX_LOG_TRACE("sf::HeartbeatBackground::heartBeatAll::resend heartbeat for %d of connections after session renew", heartBeatQueue.size());
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
        freeHeartBeatReqQueue(HeartBeatQueue);
    }

} // namespace Snowflake::Client