/*
 * GPL 2
 */

#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>

#include "presets.h"

G_DEFINE_TYPE(CFmPresets, cfm_presets, G_TYPE_OBJECT);

#define CFM_PRESETS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CFM_TYPE_PRESETS, CFmPresetsPrivate))

#define GCONF_KEY_BUFFER_LEN 1024
#define GCONF_PATH           "/apps/maemo/cfmradio/presets"

struct _CFmPresetsPrivate {
	GConfClient *gconf;
	gchar *name;
	gchar *gconf_dir;
	guint gconf_notify;
	GtkListStore *l;
	GHashTable *t;
};

enum {
	COL_INVALID = -1,
	COL_FREQUENCY = 0,
	COL_NAME
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_MODEL,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static inline gulong round_freq(gulong freq)
{
	const gulong round_to = 100000; /* 0.1 MHz */
	return ((freq + round_to / 2) / round_to) * round_to;
}

static inline gulong ffreq_to_freq(gfloat ffreq)
{
	return round_freq(ffreq * 1000000.0f);
}

static inline gfloat freq_to_ffreq(gulong freq)
{
	return freq / 1000000.0f;
}

static inline gpointer freq_to_pointer(gulong freq)
{
	return GUINT_TO_POINTER(freq);
}

static inline gulong pointer_to_freq(gpointer ptr)
{
	return GPOINTER_TO_UINT(ptr);
}

static inline gpointer ffreq_to_pointer(gfloat ffreq)
{
	return freq_to_pointer(ffreq_to_freq(ffreq));
}

static inline gfloat pointer_to_ffreq(gpointer ptr)
{
	return freq_to_ffreq(pointer_to_freq(ptr));
}

static void destroy_iter(gpointer data)
{
	g_slice_free(GtkTreeIter, data);
}

static void func_gconf_entry_free(gpointer data, gpointer user_data)
{
	GConfEntry *entry = (GConfEntry*) data;
	gconf_entry_free(entry);
}

static gint compare_freq(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
	gpointer user_data)
{
	gulong freq_a, freq_b;
	gtk_tree_model_get(model, a, 0, &freq_a, -1);
	gtk_tree_model_get(model, b, 0, &freq_b, -1);

	if (freq_a > freq_b) return 1;
	else if (freq_a < freq_b) return -1;
	else return 0;
}

static void cfm_presets_load(CFmPresets *self)
{
	CFmPresetsPrivate *priv = self->priv;

	g_debug("Loading presets from %s\n", priv->gconf_dir);

	g_hash_table_remove_all(priv->t);

	GSList *i, *l = gconf_client_all_entries(priv->gconf, priv->gconf_dir, NULL);
	for (i = l; i; i = g_slist_next(i)) {
		GConfEntry *entry = (GConfEntry*) i->data;
		const gchar *basename = g_basename(gconf_entry_get_key(entry));
		gfloat ffreq = g_ascii_strtod(basename, NULL);
		gulong freq = ffreq_to_freq(ffreq);
		const gchar *name = gconf_value_get_string(gconf_entry_get_value(entry));
		GtkTreeIter *iter = g_slice_new(GtkTreeIter);
		gtk_list_store_insert_with_values(priv->l, iter, 0, 
			COL_FREQUENCY, freq,
			COL_NAME, name,
			COL_INVALID
			);
		g_hash_table_insert(priv->t, freq_to_pointer(freq), iter);
	}

	g_slist_foreach(l, func_gconf_entry_free, NULL);
	g_slist_free(l);
}

static void cfm_presets_gconf_notify(GConfClient *gconf, guint cnxn_id,
	GConfEntry *entry, gpointer user_data)
{
	CFmPresets *self = CFM_PRESETS(user_data);
	CFmPresetsPrivate *priv = self->priv;

	if (!entry) {
		return;
	}

	const gchar *basename = g_basename(gconf_entry_get_key(entry));
	gfloat ffreq = g_ascii_strtod(basename, NULL);
	gulong freq = ffreq_to_freq(ffreq);
	gpointer t_data = g_hash_table_lookup(priv->t, freq_to_pointer(freq));
	GConfValue *value = gconf_entry_get_value(entry);

	if (value) {
		const gchar *name = gconf_value_get_string(gconf_entry_get_value(entry));
		if (t_data) {
			/* Modifying existing preset's name */
			GtkTreeIter *iter = (GtkTreeIter*)t_data;
			g_debug("Preset '%s' changed to '%s'\n", basename, name);
			gtk_list_store_set(priv->l, iter, COL_NAME, name, COL_INVALID);
		} else {
			GtkTreeIter *iter = g_slice_new(GtkTreeIter);
			g_debug("Preset '%s' set to '%s'\n", basename, name);
			gtk_list_store_insert_with_values(priv->l, iter, 0,
				COL_FREQUENCY, freq,
				COL_NAME, name,
				COL_INVALID
			);
			g_hash_table_insert(priv->t, freq_to_pointer(freq), iter);
		}
		
	} else {
		
		if (t_data) {
			GtkTreeIter *iter = (GtkTreeIter*)t_data;
			g_debug("Preset '%s' removed\n", basename);
			gtk_list_store_remove(priv->l, iter);
			g_hash_table_remove(priv->t, freq_to_pointer(freq));
		}
	}
}

static void cfm_presets_set_property(GObject *object, guint property_id,
	const GValue *value, GParamSpec *pspec)
{
	CFmPresets *self = CFM_PRESETS(object);
	switch (property_id) {
	case PROP_NAME:
		g_free(self->priv->name);
		self->priv->name = g_value_dup_string(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_presets_get_property(GObject *object, guint property_id,
	GValue *value, GParamSpec *pspec)
{
	CFmPresets *self = CFM_PRESETS(object);
	switch (property_id) {
	case PROP_NAME:
		g_value_set_string(value, self->priv->name);
		break;
	case PROP_MODEL:
		g_value_set_object(value, self->priv->l);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_presets_init(CFmPresets *self)
{
	CFmPresetsPrivate *priv;

	self->priv = priv = CFM_PRESETS_GET_PRIVATE(self);

	priv->gconf = gconf_client_get_default();
	priv->t = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, destroy_iter);
}

static GObject * cfm_presets_constructor(GType gtype, guint n_properties,
 GObjectConstructParam *properties)
{
	GObject *object = G_OBJECT_CLASS(cfm_presets_parent_class)->constructor(
		gtype, n_properties, properties);
	CFmPresets *self = CFM_PRESETS(object);
	CFmPresetsPrivate *priv = self->priv;

	priv->gconf_dir = g_strdup_printf("%s/%s", GCONF_PATH, priv->name);

	gconf_client_add_dir(priv->gconf, priv->gconf_dir, GCONF_CLIENT_PRELOAD_ONELEVEL,
		NULL);
	priv->gconf_notify = gconf_client_notify_add(priv->gconf, priv->gconf_dir,
		cfm_presets_gconf_notify, self, NULL, NULL);

	priv->l = gtk_list_store_new(2, G_TYPE_ULONG, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(priv->l), 0, compare_freq,
		NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(priv->l), 0,
		GTK_SORT_ASCENDING);

	cfm_presets_load(self);

	return object;
}

static void cfm_presets_dispose(GObject *object)
{
	CFmPresets *self = CFM_PRESETS(object);
	CFmPresetsPrivate *priv = self->priv;

	if (priv->gconf && priv->gconf_notify) {
		gconf_client_notify_remove(priv->gconf, priv->gconf_notify);
	}
	if (priv->gconf && priv->gconf_dir) {
		gconf_client_remove_dir(priv->gconf, priv->gconf_dir, NULL);
	}
	if (priv->gconf_dir) {
		g_free(priv->gconf_dir);
		priv->gconf_dir = NULL;
	}
	if (priv->gconf) {
		g_object_unref(priv->gconf);
		priv->gconf = NULL;
	}
	if (priv->l) {
		g_object_unref(priv->l);
		priv->l = NULL;
	}
}

static void cfm_presets_finalize(GObject *object)
{
	CFmPresets *self = CFM_PRESETS(object);
	CFmPresetsPrivate *priv = self->priv;

	g_hash_table_destroy(priv->t);
	g_free(priv->name);
}

static void cfm_presets_class_init(CFmPresetsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GParamSpec *param_spec;

	g_type_class_add_private (klass, sizeof(CFmPresetsPrivate));

	gobject_class->constructor = cfm_presets_constructor;
	gobject_class->set_property = cfm_presets_set_property;
	gobject_class->get_property = cfm_presets_get_property;
	gobject_class->dispose = cfm_presets_dispose;
	gobject_class->finalize = cfm_presets_finalize;

	param_spec = g_param_spec_string("name",
	                                 "Preset set name",
	                                 "This is the name for this set of presets",
	                                 "default",
	                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
	                                 G_PARAM_STATIC_STRINGS);
	properties[PROP_NAME] = param_spec;
	g_object_class_install_property(gobject_class, PROP_NAME, param_spec);
	param_spec = g_param_spec_object("model",
	                                 "Preset list model",
	                                 "A model containing this set of presets",
	                                 GTK_TYPE_TREE_MODEL,
	                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_MODEL] = param_spec;
	g_object_class_install_property(gobject_class, PROP_MODEL, param_spec);
}

static gpointer cfm_presets_build_default(gpointer data)
{
	return g_object_new(CFM_TYPE_PRESETS, NULL);
}

CFmPresets* cfm_presets_get_default()
{
	static GOnce once = G_ONCE_INIT;
	g_once(&once, cfm_presets_build_default, NULL);

	return CFM_PRESETS(g_object_ref(G_OBJECT(once.retval)));
}

CFmPresets* cfm_presets_get_for_name(const char * name)
{
	return g_object_new(CFM_TYPE_PRESETS, "name", name, NULL);
}

void cfm_presets_set_preset(CFmPresets *self, gulong freq, const gchar *name)
{
	CFmPresetsPrivate *priv = self->priv;
	GError *error = NULL;
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	gchar *key = g_strdup_printf("%s/%s", priv->gconf_dir,
		g_ascii_formatd(buf, sizeof(buf), "%.1f", freq_to_ffreq(freq)));
	if (!gconf_client_set_string(priv->gconf, key, name, &error)) {
		g_warning("Failed to store preset '%s' ('%s'): %s\n", key, name,
			error->message);
	}
	g_free(key);
}

void cfm_presets_remove_preset(CFmPresets *self, gulong freq)
{
	CFmPresetsPrivate *priv = self->priv;
	GError *error = NULL;
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	gchar *key = g_strdup_printf("%s/%s", priv->gconf_dir,
		g_ascii_formatd(buf, sizeof(buf), "%.1f", freq_to_ffreq(freq)));
	if (!gconf_client_unset(priv->gconf, key, &error)) {
		g_warning("Failed to remove preset '%s': %s\n", key, error->message);
	}
	g_free(key);
}

gboolean cfm_presets_is_preset(CFmPresets *self, gulong freq)
{
	CFmPresetsPrivate *priv = self->priv;

	return g_hash_table_lookup(priv->t, freq_to_pointer(freq)) ? TRUE : FALSE;
}

gchar * cfm_presets_get_preset(CFmPresets *self, gulong freq)
{
	CFmPresetsPrivate *priv = self->priv;

	gpointer t_data = g_hash_table_lookup(priv->t, freq_to_pointer(freq));
	if (t_data) {
		GtkTreeIter *iter = (GtkTreeIter*) t_data;
		gchar *name;
		gtk_tree_model_get(GTK_TREE_MODEL(priv->l), iter,
			COL_NAME, &name, COL_INVALID);
		return name;
	} else {
		return NULL;
	}

	return NULL;
}

