#ifndef __otp_H__
#define __otp_H__

#include <app.h>
#include <Elementary.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>
#include <glib.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "otp"

#if !defined(PACKAGE)
#define PACKAGE "net.nabam.wearable.otp"
#endif

typedef struct appdata {
  Evas_Object         *win;
  int                 seconds;
  Evas_Object         *conform;
  Evas_Object         *name_label;
  Evas_Object         *code_label;
  Evas_Object         *box;
  Evas_Object         *progressbar;
  Ecore_Timer         *timer;
  GList               *entries;
  Eext_Circle_Surface *c_surface;
} appdata_s;

typedef enum otp_type {
  TOTP, HOTP
} otp_type_e;

typedef struct otp_info_item {
  otp_type_e type;
  char user[255];
  char secret[255];
  int  counter;
  int  id;
} otp_info_s;

void initialize_sap();
void add_entry(char *);

#endif /* __otp_H__ */
