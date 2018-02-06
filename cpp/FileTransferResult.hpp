//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_IRESULTSET_HPP
#define SNOWFLAKECLIENT_IRESULTSET_HPP

namespace Snowflake
{
  namespace Client
  {
    /**
     * Class wrapped up the information about file transfer information
     * If succeed, includes the information about dest file name/size etc.
     *
     * External component needs build a result set wrapper on top of this class
     */
    class FileTransferResult
    {

    };
  }
}

#endif //SNOWFLAKECLIENT_IRESULTSET_HPP
