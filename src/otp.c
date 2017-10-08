#include <app.h>
#include <system_settings.h>
#include <dlog.h>
#include <json-glib.h>
#include "otp.h"
#include "database.h"
#include "sap.h"

void add_entry(char *data) {
  JsonParser *parser = json_parser_new();
  GError *error = NULL;

  json_parser_load_from_data(parser, data, strlen(data), &error);
  if (error != NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() json parser failed: %s", error);
    g_error_free(error);
    goto end;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (!(root != NULL && JSON_NODE_TYPE(root) == JSON_NODE_OBJECT)) {
    dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() got wrong json");
    goto end;
  }

  JsonObject *object;
  object = json_node_get_object(root);
  if (object == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() empty json object");
    goto end;
  }

  guint size = json_object_get_size(object);

  GList *keys = json_object_get_members(object);
  GList *values = json_object_get_values(object);
  GList *keys_c = keys;
  GList *values_c = values;

  otp_info_s result = {0};

  for (int i = 0; i < size; i++) {
    if (keys_c) {
      gchar *key = (gchar*)(keys_c->data);

      if (values_c) {
        JsonNode *value = (JsonNode*)(values_c->data);

        if (!(value != NULL && JSON_NODE_TYPE(value) == JSON_NODE_VALUE)) {
          dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
          goto free;
        }

        if (strcmp(key, "type") == 0) {
          const gchar *type = json_node_get_string(value);
          if (type == NULL) {
            dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
            goto free;
          }
          if (strcmp(type, "TOTP") == 0) result.type = TOTP;
          else if (strcmp(type, "HOTP") == 0) result.type = HOTP;
        } else if (strcmp(key, "counter") == 0) {
          result.counter = json_node_get_int(value);
        } else if (strcmp(key, "user") == 0) {
          const gchar *user = json_node_get_string(value);
          if (user == NULL) {
            dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
            goto free;
          }
          strncpy(result.user, user, 254);
        } else if (strcmp(key, "secret") == 0) {
          const gchar *secret = json_node_get_string(value);
          if (secret == NULL) {
            dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
            goto free;
          }
          strncpy(result.secret, secret, 254);
        }
      } else {
        dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
        goto free;
      }
    } else {
      dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
      goto free;
    }

    keys_c = g_list_next(keys_c);
    values_c = g_list_next(values_c);
  }

  if (result.user[0] != '\0' && result.secret[0] != '\0') {
    dlog_print(DLOG_DEBUG, LOG_TAG, "add_entry() adding new entry to database");
    db_insert(&result);
  } else {
    dlog_print(DLOG_ERROR, LOG_TAG, "add_entry() wrong json object");
    goto free;
  }

free:
  if (keys   != NULL) g_list_free(keys);
  if (values != NULL) g_list_free(values);
end:
  g_object_unref(parser);

  return;
}

static void win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
  ui_app_exit();
}

static void base_ui_create(appdata_s *ad)
{
  /*
   * Widget Tree
   * Window
   *  - conform
   *   - layout main
   *    - naviframe */

  /* Window */
  ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
  elm_win_conformant_set(ad->win, EINA_TRUE);
  elm_win_autodel_set(ad->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get(ad->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
  }

  evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);

  /* Conformant */
  ad->conform = elm_conformant_add(ad->win);
  evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_win_resize_object_add(ad->win, ad->conform);
  evas_object_show(ad->conform);

  // Eext Circle Surface Creation
  ad->circle_surface = eext_circle_surface_conformant_add(ad->conform);

  /* Base Layout */
  ad->layout = elm_layout_add(ad->conform);
  evas_object_size_hint_weight_set(ad->layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_layout_theme_set(ad->layout, "layout", "application", "default");
  evas_object_show(ad->layout);

  elm_object_content_set(ad->conform, ad->layout);

  /* Naviframe */
  ad->nf = elm_naviframe_add(ad->layout);
  elm_object_part_content_set(ad->layout, "elm.swallow.content", ad->nf);
  eext_object_event_callback_add(ad->nf, EEXT_CALLBACK_BACK, eext_naviframe_back_cb, NULL);
  eext_object_event_callback_add(ad->nf, EEXT_CALLBACK_MORE, eext_naviframe_more_cb, NULL);
}

static bool app_create(void *data)
{
  /* Hook to take necessary actions before main event loop starts
    Initialize UI resources and application's data
    If this function returns true, the main loop of application starts
    If this function returns false, the application is terminated */
  appdata_s *ad = data;

  initialize_sap();

  base_ui_create(ad);
  menu_create(ad);

  /* Show window after base gui is set up */
  evas_object_show(ad->win);

  return true;
}

static void app_control(app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

static void app_pause(void *data)
{
  /* Take necessary actions when application becomes invisible. */
}

static void app_resume(void *data)
{
  appdata_s *ad = (appdata_s *) data;
  code_view_resume(ad->current_cvd);
}

static void app_terminate(void *data)
{
  /* Release all resources. */
}

static void ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LANGUAGE_CHANGED*/
  char *locale = NULL;
  system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
  elm_language_set(locale);
  free(locale);
  return;
}

static void ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
  return;
}

static void ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_BATTERY*/
}

static void ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_MEMORY*/
}

int main(int argc, char *argv[])
{
  appdata_s ad = {0};
  int ret = 0;

  ui_app_lifecycle_callback_s event_callback = {0};
  app_event_handler_h handlers[5] = {NULL};

  db_init();

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
  ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
  ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
  ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
  ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);
  ui_app_remove_event_handler(handlers[APP_EVENT_LOW_MEMORY]);

  ret = ui_app_main(argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
