#include "snowflake/CurlDesc.hpp"
#include "../logger/SFLogger.hpp"

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
  }

}
}
