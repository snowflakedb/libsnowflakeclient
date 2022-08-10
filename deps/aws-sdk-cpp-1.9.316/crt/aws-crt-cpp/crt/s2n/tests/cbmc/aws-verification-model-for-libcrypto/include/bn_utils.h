/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#ifndef BN_UTILS_H
#define BN_UTILS_H

#include <openssl/bn.h>
#include <stdbool.h>

bool bignum_is_valid(BIGNUM *bn);
BIGNUM *bignum_nondet_alloc();

#endif /* BN_UTILS_H */
