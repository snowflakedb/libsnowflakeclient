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

#include <openssl/dh.h>
#include <openssl/ossl_typ.h>

bool openssl_DH_is_valid(const DH *dh) {
    return __CPROVER_w_ok(dh, sizeof(*dh));
}

DH *DH_new(void) {
    DH *dh = malloc(sizeof(*dh));
    if (dh != NULL) {
        dh->pub_key  = BN_new();
        dh->priv_key = BN_new();
        dh->p        = BN_new();
        dh->q        = BN_new();
        dh->g        = BN_new();
    }
    return dh;
}

int DH_size(const DH *dh) {
    /**
     * Both dh and dh->p must not be NULL.
     * Per https://www.openssl.org/docs/man1.1.0/man3/DH_size.html.
     */
    assert(openssl_DH_is_valid(dh));
    assert(dh->p != NULL);
    int size;
    __CPROVER_assume(0 < size);
    return size;
}

void DH_free(DH *dh) {
    assert(dh == NULL || openssl_DH_is_valid(dh));
    if (dh != NULL) {
        BN_free(dh->pub_key);
        BN_free(dh->priv_key);
        BN_free(dh->p);
        BN_free(dh->q);
        BN_free(dh->g);
        free(dh);
    }
    return;
}

/* Returns a dummy DH that can't be dereferenced. */
DH *d2i_DHparams(DH **a, const unsigned char **pp, long length) {
    assert(pp != NULL);
    DH *dummy_dh = malloc(sizeof(*dummy_dh));
    if (dummy_dh != NULL) {
        dummy_dh->pub_key  = BN_new();
        dummy_dh->priv_key = BN_new();
        dummy_dh->p        = BN_new();
        dummy_dh->g        = BN_new();
        dummy_dh->q        = BN_new();
        if (a != NULL) *a = dummy_dh;
    }
    if (nondet_bool() && *pp != NULL) {
        *pp = *pp + length;
    }
    return dummy_dh;
}

int DH_check(DH *dh, int *codes) {
    /**
     * Only check for nullness at this point, since we need to re-evaluate all validty functions.
     * See https://github.com/awslabs/aws-verification-model-for-libcrypto/issues/17.
     * */
    assert(dh != NULL);
    assert(codes != NULL);
    *codes = nondet_int();
    return (int)nondet_bool();
}

/**
 * The p, q and g parameters can be obtained by calling DH_get0_pqg().
 * If the parameters have not yet been set then
 * *p, *q and *g will be set to NULL.
 * Per https://www.openssl.org/docs/man1.1.0/man3/DH_get0_pqg.html.
 */
void DH_get0_pqg(const DH *dh, BIGNUM **p, const BIGNUM **q, const BIGNUM **g) {
    assert(openssl_DH_is_valid(dh));
    if (p != NULL) {
        if (dh->p != NULL) {
            *p = dh->p;
        } else {
            *p = NULL;
        }
    }
    if (q != NULL) {
        if (dh->q != NULL) {
            *q = dh->q;
        } else {
            *q = NULL;
        }
    }
    if (g != NULL) {
        if (dh->g != NULL) {
            *g = dh->g;
        } else {
            *g = NULL;
        }
    }
}

int DH_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh) {
    assert(pub_key != NULL);
    assert(dh != NULL);
    return nondet_bool() ? DH_size(dh) : -1;
}

int DH_generate_key(DH *dh) {
    /**
     * DH_generate_key() expects dh to contain the shared parameters dh->p and dh->g.
     * Per https://www.openssl.org/docs/man1.1.0/man3/DH_generate_key.html.
     */
    assert(dh != NULL);
    assert(dh->p != NULL);
    assert(dh->g != NULL);
    return (int)nondet_bool();
}

DH *DHparams_dup(const DH *dh) {
    DH *ret;
    ret = DH_new();
    if (ret == NULL) return NULL;
    ret->pad     = dh->pad;
    ret->version = dh->version;
    ret->params  = dh->params;
    ret->length  = dh->length;
    ret->flags   = dh->flags;
    if (dh->pub_key != NULL) {
        __CPROVER_assume(ret->pub_key != NULL);
        *ret->pub_key = *dh->pub_key;
    } else {
        ret->pub_key = NULL;
    }
    if (dh->priv_key != NULL) {
        __CPROVER_assume(ret->priv_key != NULL);
        *ret->priv_key = *dh->priv_key;
    } else {
        ret->priv_key = NULL;
    }
    if (dh->p != NULL) {
        __CPROVER_assume(ret->p != NULL);
        *ret->p = *dh->p;
    } else {
        ret->p = NULL;
    }
    if (dh->g != NULL) {
        __CPROVER_assume(ret->g != NULL);
        *ret->g = *dh->g;
    } else {
        ret->g = NULL;
    }
    return ret;
}
