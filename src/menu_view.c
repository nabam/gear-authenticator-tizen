#include <app.h>
#include <dlog.h>
#include "otp.h"
#include "database.h"

static char * menu_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
  if (data == NULL) return NULL;

  char account[255];
  char issuer[255];
  otp_info_s *entry = (otp_info_s *) data;

  get_otp_account(entry->label, account);

  if (get_otp_issuer(entry->label, issuer)) {
    if (!strcmp(part, "elm.text.1")) {
      return strdup(account);
    } else {
      return strdup(issuer);
    }
  } else {
    return strdup(account);
  }
}

static Eina_Bool menu_pop_cb(void *data, Elm_Object_Item *it)
{
  ui_app_exit();
  return EINA_FALSE;
}

static void menu_del_cb(void *data, Evas_Object *obj)
{
	otp_info_s *payload = (otp_info_s *)data;
	if (payload) free(payload);
}

static void menu_sel_cb(void *data, Evas_Object *obj, void *event_info)
{
	Elm_Object_Item *it = (Elm_Object_Item *)event_info;
	elm_genlist_item_selected_set(it, EINA_FALSE);

  code_view_create(data, elm_object_item_data_get(it));

	return;
}

static void menu_longpressed_cb(void *data, Evas_Object *obj, void *event_info)
{
	Elm_Object_Item *it = (Elm_Object_Item *)event_info;
	elm_genlist_item_selected_set(it, EINA_FALSE);


	return;
}

void menu_items_create(appdata_s *ad) {
  GList *entries = NULL, *entry = NULL;
  Evas_Object *popup = NULL, *layout = NULL;
  Elm_Genlist_Item_Class *ptc = elm_genlist_item_class_new();
  Elm_Genlist_Item_Class *style_1text = elm_genlist_item_class_new();
  Elm_Genlist_Item_Class *style_2text = elm_genlist_item_class_new();

  ptc->item_style = "padding";

  style_1text->item_style = "1text";
  style_1text->func.text_get = menu_text_get_cb;
	style_1text->func.del = menu_del_cb;

  style_2text->item_style = "2text";
  style_2text->func.text_get = menu_text_get_cb;
	style_2text->func.del = menu_del_cb;

  db_select_all(&entries);

  if (entries == NULL) {
    popup = elm_popup_add(ad->nf);
    elm_object_style_set(popup, "circle");
    evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    layout = elm_layout_add(popup);
    elm_layout_theme_set(layout, "layout", "popup", "content/circle");

    elm_object_part_text_set(layout, "elm.text", "Add OTP entries with a companion app on your phone");
    elm_object_content_set(popup, layout);

    evas_object_show(popup);

    goto free;
  }

  elm_genlist_item_append(ad->menu, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

  entry = entries;
  while (entry != NULL) {
    if (entry->data != NULL) {
      otp_info_s *payload = malloc(sizeof(otp_info_s));
      memcpy(payload, entry->data, sizeof(otp_info_s));
      dlog_print(DLOG_ERROR, LOG_TAG, ((otp_info_s*) entry->data)->label);

      Elm_Genlist_Item_Class *class;
      if (strchr(payload->label, ':')) {
        class = style_2text;
      } else {
        class = style_1text;
      }

      elm_genlist_item_append(
          ad->menu, // genlist object
          class,    // item class
          payload,  // data
          NULL,
          ELM_GENLIST_ITEM_NONE,
          menu_sel_cb,
          ad);
    }
    entry = g_list_next(entry);
  }

  elm_genlist_item_append(ad->menu, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

free:
  elm_genlist_item_class_free(style_1text);
  elm_genlist_item_class_free(style_2text);
  g_list_free_full(entries, free);
}

void menu_create(appdata_s *ad) {
  Evas_Object *circle_genlist = NULL, *btn = NULL;
  Elm_Object_Item *nf_it = NULL;

  ad->menu = elm_genlist_add(ad->nf);
  elm_genlist_mode_set(ad->menu, ELM_LIST_COMPRESS);
  elm_object_style_set(ad->menu, "focus_bg");
	evas_object_smart_callback_add(ad->menu, "longpressed", menu_longpressed_cb, NULL);

  circle_genlist = eext_circle_object_genlist_add(ad->menu, ad->circle_surface);
  eext_circle_object_genlist_scroller_policy_set(circle_genlist, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
  eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);

   /* This button is set for devices which doesn't have H/W back key. */
  btn = elm_button_add(ad->nf);
  elm_object_style_set(btn, "naviframe/end_btn/default");

  menu_items_create(ad);

  nf_it = elm_naviframe_item_push(ad->nf, NULL, btn, NULL, ad->menu, "empty");
  elm_naviframe_item_pop_cb_set(nf_it, menu_pop_cb, ad->win);
}


