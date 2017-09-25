#include <json-glib.h>
#include <system_info.h>
#include <app.h>
#include <system_settings.h>
#include <dlog.h>
#include "main.h"
#include "util/sap.h"
#include "util/otp.h"
#include "util/sqlite.h"

static const char *NAME_LABEL = "<font font_weight=Bold font_size=40><align=center> %s </align></font>";
static const char *CODE_LABEL = "<font font_weight=Regular font_size=75>%06d</font>";

static int get_otp_issuer(char* item, char *res) {
  snprintf(res, 255, item);
  char* p = strchr(res, ':');

  if (p != NULL) {
    *p = '\0';
    return true;
  }

  return false;
}

static void get_otp_account(char* item, char *res) {
  char* p = strchr(item, ':');
  if (p == NULL) p = item;

  snprintf(res, 255, ++p);
}

static void get_code(code_view_data_s *cvd) {
  int expires = 0;
  char code[255];
  otp_info_s *entry = (otp_info_s *) (cvd->entry);

  if (entry != NULL) {
    snprintf(code, 255, CODE_LABEL, totp_get_code(entry->secret, 0, &expires));
    elm_object_text_set(cvd->code_label, code);
  }

  cvd->seconds = expires;
}

static Eina_Bool _timer_start_cb(void *data) {
  code_view_data_s *cvd = data;

  if (--cvd->seconds < 1) {
    get_code(cvd);
  }

  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

  return ECORE_CALLBACK_RENEW;
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

/*static void win_back_cb(void *data, Evas_Object *obj, void *event_info)*/
/*{*/
  /*appdata_s *ad = (appdata_s *)data;*/
  /*[> Let window go to hide state. <]*/
  /*elm_win_lower(ad->win);*/
/*}*/

static void create_code_view_cb(void *data, Evas_Object *obj, void *event_info)
{
  if (!data) return;
  appdata_s *ad = (appdata_s *)data;
  code_view_data_s *cvd = calloc(1, sizeof(code_view_data_s));
  cvd->entry = elm_object_item_data_get((Elm_Object_Item *) event_info);

  /* Box */
  cvd->box = elm_box_add(ad->nf);
  elm_box_align_set(cvd->box, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(cvd->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show(cvd->box);

  /* Name label */
  cvd->name_label = elm_label_add(cvd->box);
  evas_object_size_hint_align_set(cvd->name_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(cvd->name_label, EVAS_HINT_EXPAND, 0);

  char label[255], issuer[255], account[255];
  get_otp_account(cvd->entry->user, account);
  if (get_otp_issuer(cvd->entry->user, issuer)) {
    char res[255];
    snprintf(res, 255, "%s <br/> %s", issuer, account);
    snprintf(label, 255, NAME_LABEL, res);
  } else {
    snprintf(label, 255, NAME_LABEL, account);
  }

  elm_object_text_set(cvd->name_label, label);
  evas_object_show(cvd->name_label);
  elm_box_pack_end(cvd->box, cvd->name_label);

  /* Code label */
  cvd->code_label = elm_label_add(cvd->box);
  evas_object_size_hint_align_set(cvd->code_label, 0.5, 0.5);
  evas_object_size_hint_weight_set(cvd->code_label, EVAS_HINT_EXPAND, 0);

  get_code(cvd);
  evas_object_show(cvd->code_label);
  elm_box_pack_end(cvd->box, cvd->code_label);

  /* Progress */
  cvd->progressbar = eext_circle_object_progressbar_add(ad->win, ad->circle_surface);
  eext_circle_object_value_min_max_set(cvd->progressbar, 0, TOTP_STEP_SIZE);
  eext_circle_object_value_set(cvd->progressbar, cvd->seconds);

  evas_object_show(cvd->progressbar);


  /* Schedule update */
  cvd->timer = ecore_timer_add(1.0f, _timer_start_cb, cvd);

  elm_naviframe_item_push(ad->nf, NULL, NULL, NULL, cvd->box, "empty");
}

static char * _gl_text_get(void *data, Evas_Object *obj, const char *part)
{
  if (data == NULL) return NULL;

  char account[255];
  char issuer[255];
  otp_info_s *entry = (otp_info_s *) data;

  get_otp_account(entry->user, account);
  get_otp_issuer(entry->user, issuer);

  if (issuer[0] != '\0') {
    if (!strcmp(part, "elm.text.1")) {
      return strdup(account);
    } else {
      return strdup(issuer);
    }
  } else {
    if (!strcmp(part, "elm.text.1")) {
      return NULL;
    } else {
      return strdup(account);
    }
  }
}

static Eina_Bool _naviframe_pop_cb(void *data, Elm_Object_Item *it)
{
  ui_app_exit();
  return EINA_FALSE;
}

static void create_main_menu(void *data) {
  appdata_s *ad = (appdata_s *)data;
  Evas_Object *genlist = NULL;
  Evas_Object *circle_genlist = NULL;

  /* Create item class */
  Elm_Genlist_Item_Class *ptc = elm_genlist_item_class_new();
  Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();

  itc->item_style = "2text";
  itc->func.text_get = _gl_text_get;

  ptc->item_style = "padding";

  genlist = elm_genlist_add(ad->nf);

  elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

  elm_object_style_set(genlist, "focus_bg");

  circle_genlist = eext_circle_object_genlist_add(genlist, ad->circle_surface);
  eext_circle_object_genlist_scroller_policy_set(circle_genlist, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
  eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);

  elm_genlist_item_append(genlist, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

  GList *entries = NULL;
  db_select_all(&entries);

  GList* entry = entries;
  while (entry != NULL) {
    if (entry->data != NULL) {
      dlog_print(DLOG_ERROR, LOG_TAG, ((otp_info_s*) entry->data)->user);
      elm_genlist_item_append(
          genlist,      // genlist object
          itc,          // item class
          entry->data,  // data
          NULL,
          ELM_GENLIST_ITEM_NONE,
          create_code_view_cb,
          ad);
    }
    entry = g_list_next(entry);
  }

  elm_genlist_item_append(genlist, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

  elm_genlist_item_class_free(itc);
  /*g_list_free_full(entries, free);*/

  /* This button is set for devices which doesn't have H/W back key. */
  Evas_Object *btn;
  btn = elm_button_add(ad->nf);
  elm_object_style_set(btn, "naviframe/end_btn/default");

  Elm_Object_Item *nf_it;
  nf_it = elm_naviframe_item_push(ad->nf, NULL, btn, NULL, genlist, "empty");
  elm_naviframe_item_pop_cb_set(nf_it, _naviframe_pop_cb, ad->win);
}

static void
create_base_gui(appdata_s *ad)
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

  /* Indicator */
  /* elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW); */

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

  create_base_gui(ad);
  create_main_menu(ad);

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
  /* Take necessary actions when application becomes visible. */
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
