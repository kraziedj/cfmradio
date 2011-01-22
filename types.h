
#ifndef CFM_TYPES_H
#define CFM_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	CFM_RADIO_OUTPUT_MUTE = 0,
	CFM_RADIO_OUTPUT_SYSTEM,
	CFM_RADIO_OUTPUT_SPEAKER,
	CFM_RADIO_OUTPUT_HEADPHONES,
	CFM_RADIO_OUTPUT_HEADPHONES_BYPASS
} CFmRadioOutput;

GType cfm_radio_output_get_type(void) G_GNUC_CONST;
#define CFM_TYPE_RADIO_OUTPUT (cfm_radio_output_get_type())

G_END_DECLS

#endif /* CFM_TYPES_H */

