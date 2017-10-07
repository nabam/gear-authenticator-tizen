#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <app_common.h>
#include <dlog.h>
#include "database.h"
#include "otp.h"

#define DB_NAME        "otp.db"
#define DB_TABLE_NAME  "entries"
#define DB_COL_ID      "ID"
#define DB_COL_TYPE    "TYPE"
#define DB_COL_USER    "USER"
#define DB_COL_COUNTER "COUNTER"
#define DB_COL_SECRET  "SECRET"
#define DB_LOG_TAG     "SQLITE:"


static int _db_open(sqlite3 **otp_db)
{
  char *data_path = app_get_data_path();
  int size = strlen(data_path) + strlen(DB_NAME) + 1;

  char *path = malloc(sizeof(char) * size);

  strcpy(path, data_path);
  strncat(path, DB_NAME, size);

  int ret = sqlite3_open_v2( path , otp_db, SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE, NULL);
  if(ret != SQLITE_OK)
    dlog_print(DLOG_ERROR, LOG_TAG, DB_LOG_TAG" can't open database: %s", sqlite3_errmsg(*otp_db));

  free(data_path);
  free(path);
  return ret;
}

int db_init()
{
  sqlite3 *otp_db;

  if(_db_open(&otp_db) != SQLITE_OK)
    return SQLITE_ERROR;

  int ret;
  char *err_msg;
  char *sql = "CREATE TABLE IF NOT EXISTS "DB_TABLE_NAME" \
               ("DB_COL_TYPE"    INTEGER NOT NULL, \
                "DB_COL_USER"    TEXT    NOT NULL, \
                "DB_COL_COUNTER" INTEGET NOT NULL, \
                "DB_COL_SECRET"  TEXT    NOT NULL, \
                "DB_COL_ID"      INTEGER PRIMARY KEY AUTOINCREMENT);";

  ret = sqlite3_exec(otp_db, sql, NULL, 0, &err_msg);
  if(ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, DB_LOG_TAG" create table query failed: %s", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }
  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int _insert_cb(void *unused, int count, char **data, char **columns){
  // TODO: refresh callback
  return 0;
}

int db_insert(otp_info_s *data)
{
  sqlite3 *otp_db;

  if(_db_open(&otp_db) != SQLITE_OK)
    return SQLITE_ERROR;

  char *err_msg, *sql;
  int ret;

  sql = sqlite3_mprintf(
      "INSERT INTO "DB_TABLE_NAME" VALUES(%d, %Q, %d, %Q, NULL);",
      data->type, data->user, data->counter, data->secret);

  ret = sqlite3_exec(otp_db, sql, _insert_cb, 0, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, DB_LOG_TAG" insert query failed: %s", sqlite3_errmsg(otp_db));
    sqlite3_free(err_msg);
    sqlite3_free(sql);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  sqlite3_free(sql);
  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int _select_cb(void *list, int count, char **data, char **columns){
  GList **head = (GList **) list;
  otp_info_s *temp = (otp_info_s*) malloc(sizeof(otp_info_s));
  memset(temp, 0, sizeof(otp_info_s));

  if (temp == NULL){
    dlog_print(DLOG_ERROR, LOG_TAG, DB_LOG_TAG" can't allocate memory for otp_info_s");
    return SQLITE_ERROR;
  } else {
            temp->type    = atoi(data[0]);
    strncpy(temp->user,          data[1], 254);
           temp->counter = atoi(data[2]);
    strncpy(temp->secret,        data[3], 254);
            temp->id      = atoi(data[4]);
  }

  *head = g_list_append(*head, temp);
  return SQLITE_OK;
}

int db_select_all(GList** result)
{
  sqlite3 *otp_db;

  if(_db_open(&otp_db) != SQLITE_OK)
    return SQLITE_ERROR;

  char *sql = "SELECT * FROM "DB_TABLE_NAME" ORDER BY ID DESC";
  int ret;
  char *err_msg;

  ret = sqlite3_exec(otp_db, sql, _select_cb, (void *) result, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_DEBUG, LOG_TAG, DB_LOG_TAG" select query failed: %s", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int _delete_cb(void *list, int count, char **data, char **columns){
  // TODO: refresh interface callback
  return 0;
}

int db_delete_id(int id)
{
  sqlite3 *otp_db;

  if(_db_open(&otp_db) != SQLITE_OK)
    return SQLITE_ERROR;

  char *sql = sqlite3_mprintf("DELETE from "DB_TABLE_NAME" where "DB_COL_ID"=%d;", id);

  int counter = 0, ret = 0;
  char *err_msg;

  ret = sqlite3_exec(otp_db, sql, _delete_cb, &counter, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, DB_LOG_TAG" delete query failed: %s", err_msg);
    sqlite3_free(sql);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  sqlite3_free(sql);
  sqlite3_close(otp_db);

  return SQLITE_OK;
}
