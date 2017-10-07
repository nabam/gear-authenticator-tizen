#ifndef __OTP_DATABASE_H__
#define __OTP_DATABASE_H__

#include "otp.h"

int db_init();
int db_insert(otp_info_s *data);
int db_select_all(GList** result);
int db_delete_id(int id);

#endif /* __OTP_DATABASE_H__ */
