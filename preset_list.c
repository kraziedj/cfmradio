/*
 * GPL 2
 */

#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include "presets.h"
#include "preset_list.h"

G_DEFINE_TYPE(CFmPresetList, cfm_preset_list, HILDON_TYPE_STACKABLE_WINDOW);

#define CFM_PRESET_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CFM_TYPE_PRESET_LIST, CFmPresetListPrivate))

#define ABS_RANGE_LOW 60000000
#define ABS_RANGE_HIGH 140000000

struct _CFmPresetListPrivate {
	HildonTouchSelector *sel;
	HildonTouchSelectorColumn *col;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_FREQUENCY,
	PROP_LAST
};

enum {
	SIGNAL_0,
	SIGNAL_LAST
};

static GParamSpec *properties[PROP_LAST];
static guint signals[SIGNAL_LAST];

static void cfm_preset_list_selection_changed(HildonTouchSelector *selector,
	gint column, gpointer user_data)
{
	CFmPresetList *self = CFM_PRESET_LIST(user_data);

	g_object_notify(G_OBJECT(self), "frequency");
}

static void cfm_preset_list_set_property(GObject *object, guint property_id,
	const GValue *value, GParamSpec *pspec)
{
	CFmPresetList *self = CFM_PRESET_LIST(object);
	CFmPresetListPrivate *priv = self->priv;
	switch (property_id) {
	case PROP_MODEL:
		hildon_touch_selector_set_model(priv->sel, 0, g_value_get_object(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_preset_list_get_property(GObject *object, guint property_id,
	GValue *value, GParamSpec *pspec)
{
	CFmPresetList *self = CFM_PRESET_LIST(object);
	CFmPresetListPrivate *priv = self->priv;
	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object(value, hildon_touch_selector_get_model(priv->sel, 0));
		break;
	case PROP_FREQUENCY: {
		GtkTreeIter iter;
		if (hildon_touch_selector_get_selected(priv->sel, 0, &iter)) {
			gulong freq;
			gtk_tree_model_get(hildon_touch_selector_get_model(priv->sel, 0),
				&iter, 0, &freq, -1);
			g_value_set_ulong(value, freq);
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_preset_list_init(CFmPresetList *self)
{
	CFmPresetListPrivate *priv;

	self->priv = priv = CFM_PRESET_LIST_GET_PRIVATE(self);

	/* Empty model for now */
	GtkTreeModel *model = GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_FLOAT, G_TYPE_STRING));

	gtk_window_set_title(GTK_WINDOW(self), "Presets");
	hildon_gtk_window_set_portrait_flags(GTK_WINDOW(self),
		HILDON_PORTRAIT_MODE_SUPPORT);

	priv->sel = HILDON_TOUCH_SELECTOR(hildon_touch_selector_new());
	priv->col = hildon_touch_selector_append_text_column(priv->sel, model, FALSE);
	hildon_touch_selector_column_set_text_column(priv->col, 1);

	gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->sel));

	g_signal_connect(G_OBJECT(priv->sel), "changed",
                     G_CALLBACK(cfm_preset_list_selection_changed), self);

	g_object_unref(model);
}

static void cfm_preset_list_class_init(CFmPresetListClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GParamSpec *param_spec;

	gobject_class->set_property = cfm_preset_list_set_property;
	gobject_class->get_property = cfm_preset_list_get_property;

	g_type_class_add_private(klass, sizeof(CFmPresetListPrivate));

	param_spec = g_param_spec_object("model",
	                                 "GtkTreeModel to use",
	                                 "This is the model where contents will be read from",
	                                 GTK_TYPE_TREE_MODEL,
	                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_MODEL] = param_spec;
	g_object_class_install_property(gobject_class, PROP_MODEL, param_spec);

	param_spec = g_param_spec_ulong("frequency",
	                                "Frequency to tune (Hz)",
	                                "This is the frequency to tune, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_LOW,
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_FREQUENCY] = param_spec;
	g_object_class_install_property(gobject_class, PROP_FREQUENCY, param_spec);
}

CFmPresetList* cfm_preset_list_new()
{
	return g_object_new(CFM_TYPE_PRESET_LIST, NULL);
}

void cfm_preset_list_show_for(CFmPresetList *self, CFmPresets *presets)
{
	g_object_set(self, "model", cfm_presets_get_all(presets), NULL);

	gtk_widget_show_all(GTK_WIDGET(self));
}

