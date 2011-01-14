/*
 * GPL 2
 */

#ifndef _CFM_PRESET_RENDERER_H_
#define _CFM_PRESET_RENDERER_H_

#include <gtk/gtk.h>

#define CFM_TYPE_PRESET_RENDERER             (cfm_preset_renderer_get_type ())
#define CFM_PRESET_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CFM_TYPE_PRESET_RENDERER, CFmPresetRenderer))
#define CFM_PRESET_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CFM_TYPE_PRESET_RENDERER, CFmPresetRendererClass))
#define CFM_IS_PRESET_RENDERER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CFM_TYPE_PRESET_RENDERER))
#define CFM_IS_PRESET_RENDERER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CFM_TYPE_PRESET_RENDERER))
#define CFM_PRESET_RENDERER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CFM_TYPE_PRESET_RENDERER, CFmPresetRendererClass))

typedef struct _CFmPresetRendererClass CFmPresetRendererClass;
typedef struct _CFmPresetRenderer CFmPresetRenderer;

struct _CFmPresetRendererClass
{
	GtkCellRendererClass parent_class;
};

struct _CFmPresetRenderer
{
	GtkCellRenderer parent_instance;
	gchar *name;
	gchar *freq_text;
	float frequency;
};

GType cfm_preset_renderer_get_type(void) G_GNUC_CONST;
GtkCellRenderer* cfm_preset_renderer_new(void);

G_END_DECLS

#endif /* _CFM_PRESET_RENDERER_H_ */
