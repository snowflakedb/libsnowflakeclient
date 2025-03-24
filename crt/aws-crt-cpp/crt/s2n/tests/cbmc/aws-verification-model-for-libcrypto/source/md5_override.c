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
#include <openssl/md5.h>

#include <assert.h>

/*
 * Implemented from RFC1321 The MD5 Message-Digest Algorithm.
 * Per https://github.com/openssl/openssl/blob/33388b44b67145af2181b1e9528c381c8ea0d1b6/crypto/md5/md5_dgst.c.
 */
#define INIT_DATA_A (unsigned long)0x67452301L
#define INIT_DATA_B (unsigned long)0xefcdab89L
#define INIT_DATA_C (unsigned long)0x98badcfeL
#define INIT_DATA_D (unsigned long)0x10325476L

int MD5_Init(MD5_CTX *c) {
    assert(c != NULL);
    if (nondet_bool()) return 0;
    *c   = (const MD5_CTX){ 0 }; /* memset(c, 0, sizeof(*c)); */
    c->A = INIT_DATA_A;
    c->B = INIT_DATA_B;
    c->C = INIT_DATA_C;
    c->D = INIT_DATA_D;
    return 1;
}

int MD5_Final(unsigned char *md, MD5_CTX *c) {
    assert(__CPROVER_w_ok(md, MD5_DIGEST_LENGTH));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    __CPROVER_havoc_slice(md, MD5_DIGEST_LENGTH);
    *c = (const MD5_CTX){ 0 };
    return 1;
}

int MD5_Update(MD5_CTX *c, const void *data, size_t len) {
    assert(len == 0 || __CPROVER_w_ok(data, len));
    assert(c != NULL);
    if (nondet_bool()) return 0;
    return 1;
}
