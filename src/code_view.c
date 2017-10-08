#include <app.h>
#include <dlog.h>
#include "util/base32.h"
#include "util/hmac.h"
#include "util/sha1.h"
#include "otp.h"

#define TOTP_STEP_SIZE 30

#define NAME_LABEL "<font font_weight=Bold font_size=40><align=center> %s </align></font>"
#define CODE_LABEL "<font font_weight=Regular font_size=75>%06d</font>"

static uint8_t *_get_shared_secret(const char *secret_string, int *secretLen) {
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

static int _compute_code(const uint8_t *secret, int secretLen, unsigned long value) {
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

int _totp_get_code(const char *secret, int skew, int *expires) {
  const int tm = time(NULL);
  int len;
  *expires = TOTP_STEP_SIZE - tm % TOTP_STEP_SIZE;
  return _compute_code(_get_shared_secret(secret, &len), len, (tm / TOTP_STEP_SIZE) + skew);
}

static int _hotp_get_code(const char *secret, int counter) {
  int len;
  return _compute_code(_get_shared_secret(secret, &len), len, counter);
}

static Eina_Bool code_view_pop_cb(void *data, Elm_Object_Item *it)
{
  appdata_s *ad = (appdata_s *) data;
  code_view_data_s *cvd = ad->current_cvd;
  if (cvd) {
    ad->current_cvd = NULL;
    if (cvd->timer) ecore_timer_del(cvd->timer);
    evas_object_hide(cvd->progressbar);
    free(cvd);
  }

  return EINA_TRUE;
}

static void refresh_code(code_view_data_s *cvd) {
  int expires = 0;
  char code[255];
  otp_info_s *entry = (otp_info_s *) (cvd->entry);

  if (entry != NULL) {
    snprintf(code, 255, CODE_LABEL, _totp_get_code(entry->secret, 0, &expires));
    elm_object_text_set(cvd->code_label, code);
  }

  cvd->seconds = expires;
}

static Eina_Bool refresh_view_cb(void *data) {
  code_view_data_s *cvd = data;

  if (--cvd->seconds < 1) {
    refresh_code(cvd);
  }

  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

  return ECORE_CALLBACK_RENEW;
}

void code_view_create(appdata_s *ad, otp_info_s *entry)
{
  code_view_data_s *cvd = calloc(1, sizeof(code_view_data_s));
  cvd->entry = entry;
  ad->current_cvd = cvd;

  /* Box */
  cvd->box = elm_box_add(ad->nf);
  elm_box_align_set(cvd->box, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(cvd->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show(cvd->box);

  /* Name label */
  cvd->name_label = elm_label_add(cvd->box);
  evas_object_size_hint_align_set(cvd->name_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(cvd->name_label, EVAS_HINT_EXPAND, 0);

  char label[255], issuer[255], account[255];
  get_otp_account(cvd->entry->user, account);
  if (get_otp_issuer(cvd->entry->user, issuer)) {
    char res[255];
    snprintf(res, 255, "%s <br/> %s", issuer, account);
    snprintf(label, 255, NAME_LABEL, res);
  } else {
    snprintf(label, 255, NAME_LABEL, account);
  }

  elm_object_text_set(cvd->name_label, label);
  evas_object_show(cvd->name_label);
  elm_box_pack_end(cvd->box, cvd->name_label);

  /* Code label */
  cvd->code_label = elm_label_add(cvd->box);
  evas_object_size_hint_align_set(cvd->code_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(cvd->code_label, EVAS_HINT_EXPAND, 0);

  refresh_code(cvd);
  evas_object_show(cvd->code_label);
  elm_box_pack_end(cvd->box, cvd->code_label);

  /* Progress */
  cvd->progressbar = eext_circle_object_progressbar_add(cvd->box, ad->circle_surface);
  eext_circle_object_value_min_max_set(cvd->progressbar, 0, TOTP_STEP_SIZE);
  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

  evas_object_show(cvd->progressbar);

  /* Schedule update */
  cvd->timer = ecore_timer_add(1.0f, refresh_view_cb, cvd);

  Elm_Object_Item *nf_it = elm_naviframe_item_push(ad->nf, NULL, NULL, NULL, cvd->box, "empty");
  elm_naviframe_item_pop_cb_set(nf_it, code_view_pop_cb, ad);
}

void code_view_resume(code_view_data_s *cvd) {
  if (cvd != NULL) return;

  refresh_code(cvd);
  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);
}
