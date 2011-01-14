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
	GHashTable *t;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_LAST
};

enum {
	SIGNAL_0,
	SIGNAL_LAST
};

static GParamSpec *properties[PROP_LAST];
static guint signals[SIGNAL_LAST];

static inline gfloat freq_to_float(gulong f)
{
	return f / 1000000.0;
}

static inline gulong float_to_freq(gfloat f)
{
	return f * 1000000.0;
}

static inline gfloat pointer_to_float(gpointer p)
{
	union {
		gfloat f;
		gpointer p;
	} u;
	u.p = p;
	return u.f;
}

static inline gpointer float_to_pointer(gfloat f)
{
	union {
		gfloat f;
		gpointer p;
	} u;
	u.f = f;
	return u.p;
}

static inline gpointer freq_to_pointer(gulong f)
{
	return float_to_pointer(freq_to_float(f));
}

static inline gulong pointer_to_freq(gpointer p)
{
	return float_to_freq(pointer_to_float(p));
}

static void func_gconf_entry_free(gpointer data, gpointer user_data)
{
	GConfEntry *entry = (GConfEntry*) data;
	gconf_entry_free(entry);
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
		gfloat freq = g_ascii_strtod(basename, NULL);
		const gchar *name = gconf_value_get_string(gconf_entry_get_value(entry));
		g_hash_table_insert(priv->t, float_to_pointer(freq), g_strdup(name));
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
	gfloat freq = g_ascii_strtod(basename, NULL);
	GConfValue *value = gconf_entry_get_value(entry);

	if (value) {
		const gchar *name = gconf_value_get_string(gconf_entry_get_value(entry));
		g_debug("Preset '%s' changed to '%s'\n", basename, name);
		g_hash_table_insert(priv->t, float_to_pointer(freq), g_strdup(name));
	} else {
		g_debug("Preset '%s' removed\n", basename);
		g_hash_table_remove(priv->t, float_to_pointer(freq));
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
	priv->t = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
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
		g_ascii_formatd(buf, sizeof(buf), "%.1f", freq_to_float(freq)));
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
		g_ascii_formatd(buf, sizeof(buf), "%.1f", freq_to_float(freq)));
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

const gchar * cfm_presets_get_preset(CFmPresets *self, gulong freq)
{
	CFmPresetsPrivate *priv = self->priv;

	gpointer found = g_hash_table_lookup(priv->t, freq_to_pointer(freq));
	if (found) {
		return (const gchar *) found;
	} else {
		return NULL;
	}

	return NULL;
}

static void preset_to_list_store(gpointer key, gpointer value, gpointer user_data)
{
	GtkListStore *l = GTK_LIST_STORE(user_data);
	GtkTreeIter iter;

	gulong freq = pointer_to_freq(key);
	gtk_list_store_insert_with_values(l, &iter, 0, 0, freq, 1, value, -1);
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

GtkListStore* cfm_presets_get_all(CFmPresets *self)
{
	CFmPresetsPrivate *priv = self->priv;
	GtkListStore *l = gtk_list_store_new(2, G_TYPE_ULONG, G_TYPE_STRING);
	g_hash_table_foreach(priv->t, preset_to_list_store, l);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(l), 0, compare_freq,
		NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(l), 0,
		GTK_SORT_ASCENDING);

	return l;
}

