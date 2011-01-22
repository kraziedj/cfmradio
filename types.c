
#include <glib-object.h>
#include "types.h"

GType cfm_radio_output_get_type(void)
{
    static GType etype = 0;

    if (etype == 0) {
        static const GEnumValue values[] = {
            { CFM_RADIO_OUTPUT_MUTE, "CFM_RADIO_OUTPUT_MUTE", "mute" },
			{ CFM_RADIO_OUTPUT_SYSTEM, "CFM_RADIO_OUTPUT_SYSTEM", "system" },
            { CFM_RADIO_OUTPUT_SPEAKER, "CFM_RADIO_OUTPUT_SPEAKER", "speaker" },
            { CFM_RADIO_OUTPUT_HEADPHONES, "CFM_RADIO_OUTPUT_HEADPHONES", "headphones" },
            { CFM_RADIO_OUTPUT_HEADPHONES_BYPASS, "CFM_RADIO_OUTPUT_HEADPHONES_BYPASS",
				"headphones-bypass" },
            { 0, NULL, NULL }
        };
        etype = g_enum_register_static("CFmRadioOutput", values);
    }
    return etype;
}

