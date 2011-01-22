/*
 * GPL 2
 */

#ifndef __CFM_PRESETS_H__
#define __CFM_PRESETS_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define CFM_TYPE_PRESETS                  (cfm_presets_get_type ())
#define CFM_PRESETS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CFM_TYPE_PRESETS, CFmPresets))
#define CFM_IS_PRESETS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CFM_TYPE_PRESETS))
#define CFM_PRESETS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CFM_TYPE_PRESETS, CFmPresetsClass))
#define CFM_IS_PRESETS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CFM_TYPE_PRESETS))
#define CFM_PRESETS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CFM_TYPE_PRESETS, CFmPresetsClass))

typedef struct _CFmPresets        CFmPresets;
typedef struct _CFmPresetsPrivate CFmPresetsPrivate;
typedef struct _CFmPresetsClass   CFmPresetsClass;

struct _CFmPresets
{
	GObject parent_instance;
	CFmPresetsPrivate *priv;
};

struct _CFmPresetsClass
{
	GObjectClass parent_class;
};

GType cfm_presets_get_type(void) G_GNUC_CONST;
CFmPresets* cfm_presets_get_default();
CFmPresets* cfm_presets_get_for_name(const gchar *name);

void cfm_presets_set_preset(CFmPresets *self, gulong freq, const gchar *name);
void cfm_presets_remove_preset(CFmPresets *self, gulong freq);
gboolean cfm_presets_is_preset(CFmPresets *self, gulong freq);
gchar* cfm_presets_get_preset(CFmPresets *self, gulong freq);

#endif /* __CFM_PRESETS_H__ */

