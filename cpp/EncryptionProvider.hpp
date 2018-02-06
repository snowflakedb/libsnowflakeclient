//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP
#define SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP

namespace Snowflake
{
  namespace Client
  {
    /**
     * Wrapper on top of openssl to do file encryption/decryption
     */
    class EncryptionProvider
    {
    public:

      static char* encrypt();

      static char* decrypt();
    };
  }
}


#endif //SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP
