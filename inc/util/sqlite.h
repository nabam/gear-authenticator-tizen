#ifndef __UTIL_SQLITE_H__
#define __UTIL_SQLITE_H__

#define DB_NAME        "otp.db"
#define DB_TABLE_NAME  "entries"
#define DB_COL_ID      "ID"
#define DB_COL_TYPE    "TYPE"
#define DB_COL_USER    "USER"
#define DB_COL_COUNTER "COUNTER"
#define DB_COL_SECRET  "SECRET"

#include "main.h"

int db_init();
int db_insert(otp_info_s *data);
int db_select_all(GList** result);
int db_delete_id(int id);

#endif /* __UTIL_SQLITE_H__ */
