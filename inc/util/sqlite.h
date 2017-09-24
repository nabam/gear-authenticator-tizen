#ifndef __UTIL_SQLITE_H__
#define __UTIL_SQLITE_H__

#include "main.h"

int db_init();
int db_insert(otp_info_s *data);
int db_select_all(GList** result);
int db_delete_id(int id);

#endif /* __UTIL_SQLITE_H__ */
