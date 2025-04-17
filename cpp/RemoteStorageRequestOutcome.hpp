#ifndef SNOWFLAKECLIENT_TRANSFEROUTCOME_HPP
#define SNOWFLAKECLIENT_TRANSFEROUTCOME_HPP

namespace Snowflake
{
  namespace Client
  {

    /**
     * File Transfer result from remote storage client.
     */
    enum RemoteStorageRequestOutcome
    {
      SUCCESS,
      FAILED,
      TOKEN_EXPIRED,
      SKIP_UPLOAD_FILE
    };

  }
}
#endif // SNOWFLAKECLIENT_TRANSFEROUTCOME_HPP
