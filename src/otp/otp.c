#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "otp/base32.h"
#include "otp/hmac.h"
#include "otp/sha1.h"
#include "otp/otp.h"

static uint8_t *get_shared_secret(const char *secret_string, int *secretLen) {
  if (!secret_string) {
    return NULL;
  }

  // Decode secret key
  const int base32Len = strlen(secret_string);
  *secretLen = (base32Len*5 + 7)/8;
  uint8_t *secret = malloc(base32Len + 1);
  if (secret == NULL) {
    *secretLen = 0;
    return NULL;
  }
  memcpy(secret, secret_string, base32Len);
  secret[base32Len] = '\000';

  if ((*secretLen = base32_decode(secret, secret, base32Len)) < 1) {
    memset(secret, 0, base32Len);
    free(secret);
    return NULL;
  }
  memset(secret + *secretLen, 0, base32Len + 1 - *secretLen);

  return secret;
}

static int compute_code(const uint8_t *secret, int secretLen, unsigned long value) {
  uint8_t val[8];
  for (int i = 8; i--; value >>= 8) {
    val[i] = value;
  }
  uint8_t hash[SHA1_DIGEST_LENGTH];
  hmac_sha1(secret, secretLen, val, 8, hash, SHA1_DIGEST_LENGTH);
  memset(val, 0, sizeof(val));
  const int offset = hash[SHA1_DIGEST_LENGTH - 1] & 0xF;
  unsigned int truncatedHash = 0;
  for (int i = 0; i < 4; ++i) {
    truncatedHash <<= 8;
    truncatedHash  |= hash[offset + i];
  }
  memset(hash, 0, sizeof(hash));
  truncatedHash &= 0x7FFFFFFF;
  truncatedHash %= 1000000;
  return truncatedHash;
}

int totp_get_code(const char *secret, int skew, int *expires) {
  const int tm = time(NULL);
  int len;
  *expires = TOTP_STEP_SIZE - tm % TOTP_STEP_SIZE;
  return compute_code(get_shared_secret(secret, &len), len, (tm / TOTP_STEP_SIZE) + skew);
}

int hotp_get_code(const char *secret, int counter) {
  int len;
  return compute_code(get_shared_secret(secret, &len), len, counter);
}
