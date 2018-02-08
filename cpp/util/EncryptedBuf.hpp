//
// Created by hyu on 2/7/18.
//

#ifndef SNOWFLAKECLIENT_ENCRYPTEDBUF_HPP
#define SNOWFLAKECLIENT_ENCRYPTEDBUF_HPP

namespace Snowflake
{
  namespace Client
  {
    namespace Util
    {
      class EncryptedBuf : public std::basic_filebuf
      {
        virtual int underflow();

        virtual int sync();
      };
    }
  }
}


#endif //SNOWFLAKECLIENT_ENCRYPTEDBUF_HPP
