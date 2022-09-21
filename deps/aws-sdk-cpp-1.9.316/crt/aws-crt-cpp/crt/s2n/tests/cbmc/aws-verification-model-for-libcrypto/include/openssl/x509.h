/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef X509_H
#define X509_H
/* x509 */

#define STACK_OF(type) type

typedef struct x509_st {
    /* See https://github.com/openssl/openssl/blob/master/include/openssl/x509.h.in */
} X509;

typedef struct x509_object_st {
    /* See https://github.com/openssl/openssl/blob/master/include/openssl/x509.h.in */
} X509_OBJECT;

typedef struct x509_store_st {
    /* See https://github.com/openssl/openssl/blob/master/include/openssl/x509.h.in */
} X509_STORE;

typedef struct x509_store_ctx_st {
    /* See https://github.com/openssl/openssl/blob/master/include/openssl/x509.h.in */
} X509_STORE_CTX;

#endif
