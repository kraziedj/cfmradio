/*
 * GPL 2
 */

#ifndef __CFM_RADIO_H__
#define __CFM_RADIO_H__

#include <glib-object.h>

#define CFM_TYPE_RADIO                  (cfm_radio_get_type ())
#define CFM_RADIO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CFM_TYPE_RADIO, CFmRadio))
#define CFM_IS_RADIO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CFM_TYPE_RADIO))
#define CFM_RADIO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CFM_TYPE_RADIO, CFmRadioClass))
#define CFM_IS_RADIO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CFM_TYPE_RADIO))
#define CFM_RADIO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CFM_TYPE_RADIO, CFmRadioClass))

typedef struct _CFmRadio        CFmRadio;
typedef struct _CFmRadioPrivate CFmRadioPrivate;
typedef struct _CFmRadioClass   CFmRadioClass;

struct _CFmRadio
{
  GObject parent_instance;
  CFmRadioPrivate *priv;
};

struct _CFmRadioClass
{
  GObjectClass parent_class;
};

GType cfm_radio_get_type (void);
CFmRadio* cfm_radio_new();

void cfm_radio_seek_up(CFmRadio* radio);
void cfm_radio_seek_down(CFmRadio* radio);

#endif /* __CFM_RADIO_H__ */

