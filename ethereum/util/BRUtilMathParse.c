//
//  BBRUtilMathParse.c
//  breadwallet-core Ethereum
//
//  Created by Ed Gamble on 3/16/2018.
//  Copyright (c) 2018 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <regex.h>
#include "BRUtil.h"

//
// Parsing
//

#define INTEGER_REGEX "^[0-9]+$"
#define DECIMAL_REGEX "^[0-9]+\\.[0-9]*$"

extern BRUtilMathParseStatus
parseIsInteger (const char *number) {
  static regex_t regex;
  static int regexInitialized = 0;

  if (!regexInitialized) {
    regcomp(&regex, INTEGER_REGEX, REG_EXTENDED);
    regexInitialized = 1;
  }

  return (0 == regexec (&regex, number, 0, NULL, 0)
          ? UTIL_MATH_PARSE_OK
          : UTIL_MATH_PARSE_STRANGE_DIGITS);
}

extern BRUtilMathParseStatus
parseIsDecimal (const char *number) {
  static regex_t regex;
  static int regexInitialized = 0;

  if (!regexInitialized) {
    regcomp(&regex, DECIMAL_REGEX, REG_EXTENDED);
    regexInitialized = 1;
  }

  return (0 == regexec (&regex, number, 0, NULL, 0)
          ? UTIL_MATH_PARSE_OK
          : parseIsInteger (number));
}


#define SURELY_ENOUGH_CHARS 100     // No more than ~78 in UInt256

extern UInt256
createUInt256ParseDecimal (const char *string, int decimals, int *error) {
  if (UTIL_MATH_PARSE_OK != parseIsDecimal(string)
      || strlen(string) >= SURELY_ENOUGH_CHARS
      || decimals >= SURELY_ENOUGH_CHARS) {
    *error = 1;
    return UINT256_ZERO;
  }

  if (UTIL_MATH_PARSE_OK == parseIsInteger(string))
    return createUInt256Parse(string, 10, error);

  // Get a 'new' string so strsep() can overwrite it.
  char splitterA [SURELY_ENOUGH_CHARS];
  char *splitter = splitterA;
  strcpy (splitter, string);

  char *whole = strsep(&splitter, ".");
  char *fract = strsep(&splitter, ".");

  if (NULL == whole) whole = "";
  if (NULL == fract) fract = "";

  unsigned long fractLen = strlen(fract);

  // Strip trailing '0'
  for (int i = 0; i < fractLen; i++) {
    if ('0' == fract[fractLen - 1 - i])
      fract[fractLen - 1 - i] = '\0';
    else
      break;
  }

  // Too many fractional digits
  fractLen = strlen(fract);
  if (fractLen > decimals) {
    *error = 1;
    return UINT256_ZERO;
  }

  // Get a 'new' string with `whole`, `fract` and some padding '0' tacked on.
  char number [2 * SURELY_ENOUGH_CHARS];
  strcpy(number, whole);
  strcat(number, fract);

  char *padding = &number[strlen(number)];
  // Pad with '0'
  for (; fractLen < decimals; fractLen++)
    *padding++ = '0';
  *padding = 0;

  return createUInt256Parse(number, 10, error);
}


//    > max064
//    18446744073709551615
//    > (number->string max064 2)
//    "1111111111111111111111111111111111111111111111111111111111111111"
//    > (string-length (number->string max064 2))
//    64
//    > (number->string max064 10)
//    "18446744073709551615"
//    > (string-length (number->string max064 10))
//    20
//    >(number->string max064 16)
//    "FFFFFFFFFFFFFFFF"
//    > (string-length (number->string max064 16))
//    16

// The maximum digits allowed in a string so as to prevent overflow in uint64_t
static int
parseMaximumDigitsForUInt64InBase (int base) {
  switch (base) {
    case 2:  return 64;
    case 10: return 19;
    case 16: return 16;
    default: return -1;
  }
}

static int
parseMaximumDigitsForUInt256InBase (int base) {
  switch (base) {
    case 2:  return 256;
    case 10: return 78;
    case 16: return 64;
    default: return 0;
  }
}

// Compute (expt base power)
static UInt256
parseUInt256Power (int base, int power, int *overflow) {
  int maxPower = parseMaximumDigitsForUInt64InBase(base);

  if (power > maxPower) {
    *overflow = 1;
    return UINT256_ZERO;
  }

  uint64_t value = 1;
  while (power-- > 0)
    value *= base;  // slow, but yeah.

  // Reality is that this can overflow for 16^16 = 2^64 (one too many).  We'll handle it.
  //  Probably also for 2^256... can't handle that one similarly.
  *overflow = 0;
  if (value == 0) {
    UInt256 result = UINT256_ZERO;
    result.u64[1] = 1;
    return result;
  }
  else return createUInt256(value);
}

