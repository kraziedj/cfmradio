/*
 * GPL 2
 */

#ifndef _CFM_PRESET_LIST_H_
#define _CFM_PRESET_LIST_H_

#include <hildon/hildon.h>
#include "presets.h"

#define CFM_TYPE_PRESET_LIST                  (cfm_preset_list_get_type ())
#define CFM_PRESET_LIST(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CFM_TYPE_PRESET_LIST, CFmPresetList))
#define CFM_IS_PRESET_LIST(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CFM_TYPE_PRESET_LIST))
#define CFM_PRESET_LIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CFM_TYPE_PRESET_LIST, CFmPresetListClass))
#define CFM_IS_PRESET_LIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CFM_TYPE_PRESET_LIST))
#define CFM_PRESET_LIST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CFM_TYPE_PRESET_LIST, CFmPresetListClass))

typedef struct _CFmPresetList        CFmPresetList;
typedef struct _CFmPresetListPrivate CFmPresetListPrivate;
typedef struct _CFmPresetListClass   CFmPresetListClass;

struct _CFmPresetList
{
	HildonStackableWindow parent;
	CFmPresetListPrivate *priv;
};

struct _CFmPresetListClass
{
	HildonStackableWindowClass parent;
};

GType cfm_preset_list_get_type (void);
CFmPresetList* cfm_preset_list_new();
void cfm_preset_list_show_for(CFmPresetList *self, CFmPresets *presets);

#endif /* _CFM_PRESET_LIST_H_ */
