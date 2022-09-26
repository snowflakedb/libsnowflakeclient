/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include "../../internal.h"

#if defined(OPENSSL_AARCH64) && defined(OPENSSL_LINUX) && \
    !defined(OPENSSL_STATIC_ARMCAP)

#include <sys/auxv.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/arm_arch.h>


extern uint32_t OPENSSL_armcap_P;

// handle_cpu_env applies the value from |in| to the CPUID values in |out[0]|.
// See the comment in |OPENSSL_cpuid_setup| about this.
static void handle_cpu_env(uint32_t *out, const char *in) {
  const int invert = in[0] == '~';
  const int or = in[0] == '|';
  const int skip_first_byte = invert || or;
  const int hex = in[skip_first_byte] == '0' && in[skip_first_byte+1] == 'x';
  uint32_t armcap = out[0];

  int sscanf_result;
  uint32_t v;
  if (hex) {
    sscanf_result = sscanf(in + skip_first_byte + 2, "%" PRIx32, &v);
  } else {
    sscanf_result = sscanf(in + skip_first_byte, "%" PRIu32, &v);
  }

  if (!sscanf_result) {
    return;
  }

  // Detect if the user is trying to use the environment variable to set
  // a capability that is _not_ available on the CPU:
  // If getauxval() returned a non-zero hwcap in `armcap` (out)
  // and a bit set in the requested `v` is not set in `armcap`,
  // abort instead of crashing later.
  // The case of invert cannot enable an unexisting capability;
  // it can only disable an existing one.
  if (!invert && armcap && (~armcap & v))
  {
    fprintf(stderr,
            "Fatal Error: HW capability found: 0x%02X, but HW capability requested: 0x%02X.\n",
            armcap, v);
    exit(1);
  }

  if (invert) {
    out[0] &= ~v;
  } else if (or) {
    out[0] |= v;
  } else {
    out[0] = v;
  }
}

void OPENSSL_cpuid_setup(void) {
  unsigned long hwcap = getauxval(AT_HWCAP);

  // See /usr/include/asm/hwcap.h on an aarch64 installation for the source of
  // these values.
  static const unsigned long kNEON = 1 << 1;
  static const unsigned long kAES = 1 << 3;
  static const unsigned long kPMULL = 1 << 4;
  static const unsigned long kSHA1 = 1 << 5;
  static const unsigned long kSHA256 = 1 << 6;
  static const unsigned long kSHA512 = 1 << 21;

  if ((hwcap & kNEON) == 0) {
    // Matching OpenSSL, if NEON is missing, don't report other features
    // either.
    return;
  }

  OPENSSL_armcap_P |= ARMV7_NEON;

  if (hwcap & kAES) {
    OPENSSL_armcap_P |= ARMV8_AES;
  }
  if (hwcap & kPMULL) {
    OPENSSL_armcap_P |= ARMV8_PMULL;
  }
  if (hwcap & kSHA1) {
    OPENSSL_armcap_P |= ARMV8_SHA1;
  }
  if (hwcap & kSHA256) {
    OPENSSL_armcap_P |= ARMV8_SHA256;
  }
  if (hwcap & kSHA512) {
    OPENSSL_armcap_P |= ARMV8_SHA512;
  }

  // OPENSSL_armcap is a 32-bit, unsigned value which may start with "0x" to
  // indicate a hex value. Prior to the 32-bit value, a '~' or '|' may be given.
  //
  // If the '~' prefix is present:
  //   the value is inverted and ANDed with the probed CPUID result
  // If the '|' prefix is present:
  //   the value is ORed with the probed CPUID result
  // Otherwise:
  //   the value is taken as the result of the CPUID
  const char *env;
  env = getenv("OPENSSL_armcap");
  if (env != NULL) {
    handle_cpu_env(&OPENSSL_armcap_P, env);
  }
}

#endif  // OPENSSL_AARCH64 && OPENSSL_LINUX && !OPENSSL_STATIC_ARMCAP