// Compute (* value (expt base power))
static UInt256
parseUInt256ScaleByPower (UInt256 value, int base, int power, int *overflow) {
  UInt256 scale = parseUInt256Power(base, power, overflow);
  return (*overflow
          ? UINT256_ZERO
          : mulUInt256_Overflow(value, scale, overflow));
}

static UInt256
parseUInt64 (const char *string, int digits, int base) {
  int maxDigits = parseMaximumDigitsForUInt64InBase(base);
  assert (digits <= maxDigits );

  //
  char number[1 + maxDigits];
  strncpy (number, string, maxDigits);
  number[maxDigits] = '\0';

  uint64_t value = strtoull (number, NULL, base);
  return createUInt256 (value);
}

extern UInt256
createUInt256Parse (const char *string, int base, int *error) {
  assert (NULL != error);

  UInt256 value = UINT256_ZERO;
  int maxDigits = parseMaximumDigitsForUInt256InBase(base);
  long length = strlen (string);

  if (length > maxDigits) {  // overflow
    *error = 1;
    return UINT256_ZERO;
  }

  // We'll process this many digits in `string`.
  int stringChunks = parseMaximumDigitsForUInt64InBase(base);

  for (long index = 0; index < length; index += stringChunks) {
    // On the first time through, get an initial value
    if (index == 0)
      value = parseUInt64(string, stringChunks, base);

    // Otherwise, we'll scale value and add in the next chunk.
    else {
      // This many remain - if more than stringChunks, we'll scale by just stringChunks
      int remainingDigits = (int) (length - index);
      int scalingDigits = remainingDigits >= stringChunks ? stringChunks : remainingDigits;

      int scaleOverflow = 0, addOverflow = 0;

      // Scale (aka shift the value by scalingDigits)
      value = parseUInt256ScaleByPower(value, base, scalingDigits, &scaleOverflow);

      // Add in the next chuck.
      value = addUInt256_Overflow(value, parseUInt64(&string[index], stringChunks, base), &addOverflow);
      if (scaleOverflow || addOverflow) {
        *error = 1;
        return UINT256_ZERO;
      }
    }
  }
  *error = 0;
  return value;
}

//
//
//
static char *
coerceReverseString (const char *s) {
  long len = strlen(s);
  char *t = calloc (1 + len, 1);
  for (int i = 0; i < len; i++)
    t[len - i - 1] = s[i];
  return t;
}

extern char *
coerceString (UInt256 x, int base) {
  // Handle 0 explicitly, rather than in each case
  if (eqUInt256(x, UINT256_ZERO)) {
    char *result = calloc (2, 1);
    result[0] = '0';
    return result;
  }

  // otherwise, full stop
  switch (base) {
    case 16: {
      // Reverse and 'strip zeros'
      UInt256 xr = UInt256Reverse(x);
      int xrIndex = 0;
      while (0 == xr.u8[xrIndex]) xrIndex++;
      // Encode
      return encodeHexCreate (NULL, &xr.u8[xrIndex], sizeof (xr.u8) - xrIndex);
    }
    case 10: {
      char r[256];
      memset (r, 0, 256);
      for (int i = 0; i < 256 && !eqUInt256(x, UINT256_ZERO); i++) {
        uint32_t rem;
        x = divUInt256_Small(x, base, &rem);
        r[i] = '0' + rem;
      }
      return coerceReverseString(r);
    }
    case 2:
      assert (0);
    default:
      assert (0);
  }
}

extern char * 
coerceStringDecimal (UInt256 x, int decimals) {
  char *string = coerceString(x, 10);

  if (0 == decimals)
    return string;

  int slength = (int) strlen (string);
  if (decimals >= slength) {
    char *result = calloc (decimals + 3, 1);  // 0.<decimals>'\0'

    // Fill to decimals
    char format [10];
    sprintf (format, "0.%%%ds", decimals);
    sprintf (result, format, string);

    // Replace fills of ' ' with '0'
    for (int i = 0; i < decimals + 2; i++) {
      if (' ' == result[i])
        result[i] = '0';
    }
    free (string);
    return result;
  }
  else {
    int dindex = slength - decimals;
    char *result = calloc (slength + 2, 1);  // <whole>.<decimals>'\0'
    strncpy (result, string, dindex);
    result[dindex] = '.';
    strcpy (&result[dindex+1], &string[dindex]);
    free (string);
    return result;
  }
}