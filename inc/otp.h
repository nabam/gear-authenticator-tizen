#ifndef __OTP_H__
#define __OTP_H__

#include <glib.h>
#include <Elementary.h>
#include <efl_extension.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "otp"

#if !defined(PACKAGE)
#define PACKAGE "net.nabam.wearable.otp"
#endif

typedef enum otp_type {
  TOTP, HOTP
} otp_type_e;

typedef struct otp_info {
  otp_type_e type;
  char label[255];
  char alias[255];
  char secret[255];
  int  counter;
  int  id;
} otp_info_s;

typedef struct code_view_data {
  int                 seconds;
  Evas_Object         *code_label;
  Evas_Object         *progressbar;
  Ecore_Timer         *timer;
  otp_info_s          *entry;
} code_view_data_s;

typedef struct appdata {
  Evas_Object         *win;
  Evas_Object         *nf;
  Eext_Circle_Surface *circle_surface;
  code_view_data_s    *current_cvd;
  Evas_Object         *menu;
} appdata_s;

void get_otp_account(char* item, char *res);
int get_otp_issuer(char* item, char *res);
void add_entry(char *);
void code_view_create(appdata_s *ad, otp_info_s *entry);
void code_view_resume(code_view_data_s *cvd);
void menu_create(appdata_s *ad);
void menu_items_create(appdata_s *ad);

#endif /* __OTP_H__ */
