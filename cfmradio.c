#include <libosso.h>
#include <hildon/hildon.h>

#include "radio.h"
#include "presets.h"
#include "preset_list.h"
#include "tuner.h"
#include "types.h"

static osso_context_t *osso_context;
static HildonProgram *program;
static HildonWindow *main_window;

static CFmRadio *radio;
static CFmPresets *presets;

static GtkLabel *freq_label;
static GtkLabel *ps_label, *rt_label;
static CFmTuner *tuner;
//static GtkToolbar *toolbar;

static CFmPresetList *preset_list;

static guint rds_timer;

static void print_freq(gulong freq)
{
	static gchar markup[256];
	float freq_mhz;

	freq_mhz = freq / 1000000.0f;

	g_snprintf(markup, sizeof(markup), "<span font=\"64\">%.1f</span> MHz", freq_mhz);
	gtk_label_set_markup(freq_label, markup);
}

static void print_rds()
{
	gchar *rds_ps, *rds_rt;
	gchar *markup;

	g_object_get(G_OBJECT(radio), "rds-ps", &rds_ps, "rds-rt", &rds_rt, NULL);

	markup = g_markup_printf_escaped("<span font=\"31\">%s</span>",
		g_strstrip(rds_ps));
	gtk_label_set_markup(ps_label, markup);
	g_free(markup);


	gtk_label_set_text(rt_label, g_strstrip(rds_rt));

	g_free(rds_ps);
	g_free(rds_rt);
}

static void range_low_changed_cb(GObject *object, GParamSpec *psec, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(radio), "range-low", &freq, NULL);
	g_object_set(G_OBJECT(tuner), "range-low", freq, NULL);
}

static void range_high_changed_cb(GObject *object, GParamSpec *psec, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(radio), "range-high", &freq, NULL);
	g_object_set(G_OBJECT(tuner), "range-high", freq, NULL);
}

static void frequency_changed_cb(GObject *object, GParamSpec *psec, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(radio), "frequency", &freq, NULL);
	g_object_set(G_OBJECT(tuner), "frequency", freq, NULL);
	print_freq(freq);
}

static gboolean key_press_cb(GObject *object, GdkEventKey *event, gpointer user_data)
{
	gulong freq;
	switch (event->keyval) {
	case GDK_Left:
		g_object_get(G_OBJECT(radio), "frequency", &freq, NULL);
		freq -= 100000;
		g_object_set(G_OBJECT(radio), "frequency", freq, NULL);
		return FALSE;
	break;
	case GDK_Right:
		g_object_get(G_OBJECT(radio), "frequency", &freq, NULL);
		freq += 100000;
		g_object_set(G_OBJECT(radio), "frequency", freq, NULL);
		return FALSE;
	break;
	}
	return TRUE;
}

static gboolean tuner_changed_cb(GObject *object, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(tuner), "frequency", &freq, NULL);
	print_freq(freq);
	return TRUE;
}

static gboolean tuner_tuned_cb(GObject *object, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(tuner), "frequency", &freq, NULL);
	g_object_set(G_OBJECT(radio), "frequency", freq, NULL);
	print_freq(freq);
	return TRUE;
}

static gboolean rds_timer_cb(gpointer data)
{
	print_rds();
	return TRUE;
}

static void presets_clicked(GtkButton *button, gpointer user_data)
{
	cfm_preset_list_show_for(preset_list, presets);
}

static void preset_frequency_cb(GObject *object, GParamSpec *psec, gpointer user_data)
{
	gulong freq;
	g_object_get(G_OBJECT(preset_list), "frequency", &freq, NULL);
	g_object_set(G_OBJECT(radio), "frequency", freq, NULL);
	gtk_widget_hide(GTK_WIDGET(preset_list));
	print_freq(freq);
}

static void add_preset_clicked(GtkButton *button, gpointer user_data)
{
	gchar *rds_ps = NULL;
	gulong freq;

	g_object_get(G_OBJECT(radio), "rds-ps", &rds_ps, "frequency", &freq, NULL);
	cfm_presets_set_preset(presets, freq, g_strstrip(rds_ps));

	g_free(rds_ps);
}

