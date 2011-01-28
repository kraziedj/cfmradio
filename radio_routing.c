
#include <dbus/dbus.h>

#include "radio_routing.h"

#define SIGNAL_PATH		"/com/nokia/policy/decision"
#define SIGNAL_IFACE 	"com.nokia.policy"
#define SIGNAL_NAME		"audio_actions"

#define POLICY_AUDIO_ROUTE "com.nokia.policy.audio_route"

typedef struct {
	const char * type;
	const char * device;
	const char * mode;
	const char * hwid;
} audio_route;

static void add_entry(DBusMessageIter *array, const char *key, const char *value)
{
	DBusMessageIter str, var;

	dbus_message_iter_open_container(array, DBUS_TYPE_STRUCT, NULL, &str);
	dbus_message_iter_append_basic(&str, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&str, DBUS_TYPE_VARIANT,
		DBUS_TYPE_STRING_AS_STRING, &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container(&str, &var);
	dbus_message_iter_close_container(array, &str);
}

static void fill_array2(DBusMessageIter *array, const audio_route *route)
{
	add_entry(array, "type", route->type);
	add_entry(array, "device", route->device);
	add_entry(array, "mode", route->mode);
	add_entry(array, "hwid", route->hwid);
}

static void fill_dict(DBusMessageIter *dict, const audio_route *route)
{
	DBusMessageIter entry, array1, array2;
	const char * str = POLICY_AUDIO_ROUTE;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &str);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, 
		DBUS_TYPE_ARRAY_AS_STRING
			DBUS_STRUCT_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING
				DBUS_TYPE_VARIANT_AS_STRING
			DBUS_STRUCT_END_CHAR_AS_STRING, &array1);
	dbus_message_iter_open_container(&array1, DBUS_TYPE_ARRAY,
		DBUS_STRUCT_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING
			DBUS_TYPE_VARIANT_AS_STRING
		DBUS_STRUCT_END_CHAR_AS_STRING, &array2);
	fill_array2(&array2, route);
	dbus_message_iter_close_container(&array1, &array2);	
	dbus_message_iter_close_container(&entry, &array1);
	dbus_message_iter_close_container(dict, &entry);
}

static void route_audio(const audio_route *route)
{
	DBusError error;
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter it, dict;
	const dbus_uint32_t zero = 0;

	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	msg = dbus_message_new_signal(SIGNAL_PATH, SIGNAL_IFACE, SIGNAL_NAME);
	dbus_message_iter_init_append(msg, &it);
	dbus_message_iter_append_basic(&it, DBUS_TYPE_UINT32, &zero);

	dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY,  
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_ARRAY_AS_STRING
		DBUS_TYPE_ARRAY_AS_STRING
		DBUS_STRUCT_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING
			DBUS_TYPE_VARIANT_AS_STRING
		DBUS_STRUCT_END_CHAR_AS_STRING
		DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
		&dict);
	fill_dict(&dict, route);
	dbus_message_iter_close_container(&it, &dict);

	dbus_connection_send(conn, msg, NULL);
	dbus_message_unref(msg);
}

void cfm_radio_route_audio_to_headphones()
{
	audio_route route = {
		.type = "sink",
		.device = "headphone",
		.mode = "lineout",
		.hwid = "na"
	};
	route_audio(&route);
}

void cfm_radio_route_audio_to_speakers()
{
	audio_route route = {
		.type = "sink",
		.device = "ihf",
		.mode = "ihf",
		.hwid = "na"
	};
	route_audio(&route);
}

void cfm_radio_route_audio_bypass()
{
	audio_route route = {
		.type = "source",
		.device = "fmrx",
		.mode = "na",
		.hwid = "na"
	};
	route_audio(&route);
}

void cfm_radio_route_audio_reset()
{
	audio_route route = {
		.type = "sink",
		.device = "headphone",
		.mode = "lineout",
		.hwid = "na"
	};
	route_audio(&route);
	audio_route route2 = {
		.type = "source",
		.device = "microphone",
		.mode = "na",
		.hwid = "na"
	};
	route_audio(&route2);
}

