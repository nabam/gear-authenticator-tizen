#include <string.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <storage.h>
#include <app_common.h>
#include <stdio.h>
#include "util/sqlite.h"
#include "main.h"

sqlite3 *otp_db;

static int db_open()
{
  char *data_path = app_get_data_path();
  int size = strlen(data_path) + 10;

  char *path = malloc(sizeof(char) * size);

  strcpy(path, data_path);
  strncat(path, DB_NAME, size);

  dlog_print(DLOG_DEBUG, LOG_TAG, "DB Path = [%s]", path);

  int ret = sqlite3_open_v2( path , &otp_db, SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE, NULL);
  if(ret != SQLITE_OK)
    dlog_print(DLOG_ERROR, LOG_TAG, "DB Create Error! [%s]", sqlite3_errmsg(otp_db));

  free(data_path);
  free(path);
  return ret;
}

int db_init()
{
  if (db_open() != SQLITE_OK)
    return SQLITE_ERROR;

  int ret;
  char *err_msg;
  char *sql = "CREATE TABLE IF NOT EXISTS "DB_TABLE_NAME" \
               ("DB_COL_TYPE"    INTEGER NOT NULL, \
                "DB_COL_USER"    TEXT    NOT NULL, \
                "DB_COL_COUNTER" INTEGET NOT NULL, \
                "DB_COL_SECRET"  TEXT    NOT NULL, \
                "DB_COL_ID"      INTEGER PRIMARY KEY AUTOINCREMENT);";

  dlog_print(DLOG_DEBUG, LOG_TAG, "Create table query : %s", sql);

  ret = sqlite3_exec(otp_db, sql, NULL, 0, &err_msg);
  if(ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, "Table Create Error! [%s]", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }
  dlog_print(DLOG_DEBUG, LOG_TAG, "DB Table created successfully");
  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int insert_cb(void *unused, int count, char **data, char **columns){
  // TODO: refresh callback
  return 0;
}

int db_insert(otp_info_s *data)
{
  if (db_open() != SQLITE_OK)
    return SQLITE_ERROR;

  char *err_msg, *sql;
  int ret;

  sql = sqlite3_mprintf(
      "INSERT INTO "DB_TABLE_NAME" VALUES(%d, %Q, %d, %Q, NULL);",
      data->type, data->user, data->counter, data->secret);

  ret = sqlite3_exec(otp_db, sql, insert_cb, 0, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, "Insertion Error! [%s]", sqlite3_errmsg(otp_db));
    sqlite3_free(err_msg);
    sqlite3_free(sql);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  sqlite3_free(sql);
  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int select_all_cb(void *list, int count, char **data, char **columns){
  Eina_List **head = (Eina_List **) list;
  otp_info_s *temp = (otp_info_s*) malloc(sizeof(otp_info_s));

  if (temp == NULL){
    dlog_print(DLOG_ERROR, LOG_TAG, "Cannot allocate memory for otp_info_s");
    return SQLITE_ERROR;
  } else {
           temp->type    = atoi(data[0]);
    strcpy(temp->user,          data[1]);
           temp->counter = atoi(data[2]);
    strcpy(temp->secret,        data[3]);
           temp->id      = atoi(data[4]);
  }

  *head = eina_list_append(*head, temp);
  return SQLITE_OK;
}

int db_select_all(Eina_List** result)
{
  if(db_open() != SQLITE_OK)
    return SQLITE_ERROR;

  char *sql = "SELECT * FROM "DB_TABLE_NAME" ORDER BY ID DESC";
  int ret;
  char *err_msg;

  ret = sqlite3_exec(otp_db, sql, select_all_cb, (void *) result, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_DEBUG, LOG_TAG, "Select query execution error [%s]", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  dlog_print(DLOG_DEBUG, LOG_TAG, "select query execution success!");
  sqlite3_close(otp_db);

  return SQLITE_OK;
}

static int delete_cb(void *list, int count, char **data, char **columns){
  // TODO: refresh interface callback
  return 0;
}

int db_delete_id(int id)
{
  if(db_open() != SQLITE_OK)
    return SQLITE_ERROR;

  char *sql = sqlite3_mprintf("DELETE from otp_info_s where QR_ID=%d;", id);

  int counter = 0, ret = 0;
  char *err_msg;

  ret = sqlite3_exec(otp_db, sql, delete_cb, &counter, &err_msg);
  if (ret != SQLITE_OK)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, "Delete Error! [%s]", err_msg);
    sqlite3_free(sql);
    sqlite3_free(err_msg);
    sqlite3_close(otp_db);

    return SQLITE_ERROR;
  }

  sqlite3_free(sql);
  sqlite3_close(otp_db);

  return SQLITE_OK;
}