static void build_main_window()
{
	GtkBox *box;

	main_window = HILDON_WINDOW(hildon_stackable_window_new());
	gtk_window_set_title(GTK_WINDOW(main_window), "Radio");
	hildon_gtk_window_set_portrait_flags(GTK_WINDOW(main_window),
		HILDON_PORTRAIT_MODE_SUPPORT);

	HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());

	GtkWidget *menu_button;
	menu_button = gtk_button_new_with_label("Presets...");
	g_signal_connect_after(G_OBJECT(menu_button), "clicked",
		G_CALLBACK(presets_clicked), NULL);
	hildon_app_menu_append(menu, GTK_BUTTON(menu_button));
	menu_button = gtk_button_new_with_label("Add preset");
	g_signal_connect_after(G_OBJECT(menu_button), "clicked",
		G_CALLBACK(add_preset_clicked), NULL);
	hildon_app_menu_append(menu, GTK_BUTTON(menu_button));
	gtk_widget_show_all(GTK_WIDGET(menu));

	box = GTK_BOX(gtk_vbox_new(FALSE, 0));

	freq_label = GTK_LABEL(gtk_label_new(NULL));
	ps_label = GTK_LABEL(gtk_label_new(NULL));
	rt_label = GTK_LABEL(gtk_label_new(NULL));
	tuner = cfm_tuner_new();

#if 0
	toolbar = GTK_TOOLBAR(gtk_toolbar_new());

	gtk_toolbar_set_icon_size(toolbar, HILDON_ICON_SIZE_THUMB);

	GtkToolItem* item = gtk_tool_button_new(
		gtk_image_new_from_icon_name("general_back", HILDON_ICON_SIZE_THUMB),
		NULL);
	gtk_toolbar_insert(toolbar, item, -1);
	item = gtk_tool_item_new();
	gtk_tool_item_set_expand(item, TRUE);
	gtk_toolbar_insert(toolbar, item, -1);
	item = gtk_tool_button_new(gtk_image_new_from_icon_name(
		"general_add", HILDON_ICON_SIZE_THUMB), NULL);
	gtk_toolbar_insert(toolbar, item, -1);
	item = gtk_tool_button_new(gtk_image_new_from_icon_name(
		"general_mybookmarks_folder", HILDON_ICON_SIZE_THUMB), NULL);
	gtk_toolbar_insert(toolbar, item, -1);
	item = gtk_tool_item_new();
	gtk_tool_item_set_expand(item, TRUE);
	gtk_toolbar_insert(toolbar, item, -1);
	item = gtk_tool_button_new(gtk_image_new_from_icon_name(
		"general_forward", HILDON_ICON_SIZE_THUMB), NULL);
	gtk_toolbar_insert(toolbar, item, -1);
#endif

	gtk_box_pack_start(box, GTK_WIDGET(freq_label), TRUE, TRUE, 0);
	gtk_box_pack_start(box, GTK_WIDGET(ps_label), FALSE, FALSE, 0);
	gtk_box_pack_start(box, GTK_WIDGET(rt_label), FALSE, FALSE, 0);
	gtk_box_pack_start(box, GTK_WIDGET(tuner), FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(box));

	hildon_window_set_app_menu(main_window, menu);

	//hildon_window_add_toolbar(main_window, toolbar);
	hildon_program_add_window(program, main_window);

	g_signal_connect(G_OBJECT(main_window), "delete-event",
	                 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(main_window), "key-press-event",
	                 G_CALLBACK(key_press_cb), NULL);
	g_signal_connect(G_OBJECT(tuner), "frequency-changed",
	                 G_CALLBACK(tuner_changed_cb), NULL);
	g_signal_connect(G_OBJECT(tuner), "frequency-tuned",
	                 G_CALLBACK(tuner_tuned_cb), NULL);
}

int main(int argc, char *argv[])
{
	hildon_gtk_init(&argc, &argv);

	osso_context = osso_initialize("com.javispedro.cfmradio", "0.1", TRUE, NULL);
	g_warn_if_fail(osso_context != NULL);

	g_set_application_name("FM Radio"); /* This might be important for Pulse */
	program = hildon_program_get_instance();

	radio = cfm_radio_new();
	g_signal_connect(G_OBJECT(radio), "notify::range-low",
	                 G_CALLBACK(range_low_changed_cb), NULL);
	g_signal_connect(G_OBJECT(radio), "notify::range-high",
	                 G_CALLBACK(range_high_changed_cb), NULL);
	g_signal_connect(G_OBJECT(radio), "notify::frequency",
	                 G_CALLBACK(frequency_changed_cb), NULL);

	presets = cfm_presets_get_default();

	rds_timer = g_timeout_add_seconds(1, rds_timer_cb, NULL);

	build_main_window();

	gtk_widget_show_all(GTK_WIDGET(main_window));

	preset_list = cfm_preset_list_new();
	g_signal_connect(G_OBJECT(preset_list), "delete-event", 
	                 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(preset_list), "notify::frequency", 
	                 G_CALLBACK(preset_frequency_cb), NULL);

	g_object_set(G_OBJECT(radio), "output", CFM_RADIO_OUTPUT_SYSTEM, NULL);

	gtk_main();

	g_object_unref(G_OBJECT(radio));
	osso_deinitialize(osso_context);

	return 0;
}

