#include <app.h>
#include <dlog.h>
#include <system_info.h>
#include "util/base32.h"
#include "util/hmac.h"
#include "util/sha1.h"
#include "otp.h"
#include "database.h"

#define TOTP_STEP_SIZE 30

#define NAME_LABEL "<font font_weight=Regular font_size=30><align=center>%s</align></font>"
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

static int _totp_get_code(const char *secret, int skew, int *expires) {
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
    if (cvd->progressbar) evas_object_hide(cvd->progressbar);
    free(cvd);
  }

  return EINA_TRUE;
}

static void refresh_entry(code_view_data_s *cvd) {
  GList *entry = NULL;
  if(db_select_id(&entry, cvd->entry->id) == SQLITE_OK) {
    memcpy(cvd->entry, entry->data, sizeof(otp_info_s));
    g_list_free_full(entry, free);
  }
}

static void refresh_code(code_view_data_s *cvd) {
  int expires = 0;
  char code[255];
  otp_info_s *entry = (otp_info_s *) (cvd->entry);

  if (cvd->entry->type == TOTP) {
    if (entry != NULL) {
      snprintf(code, 255, CODE_LABEL, _totp_get_code(entry->secret, 0, &expires));
      elm_object_text_set(cvd->code_label, code);
    }
    cvd->seconds = expires;
  } else {
    refresh_entry(cvd);
    snprintf(code, 255, CODE_LABEL, _hotp_get_code(entry->secret, entry->counter++));
    db_inc_counter(cvd->entry->id);
    elm_object_text_set(cvd->code_label, code);
  }
}

static Eina_Bool refresh_view_totp_cb(void *data) {
  code_view_data_s *cvd = data;

  if (--cvd->seconds < 1) {
    refresh_code(cvd);
  }

  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

  return ECORE_CALLBACK_RENEW;
}

static void renew_button_cb(void *data, Evas_Object *obj, void *event_info)
{
  refresh_code(data);
	return;
}

void code_view_create(appdata_s *ad, otp_info_s *entry)
{
  code_view_data_s *cvd = calloc(1, sizeof(code_view_data_s));
  cvd->entry = entry;
  ad->current_cvd = cvd;

  cvd->layout =  elm_layout_add(ad->nf);
	elm_layout_theme_set(cvd->layout, "layout", "bottom_button", "default");
	evas_object_show(cvd->layout);

  /* Box */
  Evas_Object *box = elm_box_add(cvd->layout);
  evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show(box);
	elm_object_part_content_set(cvd->layout, "elm.swallow.content", box);

  /* Name label */
  Evas_Object *name_label = elm_label_add(box);
  evas_object_size_hint_align_set(name_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(name_label, 0, 0);

  int ww;
  if(system_info_get_platform_int("tizen.org/feature/screen.width", &ww) == SYSTEM_INFO_ERROR_NONE) {
    ww = 350;
  }
  elm_label_wrap_width_set(name_label, ww * 0.9);
  elm_label_slide_mode_set(name_label, ELM_LABEL_SLIDE_MODE_AUTO);
  elm_object_style_set(name_label, "slide_bounce");
  elm_label_slide_duration_set(name_label, 2);

  char label[255], issuer[255], account[255];
  get_otp_account(cvd->entry->label, account);
  if (get_otp_issuer(cvd->entry->label, issuer)) {
    char res[255];
    snprintf(res, 255, "%s<br/>%s", issuer, account);
    snprintf(label, 255, NAME_LABEL, res);
    elm_box_align_set(box, EVAS_HINT_FILL, 0.3);
  } else {
    snprintf(label, 255, NAME_LABEL, account);
    elm_box_align_set(box, EVAS_HINT_FILL, 0.4);
  }

  elm_object_text_set(name_label, label);
  elm_label_slide_go(name_label);
  evas_object_show(name_label);
  elm_box_pack_end(box, name_label);

  /* Code label */
  cvd->code_label = elm_label_add(box);
  evas_object_size_hint_align_set(cvd->code_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(cvd->code_label, 0, 0);

  refresh_code(cvd);
  evas_object_show(cvd->code_label);
  elm_box_pack_end(box, cvd->code_label);

  if (cvd->entry->type == TOTP) {
    /* Progress */
    cvd->progressbar = eext_circle_object_progressbar_add(cvd->layout, ad->circle_surface);
    eext_circle_object_value_min_max_set(cvd->progressbar, 0, TOTP_STEP_SIZE);
    eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

    evas_object_show(cvd->progressbar);

    /* Schedule update */
    cvd->timer = ecore_timer_add(1.0f, refresh_view_totp_cb, cvd);
  } else {
    Evas_Object *button = elm_button_add(cvd->layout);
    elm_object_text_set(button, "renew");
    elm_object_style_set(button, "bottom");
    evas_object_smart_callback_add(button, "clicked", renew_button_cb, cvd);
    evas_object_show(button);
    elm_object_part_content_set(cvd->layout, "elm.swallow.button", button);
  }

  Elm_Object_Item *nf_it = elm_naviframe_item_push(ad->nf, NULL, NULL, NULL, cvd->layout, "empty");
  elm_naviframe_item_pop_cb_set(nf_it, code_view_pop_cb, ad);
}

void code_view_resume(code_view_data_s *cvd) {
  if (cvd == NULL) return;

  if (cvd->entry->type == TOTP) {
    refresh_code(cvd);
    eext_circle_object_value_set(cvd->progressbar, cvd->seconds);
  }
}
