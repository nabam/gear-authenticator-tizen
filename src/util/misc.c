#include <stdbool.h>
#include "otp.h"

int get_otp_issuer(char* item, char *res) {
  snprintf(res, 255, "%s", item);
  char* p = strchr(res, ':');

  if (p != NULL) {
    *p = '\0';
    return true;
  }

  return false;
}

void get_otp_account(char* item, char *res) {
  char* p = strchr(item, ':');
  if (p == NULL) {
    p = item;
  } else {
    ++p;
  }

  snprintf(res, 255, "%s", p);
}


