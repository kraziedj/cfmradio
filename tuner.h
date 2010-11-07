/*
 * GPL 2
 */

#ifndef __CFM_TUNER_H__
#define __CFM_TUNER_H__

#include <gtk/gtk.h>

#define CFM_TYPE_TUNER                  (cfm_tuner_get_type ())
#define CFM_TUNER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CFM_TYPE_TUNER, CFmTuner))
#define CFM_IS_TUNER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CFM_TYPE_TUNER))
#define CFM_TUNER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CFM_TYPE_TUNER, CFmTunerClass))
#define CFM_IS_TUNER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CFM_TYPE_TUNER))
#define CFM_TUNER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CFM_TYPE_TUNER, CFmTunerClass))

typedef struct _CFmTuner        CFmTuner;
typedef struct _CFmTunerPrivate CFmTunerPrivate;
typedef struct _CFmTunerClass   CFmTunerClass;

struct _CFmTuner
{
	GtkDrawingArea parent;
	CFmTunerPrivate *priv;
};

struct _CFmTunerClass
{
	GtkDrawingAreaClass parent;
};

GType cfm_tuner_get_type (void);
CFmTuner* cfm_tuner_new();

#endif /* __CFM_TUNER_H__ */

