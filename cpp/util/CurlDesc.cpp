#include "snowflake/CurlDesc.hpp"
#include "../logger/SFLogger.hpp"
#include <openssl/ssl.h>

namespace Snowflake
{
namespace Client
{
  /**
   * Constructor
   */
  CurlDesc::CurlDesc(CURLSH *curlShareDesc)
  : m_shareCurl(curlShareDesc),
    m_curl(nullptr)
  {
    // call reset on the descriptor to incarnate it
    this->reset(false);
  }

  /**
   * Destructor, no-op
   */
  CurlDesc::~CurlDesc()
  {
    // create curl descriptor
    if (m_curl)
    {
      curl_easy_cleanup(m_curl);
      m_curl = (CURL *)0;
    }
  }

  /**
   * Reset the descriptor and make it ready to be reused
   *
   * @param cleanup
   *   if true, descriptor should be re-created
   */
  void CurlDesc::reset(bool cleanup)
  {
    CXX_LOG_TRACE("CurDesc::reset(): cleanup %d", cleanup);
    // create curl descriptor if needed
    if (!m_curl || cleanup)
    {
      // destroy if needed
      if (m_curl)
      {
        curl_easy_cleanup(m_curl);
        CXX_LOG_TRACE("CurDesc::reset(): curl_easy_cleanup %p", m_curl);
      }

      // create
      m_curl = curl_easy_init();
      CXX_LOG_TRACE("CurDesc::reset(): curl_easy_init %p", m_curl);

      // set shared descriptor if any
      if (m_shareCurl)
      {
        curl_easy_setopt(m_curl, CURLOPT_SHARE, m_shareCurl);
      }
    }
    else
    {
      // simply reset, shared stuff and open connections will survive...
      curl_easy_reset(m_curl);
      CXX_LOG_TRACE("CurDesc::reset(): curl_easy_reset %p", m_curl);
    }

    // allow redirect
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, true);

    // capture negotiated SSL version for diagnostics; clientp must be this CurlDesc
    curl_easy_setopt(m_curl, CURLOPT_PREREQFUNCTION, prereqCallback);
    curl_easy_setopt(m_curl, CURLOPT_PREREQDATA, (void*)this);
  }

  int CurlDesc::prereqCallback(void* clientp,
      char*, char*, int, int)
  {
      CurlDesc* curlDesc = (CurlDesc*)clientp;
      if (curlDesc)
      {
          curlDesc->updateNegotiatedSSLVersion();
      }
      return CURL_PREREQFUNC_OK;
  }

  void CurlDesc::updateNegotiatedSSLVersion()
  {
      m_negotiatedSSLVersion = "";
      if (!m_curl)
      {
          return;
      }
      const struct curl_tlssessioninfo* info = nullptr;
      CURLcode info_res = curl_easy_getinfo(m_curl, CURLINFO_TLS_SSL_PTR, &info);

      if ((info_res != CURLE_OK) || !info ||
          (info->backend != CURLSSLBACKEND_OPENSSL) ||
          !info->internals)
      {
          CXX_LOG_DEBUG("sf::CurlDesc::updateNegotiatedSSLVersion: negotiated SSL version info not available %d, %p",
              info_res, info);
          return;
      }
      // Cast internals to OpenSSL's SSL structure
      SSL* ssl_con = static_cast<SSL*>(info->internals);
      const char* ssl_version = SSL_get_version(ssl_con);
      if (!ssl_version)
      {
          return;
      }

      m_negotiatedSSLVersion = ssl_version;
      CXX_LOG_DEBUG("sf::CurlDesc::updateNegotiatedSSLVersion: negotiated SSL version %s",
          m_negotiatedSSLVersion.c_str());
  }

}
}
