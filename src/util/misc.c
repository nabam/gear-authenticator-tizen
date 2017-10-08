#include <stdbool.h>
#include "otp.h"

int get_otp_issuer(char* label, char *res) {
  snprintf(res, 255, "%s", label);
  char* p = strchr(res, ':');

  if (p != NULL) {
    *p = '\0';
    return true;
  }

  return false;
}

void get_otp_account(char* label, char *res) {
  char* p = strchr(label, ':');
  if (p == NULL) {
    p = label;
  } else {
    ++p;
  }

  snprintf(res, 255, "%s", p);
}


