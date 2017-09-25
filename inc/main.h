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
  char user[255];
  char secret[255];
  int  counter;
  int  id;
} otp_info_s;

typedef struct appdata {
  Evas_Object         *win;
  Evas_Object         *conform;
  Evas_Object         *layout;
  Evas_Object         *nf;
  Eext_Circle_Surface *circle_surface;
} appdata_s;

typedef struct code_view_data {
  int                 seconds;
  Evas_Object         *name_label;
  Evas_Object         *code_label;
  Evas_Object         *box;
  Evas_Object         *progressbar;
  Ecore_Timer         *timer;
  otp_info_s          *entry;
} code_view_data_s;

void add_entry(char *);

#endif /* __OTP_H__ */
