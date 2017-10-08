#ifndef __OTP_DATABASE_H__
#define __OTP_DATABASE_H__

#include "otp.h"
#include <sqlite3.h>

int db_init();
int db_insert(otp_info_s*);
int db_select_all(GList**);
int db_select_id(GList**, int);
int db_delete_id(int);
int db_inc_counter(int);

#endif /* __OTP_DATABASE_H__ */
