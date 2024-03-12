/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#ifndef EVP_UTILS_H
#define EVP_UTILS_H

#include <ec_utils.h>
#include <openssl/evp.h>

#define IMPLIES(a, b) (!(a) || (b))
/**
 * If and only if (iff) is a biconditional logical connective between statements a and b.
 * Equivalent to (IMPLIES(a, b) && IMPLIES(b, a)).
 */
#define IFF(a, b) (!!(a) == !!(b))

size_t evp_md_ctx_get_digest_size(EVP_MD_CTX *ctx);

EVP_PKEY *evp_md_ctx_get0_evp_pkey(EVP_MD_CTX *ctx);

bool evp_md_ctx_is_initialized(EVP_MD_CTX *ctx);

bool evp_md_ctx_is_valid(EVP_MD_CTX *ctx);

EVP_MD_CTX *evp_md_ctx_nondet_alloc();

void evp_md_ctx_set0_evp_pkey(EVP_MD_CTX *ctx, EVP_PKEY *pkey);

void evp_md_ctx_shallow_free(EVP_MD_CTX *ctx);

int evp_pkey_get_reference_count(EVP_PKEY *pkey);

bool evp_pkey_is_valid(EVP_PKEY *pkey);

EVP_PKEY *evp_pkey_nondet_alloc();

void evp_pkey_set0_ec_key(EVP_PKEY *pkey, EC_KEY *ec);

void evp_pkey_unconditional_free(EVP_PKEY *pkey);

#endif
