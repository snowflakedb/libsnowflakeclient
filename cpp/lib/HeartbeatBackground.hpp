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
#include "../lib/connection.h"
#include "../include/snowflake/SFURL.hpp"

namespace Snowflake
{
    namespace Client
    {
        typedef struct _heartbeatreq_s
        {
            _heartbeatreq_s(SF_CONNECT* sf, const std::string& url,
                SF_HEADER* header)
                : sessionId(sf->session_id), heartBeatURL(url), httpExtraHeaders(header, sf_header_destroy),
                maxRetryCount(get_login_retry_count(sf)), retryTimeout(get_login_timeout(sf)), networkTimeout(sf->network_timeout),
                isOcspOpen(sf->ocsp_fail_open), isInsecuremode(sf->insecure_mode), retryCurlCount(sf->retry_on_curle_couldnt_connect_count),
                proxy(sf->proxy ? sf->proxy : ""), noProxy(sf->no_proxy ? sf->no_proxy : ""), crlConfig(&sf->crl_config), error(&sf->error)
            {}
            std::string sessionId;
            std::string heartBeatURL;
            std::shared_ptr<SF_HEADER> httpExtraHeaders;
            int8 maxRetryCount;
            int64 retryTimeout;
            int64 networkTimeout;
            sf_bool isOcspOpen;
            sf_bool isInsecuremode;
            int8 retryCurlCount;
            std::string proxy;
            std::string noProxy;
            sf_bool crlAdvisory;
            sf_bool crlCheck;
            sf_bool crlAllowNoCrl;
            sf_bool crlDiskCaching;
            sf_bool crlMemoryCaching;
            int64 crlDownloadTimeout;
            SF_CRL_CONFIG* crlConfig;
            SF_ERROR_STRUCT* error;
        } heartbeatReq;

        class HeartbeatBackground : public ::Snowflake::Client::Singleton<HeartbeatBackground>, private ::Snowflake::Client::DoNotCopy
        {
        public:
            HeartbeatBackground();

            /**
             * destructor
             */
            ~HeartbeatBackground();

            /**
             * Add a pointer to the connection to the heartbeat list
             */
            void addConnection(SF_CONNECT* connection);

            /**
             * Remove the connection object from heartbeat list
             * @param connection
             */
            void removeConnection(SF_CONNECT* connection);

            //Testing Purpose;
            void mockHeartBeat(SF_CONNECT* connection);

        private:
            heartbeatReq genHeartBeatReq(SF_CONNECT* connection);

            /** calculate interval between two heartbeats */
            long calculateHeartBeatInterval(SF_CONNECT* connection);

            /** worker thread that is doing heartbeat*/
            std::unique_ptr<std::thread> m_worker;

            /** Queue of connections that need to heartbeat, mapped by session Id*/
            std::map<std::string, SF_CONNECT*> m_connections;

            long m_heart_beat_interval_in_secs = 3600;

            /** global lock */
            Mutex m_lock;

            /** condition variable */
            std::condition_variable_any m_cv;

            /** thread function that is doing actual heartbeat */
            void heartBeatAll();

            // send out queued heartbeat request, put request needs session renew
            // into renew queue.
            void sendQueuedHeartBeatReq(std::vector<heartbeatReq>& heartBeatQueue,
                std::vector<heartbeatReq>* renewQueue);
            
            void renewSession(std::vector<heartbeatReq>& heartBeatQueue,
                std::vector<heartbeatReq>& renewQueue);

            /** flags indicating whether worker thread should end or not */
            bool m_workerEnded = false;

            bool m_isDebug = false;
        };
    } // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKE_HEARTBEATBACKGROUND_HPP
