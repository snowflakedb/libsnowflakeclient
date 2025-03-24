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

#include <bn_utils.h>

#include <assert.h>
#include <math.h>

/*
 * Description: BN_new() allocates and initializes a BIGNUM structure.
 * Return values: BN_new() and BN_secure_new() return a pointer to the
 * BIGNUM initialised to the value 0. If the allocation fails, they
 * return NULL and set an error code that can be obtained by ERR_get_error(3).
 */
BIGNUM *BN_new(void) {
    BIGNUM *rv = malloc(sizeof(BIGNUM));
    if (rv) {
        rv->is_initialized = true;
        rv->d              = malloc(sizeof(*(rv->d)));
    }

    /* Assuming error codes can be safely ignored. */

    return rv;
}

/*
 * Description: BN_dup() creates a new BIGNUM containing the value from.
 * Return values: BN_dup() returns the new BIGNUM, and NULL on error.
 */
BIGNUM *BN_dup(const BIGNUM *from) {
    assert(bignum_is_valid(from));

    /* WARNING: somehow CBMC doesn't catch the NULL-ptr deref? */
    /* *dup = *from; */

    /* Guarantees that return value will be either NULL or initialized. */
    return BN_new();
}

/*
 * Description: BN_sub() subtracts b from a and places the result in
 * r (r=a-b). r may be the same BIGNUM as a or b.
 * Return values: For all functions, 1 is returned for success, 0 on error.
 * The return value should always be checked
 * (e.g., if (!BN_add(r,a,b)) goto err;).
 */
int BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) {
    assert(bignum_is_valid(r));
    assert(bignum_is_valid(a));
    assert(bignum_is_valid(b));

    r->is_initialized = nondet_bool();

    return r->is_initialized;
}

/*
 * Description: BN_free() frees the components of the BIGNUM,
 * and if it was created by BN_new(), also the structure itself.
 */
void BN_free(BIGNUM *a) {
    if (a != NULL) {
        free(a->d);
        free(a);
    }
}

/*
 * Description: BN_clear_free() additionally overwrites the data
 * before the memory is returned to the system. If a is NULL, nothing is done.
 */
void BN_clear_free(BIGNUM *a) {
    /* No way currently to model or check that the data is cleared. */
    BN_free(a);
}

int BN_is_zero(BIGNUM *a) {
    return (a->top == 0);
}

/* CBMC helper functions */

bool bignum_is_valid(BIGNUM *a) {
    return a && a->is_initialized;
}

BIGNUM *bignum_nondet_alloc() {
    return malloc(sizeof(BIGNUM));
}

BIGNUM *BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret) {
    assert(len == 0 || __CPROVER_r_ok(s, len));
    if (ret == NULL) {
        ret = BN_new();
    }
    return ret;
}

int BN_num_bits(const BIGNUM *a) {
    assert(a != NULL);
    unsigned int w;
    /* Basically, except for a zero, it returns floor(log2(w))+1. */
    __CPROVER_assume(w <= 65 /* floor(log2(SIZE_MAX))+1 */);
    return w;
}

int BN_bn2bin(const BIGNUM *a, unsigned char *to) {
    assert(a != NULL);
    assert(to != NULL);
    return nondet_int();
}
