/*
 * Changes to OpenSSL version 1.1.1.
 * Copyright Amazon.com, Inc. All Rights Reserved.
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef HEADER_MD5_H
#define HEADER_MD5_H

#define MD5_LONG unsigned int
#define MD5_CBLOCK 64
#define MD5_LBLOCK (MD5_CBLOCK / 4)
#define MD5_DIGEST_LENGTH 16

#include <stdlib.h>

typedef struct MD5state_st {
    /* Internal buffers used during the computation. */
    MD5_LONG A, B, C, D;
    /* The other subfields keep information on the running hash. */
    MD5_LONG Nl, Nh;
    MD5_LONG data[MD5_LBLOCK];
    unsigned int num;
} MD5_CTX;

int MD5_Init(MD5_CTX *c);
int MD5_Final(unsigned char *md, MD5_CTX *c);
int MD5_Update(MD5_CTX *c, const void *data, size_t len);

#endif
