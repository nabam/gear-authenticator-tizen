#include "main.h"
#include "otp/otp.h"

static const char *SECRET = "zzzzzzzzz";
static const char *NAME_LABEL = "<font font_weight=Bold font_size=41>leo@nabam.net</font>";
static const char *CODE_LABEL = "<font font_weight=Regular font_size=70>%06d</font>";

void add_uri(char *data) { }

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

static int
update_code(Evas_Object *code_label) {
  char code[255];
  int expires;
  snprintf(code, 255, CODE_LABEL, totp_get_code(SECRET, 0, &expires));
  elm_object_text_set(code_label, code);
  return expires;
}

static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	/* Conformant */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

  /* Box */
  ad->box = elm_box_add(ad->conform);
  elm_object_content_set(ad->conform, ad->box);
  elm_box_align_set(ad->box, EVAS_HINT_FILL, 0.5);
	evas_object_show(ad->box);

	/* Name label */
  ad->name_label = elm_label_add(ad->box);
  elm_label_wrap_width_set(ad->name_label, 270);
  /*elm_label_ellipsis_set(ad->name_label, EINA_TRUE);*/
  evas_object_size_hint_align_set(ad->name_label, 0.5,0.5);
	evas_object_size_hint_weight_set(ad->name_label, EVAS_HINT_EXPAND, 0);

  elm_box_pack_end(ad->box, ad->name_label);

  /* Code label */
  ad->code_label = elm_label_add(ad->box);
  evas_object_size_hint_align_set(ad->code_label, 0.5,0.5);
	evas_object_size_hint_weight_set(ad->code_label, EVAS_HINT_EXPAND, 0);

  elm_box_pack_end(ad->box, ad->code_label);

  /* Progress */
  ad->c_surface = eext_circle_surface_conformant_add(ad->conform);

  ad->progressbar = eext_circle_object_progressbar_add(ad->win, ad->c_surface);
  eext_circle_object_value_min_max_set(ad->progressbar, 0, TOTP_STEP_SIZE);

  evas_object_show(ad->progressbar);

	/* Show window after base gui is set up */
	evas_object_show(ad->win);
}

static Eina_Bool _timer_start_cb(void *data) {
  appdata_s *ad = (appdata_s *) data;
  if (--ad->seconds < 1) {
    ad->seconds = update_code(ad->code_label);
  }

  eext_circle_object_value_set(ad->progressbar, ad->seconds);

  return ECORE_CALLBACK_RENEW;
}

static bool
app_create(void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */
	appdata_s *ad = data;

	create_base_gui(ad);

  ad->seconds = update_code(ad->code_label);
  eext_circle_object_value_set(ad->progressbar, ad->seconds);

  elm_object_text_set(ad->name_label, NAME_LABEL);

  evas_object_show(ad->name_label);
  evas_object_show(ad->code_label);

  /* Schedule update */
  ad->timer = ecore_timer_add(1.0f, _timer_start_cb, ad);

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
	appdata_s ad = {0,};
	int ret = 0;

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

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
