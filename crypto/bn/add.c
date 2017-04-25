/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <GFp/bn.h>

#include <string.h>

#include <GFp/err.h>
#include <GFp/mem.h>

#include "internal.h"


int GFp_BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) {
  int max, min, dif;
  BN_ULONG *ap, *bp, *rp, carry, t1, t2;
  const BIGNUM *tmp;

  if (a->top < b->top) {
    tmp = a;
    a = b;
    b = tmp;
  }
  max = a->top;
  min = b->top;
  dif = max - min;

  if (!GFp_bn_wexpand(r, max + 1)) {
    return 0;
  }

  r->top = max;

  ap = a->d;
  bp = b->d;
  rp = r->d;

  carry = GFp_bn_add_words(rp, ap, bp, min);
  rp += min;
  ap += min;
  bp += min;

  if (carry) {
    while (dif) {
      dif--;
      t1 = *(ap++);
      t2 = (t1 + 1) & BN_MASK2;
      *(rp++) = t2;
      if (t2) {
        carry = 0;
        break;
      }
    }
    if (carry) {
      /* carry != 0 => dif == 0 */
      *rp = 1;
      r->top++;
    }
  }

  if (dif && rp != ap) {
    while (dif--) {
      /* copy remaining words if ap != rp */
      *(rp++) = *(ap++);
    }
  }

  r->neg = 0;
  return 1;
}

int GFp_BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) {
  int max;
  int add = 0, neg = 0;
  const BIGNUM *tmp;

  /*  a -  b	a-b
   *  a - -b	a+b
   * -a -  b	-(a+b)
   * -a - -b	b-a
   */
  if (a->neg) {
    if (b->neg) {
      tmp = a;
      a = b;
      b = tmp;
    } else {
      add = 1;
      neg = 1;
    }
  } else {
    if (b->neg) {
      add = 1;
      neg = 0;
    }
  }

  if (add) {
    if (!GFp_BN_uadd(r, a, b)) {
      return 0;
    }

    r->neg = neg;
    return 1;
  }

  /* We are actually doing a - b :-) */

  max = (a->top > b->top) ? a->top : b->top;
  if (!GFp_bn_wexpand(r, max)) {
    return 0;
  }

  if (GFp_BN_ucmp(a, b) < 0) {
    if (!GFp_BN_usub_unchecked(r, b, a)) {
      return 0;
    }
    r->neg = 1;
  } else {
    if (!GFp_BN_usub_unchecked(r, a, b)) {
      return 0;
    }
    r->neg = 0;
  }

  return 1;
}

int GFp_BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) {
  assert(!GFp_BN_is_negative(a));
  assert(!GFp_BN_is_negative(b));
  assert(GFp_BN_cmp(a, b) >= 0);
  return GFp_BN_usub_unchecked(r, a, b);
}


int GFp_BN_usub_unchecked(BIGNUM *r, const BIGNUM *a, const BIGNUM *b) {
  int max, min, dif;
  register BN_ULONG t1, t2, *ap, *bp, *rp;
  int i, carry;

  max = a->top;
  min = b->top;
  dif = max - min;

  if (dif < 0) /* hmm... should not be happening */
  {
    OPENSSL_PUT_ERROR(BN, BN_R_ARG2_LT_ARG3);
    return 0;
  }

  if (!GFp_bn_wexpand(r, max)) {
    return 0;
  }

  ap = a->d;
  bp = b->d;
  rp = r->d;

  carry = 0;
  for (i = min; i != 0; i--) {
    t1 = *(ap++);
    t2 = *(bp++);
    if (carry) {
      carry = (t1 <= t2);
      t1 = (t1 - t2 - 1) & BN_MASK2;
    } else {
      carry = (t1 < t2);
      t1 = (t1 - t2) & BN_MASK2;
    }
    *(rp++) = t1 & BN_MASK2;
  }

  if (carry) /* subtracted */
  {
    if (!dif) {
      /* error: a < b */
      return 0;
    }

    while (dif) {
      dif--;
      t1 = *(ap++);
      t2 = (t1 - 1) & BN_MASK2;
      *(rp++) = t2;
      if (t1) {
        break;
      }
    }
  }

  if (dif > 0 && rp != ap) {
    memcpy(rp, ap, sizeof(*rp) * dif);
  }

  r->top = max;
  r->neg = 0;
  GFp_bn_correct_top(r);

  return 1;
}
