/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <ec_utils.h>
#include <openssl/sha.h>

#include <assert.h>

/*
 * All macros extracted from the original openssl implementation.
 * Per https://github.com/openssl/openssl/blob/e39e295e205ab8461d3ac814129bbb08c2d1266d/crypto/sha/sha_local.h.
 */
#define INIT_DATA_h0 0x67452301UL
#define INIT_DATA_h1 0xefcdab89UL
#define INIT_DATA_h2 0x98badcfeUL
#define INIT_DATA_h3 0x10325476UL
#define INIT_DATA_h4 0xc3d2e1f0UL

int SHA1_Init(SHA_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    *c    = (const SHA_CTX){ 0 }; /* memset(c, 0, sizeof(*c)); */
    c->h0 = INIT_DATA_h0;
    c->h1 = INIT_DATA_h1;
    c->h2 = INIT_DATA_h2;
    c->h3 = INIT_DATA_h3;
    c->h4 = INIT_DATA_h4;
    return 1;
}

int SHA224_Init(SHA256_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    *c        = (const SHA256_CTX){ 0 }; /* memset(c, 0, sizeof(*c)); */
    c->h[0]   = 0xc1059ed8UL;
    c->h[1]   = 0x367cd507UL;
    c->h[2]   = 0x3070dd17UL;
    c->h[3]   = 0xf70e5939UL;
    c->h[4]   = 0xffc00b31UL;
    c->h[5]   = 0x68581511UL;
    c->h[6]   = 0x64f98fa7UL;
    c->h[7]   = 0xbefa4fa4UL;
    c->md_len = SHA224_DIGEST_LENGTH;
    return 1;
}

int SHA256_Init(SHA256_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    *c        = (const SHA256_CTX){ 0 }; /* memset(c, 0, sizeof(*c)); */
    c->h[0]   = 0x6a09e667UL;
    c->h[1]   = 0xbb67ae85UL;
    c->h[2]   = 0x3c6ef372UL;
    c->h[3]   = 0xa54ff53aUL;
    c->h[4]   = 0x510e527fUL;
    c->h[5]   = 0x9b05688cUL;
    c->h[6]   = 0x1f83d9abUL;
    c->h[7]   = 0x5be0cd19UL;
    c->md_len = SHA256_DIGEST_LENGTH;
    return 1;
}

int SHA384_Init(SHA512_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    c->h[0] = U64(0xcbbb9d5dc1059ed8);
    c->h[1] = U64(0x629a292a367cd507);
    c->h[2] = U64(0x9159015a3070dd17);
    c->h[3] = U64(0x152fecd8f70e5939);
    c->h[4] = U64(0x67332667ffc00b31);
    c->h[5] = U64(0x8eb44a8768581511);
    c->h[6] = U64(0xdb0c2e0d64f98fa7);
    c->h[7] = U64(0x47b5481dbefa4fa4);

    c->Nl     = 0;
    c->Nh     = 0;
    c->num    = 0;
    c->md_len = SHA384_DIGEST_LENGTH;
    return 1;
}

int SHA512_Init(SHA512_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    c->h[0] = U64(0x6a09e667f3bcc908);
    c->h[1] = U64(0xbb67ae8584caa73b);
    c->h[2] = U64(0x3c6ef372fe94f82b);
    c->h[3] = U64(0xa54ff53a5f1d36f1);
    c->h[4] = U64(0x510e527fade682d1);
    c->h[5] = U64(0x9b05688c2b3e6c1f);
    c->h[6] = U64(0x1f83d9abfb41bd6b);
    c->h[7] = U64(0x5be0cd19137e2179);

    c->Nl     = 0;
    c->Nh     = 0;
    c->num    = 0;
    c->md_len = SHA512_DIGEST_LENGTH;
    return 1;
}

int SHA1_Final(unsigned char *md, SHA_CTX *c) {
    assert(__CPROVER_w_ok(md, SHA_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, SHA_DIGEST_LENGTH);
    *c = (const SHA_CTX){ 0 };
    return 1;
}

int SHA224_Final(unsigned char *md, SHA256_CTX *c) {
    assert(__CPROVER_w_ok(md, SHA224_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, SHA224_DIGEST_LENGTH);
    *c = (const SHA256_CTX){ 0 };
    return 1;
}

int SHA256_Final(unsigned char *md, SHA256_CTX *c) {
    assert(__CPROVER_w_ok(md, SHA256_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, SHA256_DIGEST_LENGTH);
    *c = (const SHA256_CTX){ 0 };
    return 1;
}

int SHA384_Final(unsigned char *md, SHA512_CTX *c) {
    assert(__CPROVER_w_ok(md, SHA384_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, SHA384_DIGEST_LENGTH);
    *c = (const SHA512_CTX){ 0 };
    return 1;
}

int SHA512_Final(unsigned char *md, SHA512_CTX *c) {
    assert(__CPROVER_w_ok(md, SHA512_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, SHA512_DIGEST_LENGTH);
    *c = (const SHA512_CTX){ 0 };
    return 1;
}

int SHA1_Update(SHA_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}

int SHA224_Update(SHA256_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}

int SHA256_Update(SHA256_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}

int SHA384_Update(SHA512_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}

int SHA512_Update(SHA512_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}
