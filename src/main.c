#include <json-glib.h>
#include <system_info.h>
#include "main.h"
#include "util/otp.h"
#include "util/sqlite.h"

static const char *NAME_LABEL = "<font font_weight=Bold font_size=40><align=center> %s </align></font>";
static const char *CODE_LABEL = "<font font_weight=Regular font_size=75>%06d</font>";

static appdata_s *state;

static int
update_code(appdata_s *ad) {
  char code[255], label[255];
  int expires;

  if (ad->entries != NULL && ad->entries->data != NULL) {
    otp_info_s *entry = (otp_info_s *) (ad->entries->data);

    snprintf(code, 255, CODE_LABEL, totp_get_code(entry->secret, 0, &expires));
    elm_object_text_set(ad->code_label, code);

    char text[255] = {'\0'};
    strncpy(text, entry->user, 254);
    char* p = strchr(text, ':');
    if (p != NULL) {
      *(p++) = '\0';
      char res[255];
      snprintf(res, 255, "%s <br/> %s", text, p);
      snprintf(label, 255, NAME_LABEL, res);
    } else {
      snprintf(label, 255, NAME_LABEL, text);
    }

    if (strcmp(elm_object_text_get(ad->name_label), label) != 0) {
      elm_object_text_set(ad->name_label, label);
      elm_label_slide_go(ad->name_label);
    }

    return expires;
  }

  return 0;
}

static Eina_Bool _timer_start_cb(void *data) {
  appdata_s *ad = (appdata_s *) data;
  if (--ad->seconds < 1) {
    ad->seconds = update_code(ad);
  }

  eext_circle_object_value_set(ad->progressbar, ad->seconds);

  return ECORE_CALLBACK_RENEW;
}

void reload_database() {
  if (state->entries != NULL) {
    g_list_free_full(state->entries, free);
    state->entries = NULL;
  }
  db_select_all(&state->entries);
  state->seconds = update_code(state);
  eext_circle_object_value_set(state->progressbar, state->seconds);
  dlog_print(DLOG_DEBUG, LOG_TAG, "reload_database() reloaded");
}

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
    reload_database();
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

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
  ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
  appdata_s *ad = data;
  /* Let window go to hide state. */
  elm_win_lower(ad->win);
}

static void
create_base_gui()
{
  /* Window */
  state->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
  elm_win_autodel_set(state->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get(state->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set(state->win, (const int *)(&rots), 4);
  }

  evas_object_smart_callback_add(state->win, "delete,request", win_delete_request_cb, NULL);
  eext_object_event_callback_add(state->win, EEXT_CALLBACK_BACK, win_back_cb, state);

  /* Conformant */
  state->conform = elm_conformant_add(state->win);
  elm_win_indicator_mode_set(state->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set(state->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set(state->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_win_resize_object_add(state->win, state->conform);
  evas_object_show(state->conform);

  /* Box */
  state->box = elm_box_add(state->conform);
  elm_object_content_set(state->conform, state->box);
  elm_box_align_set(state->box, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(state->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show(state->box);

  /* Name label */
  int ww;
  if(system_info_get_platform_int("tizen.org/feature/screen.width", &ww) == SYSTEM_INFO_ERROR_NONE) {
    ww = 350;
  }

  state->name_label = elm_label_add(state->box);
  elm_label_wrap_width_set(state->name_label, ww);
  evas_object_size_hint_align_set(state->name_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(state->name_label, EVAS_HINT_EXPAND, 0);

  elm_object_style_set(state->name_label, "slide_bounce");
  elm_label_slide_mode_set(state->name_label, ELM_LABEL_SLIDE_MODE_AUTO);

  elm_box_pack_end(state->box, state->name_label);

  /* Code label */
  state->code_label = elm_label_add(state->box);
  evas_object_size_hint_align_set(state->code_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(state->code_label, EVAS_HINT_EXPAND, 0);

  elm_box_pack_end(state->box, state->code_label);

  /* Progress */
  state->c_surface = eext_circle_surface_conformant_add(state->conform);

  state->progressbar = eext_circle_object_progressbar_add(state->win, state->c_surface);
  eext_circle_object_value_min_max_set(state->progressbar, 0, TOTP_STEP_SIZE);

  evas_object_show(state->progressbar);

  /* Show window after base gui is set up */
  evas_object_show(state->win);
}

static bool
app_create(void *data)
{
  /* Hook to take necessary actions before main event loop starts
    Initialize UI resources and application's data
    If this function returns true, the main loop of application starts
    If this function returns false, the application is terminated */
  state = data;

  create_base_gui();

  reload_database();
  eext_circle_object_value_set(state->progressbar, state->seconds);

  evas_object_show(state->name_label);
  evas_object_show(state->code_label);

  /* Schedule update */
  state->timer = ecore_timer_add(1.0f, _timer_start_cb, state);

  initialize_sap();

  return true;
}

static void
app_control(app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

static void
app_pause(void *data)
{
  /* Take necessary actions when application becomes invisible. */
}

static void
app_resume(void *data)
{
  /* Take necessary actions when application becomes visible. */
}

static void
app_terminate(void *data)
{
  /* Release all resources. */
}

static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LANGUAGE_CHANGED*/
  char *locale = NULL;
  system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
  elm_language_set(locale);
  free(locale);
  return;
}

static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
  return;
}

static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_BATTERY*/
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_MEMORY*/
}

int
main(int argc, char *argv[])
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
