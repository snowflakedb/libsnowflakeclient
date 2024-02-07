#include "hash.h"

#include "base64.h"

namespace azure {  namespace storage_lite {
    std::string hash(const std::string &to_sign, const std::vector<unsigned char> &key)
    {
        unsigned int l = SHA256_DIGEST_LENGTH;
        unsigned char digest[SHA256_DIGEST_LENGTH];

#ifdef USE_OPENSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        HMAC_CTX ctx;
        HMAC_CTX_init(&ctx);
        HMAC_Init_ex(&ctx, key.data(), static_cast<int>(key.size()), EVP_sha256(), NULL);
        HMAC_Update(&ctx, (const unsigned char*)to_sign.c_str(), to_sign.size());
        HMAC_Final(&ctx, digest, &l);
        HMAC_CTX_cleanup(&ctx);
#elif OPENSSL_VERSION_NUMBER < 0x30000000L
        HMAC_CTX * ctx = HMAC_CTX_new();
        HMAC_CTX_reset(ctx);
        HMAC_Init_ex(ctx, key.data(), key.size(), EVP_sha256(), NULL);
        HMAC_Update(ctx, (const unsigned char*)to_sign.c_str(), to_sign.size());
        HMAC_Final(ctx, digest, &l);
        HMAC_CTX_free(ctx);
#else
        HMAC(EVP_sha256(), &key[0], key.size(),(unsigned char*)&to_sign[0], to_sign.length(), digest, &l);
#endif
#else
        gnutls_hmac_fast(GNUTLS_MAC_SHA256, key.data(), key.size(), (const unsigned char *)to_sign.data(), to_sign.size(), digest);
#endif

        return to_base64(std::vector<unsigned char>(digest, digest + l));
    }
}}  // azure::storage_lite
