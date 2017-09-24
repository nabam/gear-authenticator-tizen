#ifndef __OTP_H__
#define __OTP_H__

#define TOTP_STEP_SIZE 30

int totp_get_code(const char *secret, int skew, int *expires);
int hotp_get_code(const char *secret, int counter);

#endif /* _OTP_H_ */
