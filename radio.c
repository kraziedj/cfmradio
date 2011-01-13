/*
 * GPL 2
 */

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>
#include <linux/videodev2.h>
#include <dbus/dbus-glib.h>
#include <pulse/glib-mainloop.h>
#include <pulse/error.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/xmalloc.h>

#include "radio.h"
#include "types.h"
#include "n900-fmrx-enabler.h"
#include "rds.h"

G_DEFINE_TYPE(CFmRadio, cfm_radio, G_TYPE_OBJECT);

#define CFM_RADIO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CFM_TYPE_RADIO, CFmRadioPrivate))

#define ABS_RANGE_LOW 60000000
#define ABS_RANGE_HIGH 140000000
#define MIN_BUFFER_SIZE (4096*4)

#define FMRX_SERVICE_NAME "de.pycage.FMRXEnabler"
#define FMRX_OBJECT_PATH  "/de/pycage/FMRXEnabler"
#define FMRX_INTERFACE    "de.pycage.FMRXEnabler"
#define FMRX_KEEPALIVE_INTERVAL 20

#define SYSFS_NODE_PATH	"/sys/class/i2c-adapter/i2c-3/3-0022"

#define MIXER_NAME			"hw:0"

static void cfm_radio_turn_on(CFmRadio *self);
static void cfm_radio_turn_off(CFmRadio *self);

struct _CFmRadioPrivate {
	int fd;

	CFmRadioOutput output;

	gboolean precise_tuner;
	gulong range_low, range_high;

	DBusGProxy *enabler;
	guint enabler_timer;

	pa_glib_mainloop *pa_loop;
	pa_context *pa_ctx;
	pa_stream *so, *si;

	snd_hctl_t *mixer;
};

enum {
	PROP_0,
	PROP_OUTPUT,
	PROP_FREQUENCY,
	PROP_RANGE_LOW,
	PROP_RANGE_HIGH,
	PROP_SIGNAL,
	PROP_RDS_PI,
	PROP_RDS_PS,
	PROP_RDS_RT,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void cfm_radio_tuner_power(CFmRadio *self, gboolean enable)
{
	CFmRadioPrivate *priv = self->priv;
	struct v4l2_control vctrl;
	int res;

	if (priv->fd == -1) return;

	vctrl.id = V4L2_CID_AUDIO_MUTE;
	vctrl.value = enable ? 0 : 1;
	res = ioctl(priv->fd, VIDIOC_S_CTRL, &vctrl);

	if (res < 0) {
		perror("VIDIOC_S_CTRL");
	}
}

static void cfm_radio_mixer_set_enum_value(CFmRadio *self, const char * name, const char * value)
{
	CFmRadioPrivate *priv = self->priv;
	int err;

	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, name);

	snd_hctl_elem_t *elem = snd_hctl_find_elem(priv->mixer, id);
	g_return_if_fail(elem != NULL);

	snd_ctl_elem_info_t *info;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_id(info, id);
	err = snd_hctl_elem_info(elem, info);
	g_return_if_fail(err == 0);	

	long value_idx = -1;
	int i, items = snd_ctl_elem_info_get_items(info);
	for (i = 0; i < items; i++) {
		snd_ctl_elem_info_set_item(info, i);
		err = snd_hctl_elem_info(elem, info);
		g_return_if_fail(err == 0);
		if (strcmp(snd_ctl_elem_info_get_item_name(info), value) == 0) {
			value_idx = i;
			break;
		}
	}
	g_return_if_fail(value_idx >= 0);

	snd_ctl_elem_value_t *control;
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);

	items = snd_ctl_elem_info_get_count(info);
	for (i = 0; i < items; i++) {
		snd_ctl_elem_value_set_enumerated(control, i, value_idx);
	}

	err = snd_hctl_elem_write(elem, control);
	g_return_if_fail(err == 0);
}

static void cfm_radio_mixer_set_bool_value(CFmRadio *self, const char * name, gboolean value)
{
	CFmRadioPrivate *priv = self->priv;
	int err;

	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, name);

	snd_hctl_elem_t *elem = snd_hctl_find_elem(priv->mixer, id);
	g_return_if_fail(elem != NULL);

	snd_ctl_elem_info_t *info;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_id(info, id);
	err = snd_hctl_elem_info(elem, info);
	g_return_if_fail(err == 0);	

	snd_ctl_elem_value_t *control;
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);

	int i, items = snd_ctl_elem_info_get_count(info);
	for (i = 0; i < items; i++) {
		snd_ctl_elem_value_set_boolean(control, i, value);
	}

	err = snd_hctl_elem_write(elem, control);
	g_return_if_fail(err == 0);
}

static void cfm_radio_mixer_enable(CFmRadio *self, gboolean enable)
{
	if (enable) {
		cfm_radio_mixer_set_enum_value(self, "Input Select", "ADC");
		cfm_radio_mixer_set_bool_value(self, "PGA Capture Switch", TRUE);
		cfm_radio_mixer_set_bool_value(self, "Left PGA Mixer Line2L Switch", TRUE);
		cfm_radio_mixer_set_bool_value(self, "Right PGA Mixer Line2R Switch", TRUE);
	} else {
		cfm_radio_mixer_set_enum_value(self, "Input Select", "Digital Mic");
		cfm_radio_mixer_set_bool_value(self, "PGA Capture Switch", FALSE);
		cfm_radio_mixer_set_bool_value(self, "Left PGA Mixer Line2L Switch", FALSE);
		cfm_radio_mixer_set_bool_value(self, "Right PGA Mixer Line2R Switch", FALSE);
	}
}

static void cfm_radio_init_tuner(CFmRadio *self, const gchar * device)
{
	CFmRadioPrivate *priv = self->priv;
	struct v4l2_tuner tuner = { 0 };
	int res;

	priv->fd = open(device, O_RDONLY);
	if (priv->fd == -1) {
		perror("Open radio tuner");
	}

	tuner.index = 0;
	res = ioctl(priv->fd, VIDIOC_G_TUNER, &tuner);
	if (res < 0) {
		perror("VIDIOC_G_TUNER");
		return;
	}

	if (tuner.type != V4L2_TUNER_RADIO) {
		g_warning("Not a radio tuner\n");
		return;
	}

	priv->precise_tuner = (tuner.capability & V4L2_TUNER_CAP_LOW) ?
	                        TRUE : FALSE;

	if (priv->precise_tuner) {
		priv->range_low = tuner.rangelow * 62.5f;
		priv->range_high = tuner.rangehigh * 62.5f;
	} else {
		priv->range_low = tuner.rangelow * 62500;
		priv->range_high = tuner.rangehigh * 62500;
	}

	g_debug("Tuner detected (from %lu to %lu)\n", priv->range_low, priv->range_high);

	cfm_radio_tuner_power(self, TRUE);

	g_debug("Tuner powered!\n");

	g_object_notify(G_OBJECT(self), "range-low");
	g_object_notify(G_OBJECT(self), "range-high");
	g_object_notify(G_OBJECT(self), "frequency");
}

static void cfm_radio_fmrx_request_cb(DBusGProxy *proxy, gint result, char * device, GError *error, gpointer userdata)
{
	CFmRadio *self = CFM_RADIO(userdata);
	CFmRadioPrivate *priv = self->priv;

	if (error) {
		g_warning("D-Bus error while contacting fmrx enabler: %s\n",
			error->message);
		g_error_free(error);
	} else if (result != 0) {
		g_warning("Ungranted acess to fmrx device (%d)\n", result);
	} else if (priv->fd != -1) {
		g_debug("Renewed access to device\n");
	} else {
		g_debug("Granted access to device: %s\n", device);
		cfm_radio_init_tuner(self, device);
		/* TODO: Possibly initialize more stuff. */
	}
	
	if (device) {
		g_free(device);
	}
}

static void cfm_radio_fmrx_request(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	g_return_if_fail(priv->enabler);

	de_pycage_FMRXEnabler_request_async(priv->enabler,
		cfm_radio_fmrx_request_cb, self);
}

static gboolean cfm_radio_fmrx_keepalive(gpointer data)
{
	CFmRadio *self = CFM_RADIO(data);
	CFmRadioPrivate *priv = self->priv;

	if (!priv->enabler) {
		priv->enabler_timer = 0;
		return FALSE;
	}

	cfm_radio_fmrx_request(self);

	return TRUE;
}

static void cfm_radio_ctx_state_change(pa_context *c, void *userdata)
{
	CFmRadio *self = CFM_RADIO(userdata);
	CFmRadioPrivate *priv = self->priv;
	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_READY:
		if (priv->output != CFM_RADIO_OUTPUT_MUTE && (!priv->so || !priv->si)) {
			cfm_radio_turn_on(self);
		}
	break;
	case PA_CONTEXT_FAILED:
		cfm_radio_turn_off(self);
		g_warning("Pulseaudio connection failed\n");
	break;
	default:
	break;
	}
}

static void cfm_radio_si_request(pa_stream *p, size_t nbytes, void *userdata)
{
	CFmRadio *self = CFM_RADIO(userdata);
	CFmRadioPrivate *priv = self->priv;

	if (nbytes < MIN_BUFFER_SIZE) 
		return;

	void * in;
	size_t in_nbytes;
	int res = pa_stream_peek(priv->si, (const void **) &in, &in_nbytes);
	if (res != 0) {
		g_warning("Failed to read from input stream: %s\n", pa_strerror(res));
		return;
	}
	g_warn_if_fail(in);

	res = pa_stream_write(priv->so, in, in_nbytes, NULL, 0, PA_SEEK_RELATIVE);
	if (res != 0) {
		g_warning("Failed to write to output stream: %s\n", pa_strerror(res));
		return;
	}
	
	pa_stream_drop(priv->si);
}

static void cfm_radio_turn_on(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	/* A good default for the N900. */
	pa_sample_spec spec = {
		.format = PA_SAMPLE_S16LE,
		.rate = 48000,
		.channels = 2
	};
	int res;
	g_warn_if_fail(priv->pa_ctx);

	priv->si = pa_stream_new(priv->pa_ctx, "FMRadio input", &spec, NULL);
	priv->so = pa_stream_new(priv->pa_ctx, "FMRadio output", &spec, NULL);

	pa_stream_set_read_callback(priv->si, cfm_radio_si_request, self);

	res = pa_stream_connect_playback(priv->so, NULL, NULL, 0, NULL, NULL);
	if (res != 0) {
		g_warning("Failed to connect output stream: %s\n", pa_strerror(res));
	}
	res = pa_stream_connect_record(priv->si, NULL, NULL, 0);
	if (res != 0) {
		g_warning("Failed to connect input stream: %s\n", pa_strerror(res));
	}

	cfm_radio_mixer_enable(self, TRUE);

	g_debug("Turned on\n");
}

static void cfm_radio_turn_off(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	if (priv->so) {
		pa_stream_disconnect(priv->so);
		pa_stream_unref(priv->so);
		priv->so = NULL;
	}
	if (priv->si) {
		pa_stream_disconnect(priv->si);
		pa_stream_unref(priv->si);
		priv->si = NULL;
	}
	cfm_radio_mixer_enable(self, FALSE);
	g_debug("Turned off\n");
}

static void cfm_radio_init(CFmRadio *self)
{
	CFmRadioPrivate *priv;
	GError *error = NULL;
	int res;

	self->priv = priv = CFM_RADIO_GET_PRIVATE(self);
	priv->fd = -1;

	DBusGConnection *conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!conn) {
		g_warning("Failed to get system bus: %s\n", error->message);
		g_error_free(error);
	}

	priv->enabler = dbus_g_proxy_new_for_name_owner(conn, FMRX_SERVICE_NAME,
		FMRX_OBJECT_PATH, FMRX_INTERFACE, &error);
	if (!priv->enabler) {
		g_warning("Failed to connect to fmrx enabler service: %s\n",
			error->message);
		g_error_free(error);
	}

	cfm_radio_fmrx_request(self);

	priv->pa_loop = pa_glib_mainloop_new(NULL);
	priv->pa_ctx = pa_context_new(pa_glib_mainloop_get_api(priv->pa_loop),
		"FMRadio"); /* Note that the name is very important on Maemo. */
	pa_context_set_state_callback(priv->pa_ctx, cfm_radio_ctx_state_change, self);
	res = pa_context_connect(priv->pa_ctx, NULL, 0, NULL);
	g_warn_if_fail(res == 0);

	priv->enabler_timer = g_timeout_add_seconds(FMRX_KEEPALIVE_INTERVAL,
		cfm_radio_fmrx_keepalive, self);

	res = snd_hctl_open(&priv->mixer, MIXER_NAME, 0);
	if (res < 0) {
		g_warning("Failed to open ALSA mixer res=%d\n", res);
	}
	res = snd_hctl_load(priv->mixer);
	if (res < 0) {
		g_warning("Failed to load ALSA hmixer elements res=%d\n", res);
	}
}

static void cfm_radio_set_output(CFmRadio *self, CFmRadioOutput mode)
{
	CFmRadioPrivate *priv = self->priv;
	//CFmRadioOutput old_output = priv->output;
	priv->output = mode;
	if (mode == CFM_RADIO_OUTPUT_MUTE) {
		cfm_radio_turn_off(self);
	} else if (pa_context_get_state(priv->pa_ctx) == PA_CONTEXT_READY) {
		cfm_radio_turn_on(self);
	}
}

static CFmRadioOutput cfm_radio_get_output(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	return priv->output;
}

static void cfm_radio_set_frequency(CFmRadio *self, gulong freq)
{
	CFmRadioPrivate *priv = self->priv;
	struct v4l2_frequency t_freq = { 0 };
	g_return_if_fail(priv->fd != -1);
	t_freq.tuner = 0;
	t_freq.type = V4L2_TUNER_RADIO;
	t_freq.frequency = priv->precise_tuner ? freq / 62.5f : freq / 62500;
	int res = ioctl(priv->fd, VIDIOC_S_FREQUENCY, &t_freq);
	g_warn_if_fail(res == 0);
}

static gulong cfm_radio_get_frequency(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	struct v4l2_frequency t_freq = { 0 };
	g_return_val_if_fail(priv->fd != -1, 0);
	t_freq.tuner = 0;
	int res = ioctl(priv->fd, VIDIOC_G_FREQUENCY, &t_freq);
	g_return_val_if_fail(res == 0, 0);
	return priv->precise_tuner ? t_freq.frequency * 62.5f : t_freq.frequency * 62500;
}

static guint cfm_radio_get_signal(CFmRadio *self)
{
	CFmRadioPrivate *priv = self->priv;
	struct v4l2_tuner tuner = { 0 };
	g_return_val_if_fail(priv->fd != -1, 0);
	tuner.index = 0;
	int res = ioctl(priv->fd, VIDIOC_G_TUNER, &tuner);
	g_return_val_if_fail(res == 0, 0);
	return tuner.signal;
}

static gchar* cfm_radio_get_sysfs_key(CFmRadio *self, const gchar *key)
{
	GError *error = NULL;
	gchar *file = g_strdup_printf("%s/%s", SYSFS_NODE_PATH, key);
	gchar *r = NULL;

	if (!g_file_get_contents(file, &r, NULL, &error)) {
		g_warning("Unable to read sysfs key %s: %s\n", key, error->message);
	}

	g_free(file);

	return r;
}

static gchar* cfm_radio_get_rds(CFmRadio *self, const gchar *key)
{
	gchar *v = cfm_radio_get_sysfs_key(self, key);
	if (v) {
		gchar * r = rds_decode(v);
		g_free(v);
		return r;
	} else {
		return NULL;
	}
}

static void cfm_radio_set_property(GObject *object, guint property_id,
	const GValue *value, GParamSpec *pspec)
{
	CFmRadio *self = CFM_RADIO(object);
	switch (property_id) {
	case PROP_OUTPUT:
		cfm_radio_set_output(self, g_value_get_enum(value));
		break;
	case PROP_FREQUENCY:
		cfm_radio_set_frequency(self, g_value_get_ulong(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_radio_get_property(GObject *object, guint property_id,
	GValue *value, GParamSpec *pspec)
{
	CFmRadio *self = CFM_RADIO(object);
	switch (property_id) {
	case PROP_OUTPUT:
		g_value_set_enum(value, cfm_radio_get_output(self));
		break;
	case PROP_FREQUENCY:
		g_value_set_ulong(value, cfm_radio_get_frequency(self));
		break;
	case PROP_RANGE_LOW:
		g_value_set_ulong(value, self->priv->range_low);
		break;
	case PROP_RANGE_HIGH:
		g_value_set_ulong(value, self->priv->range_high);
		break;
	case PROP_SIGNAL:
		g_value_set_uint(value, cfm_radio_get_signal(self));
		break;
	case PROP_RDS_PI:
		g_value_take_string(value, cfm_radio_get_rds(self, "rds_pi"));
		break;
	case PROP_RDS_PS:
		g_value_take_string(value, cfm_radio_get_rds(self, "rds_ps"));
		break;
	case PROP_RDS_RT:
		g_value_take_string(value, cfm_radio_get_rds(self, "rds_rt"));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_radio_dispose(GObject *object)
{
	CFmRadio *self = CFM_RADIO(object);
	CFmRadioPrivate *priv = self->priv;
	if (priv->enabler_timer) {
		g_source_remove(priv->enabler_timer);
		priv->enabler_timer = 0;
	}
	cfm_radio_tuner_power(self, FALSE);
	cfm_radio_turn_off(self);
	if (priv->enabler) {
		g_object_unref(priv->enabler);
		priv->enabler = NULL;
	}
	if (priv->pa_ctx) {
		pa_context_disconnect(priv->pa_ctx);
		pa_context_unref(priv->pa_ctx);
		priv->pa_ctx = NULL;
	}
	if (priv->pa_loop) {
		pa_glib_mainloop_free(priv->pa_loop);
		priv->pa_loop = NULL;
	}
}

static void cfm_radio_finalize(GObject *object)
{
	CFmRadio *self = CFM_RADIO(object);
	CFmRadioPrivate *priv = self->priv;
	if (priv->mixer) {
		snd_hctl_close(priv->mixer);
		priv->mixer = NULL;
	}
	if (priv->fd != -1) {
		close(priv->fd);
		priv->fd = -1;
	}
}

static void cfm_radio_class_init(CFmRadioClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *param_spec;

	gobject_class->set_property = cfm_radio_set_property;
	gobject_class->get_property = cfm_radio_get_property;
	gobject_class->dispose = cfm_radio_dispose;
	gobject_class->finalize = cfm_radio_finalize;

	g_type_class_add_private (klass, sizeof(CFmRadioPrivate));

	param_spec = g_param_spec_enum("output", "Output device",
	                               "Audio output device",
                                    CFM_TYPE_RADIO_OUTPUT,
									CFM_RADIO_OUTPUT_MUTE,
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_OUTPUT] = param_spec;
	g_object_class_install_property(gobject_class, PROP_OUTPUT, param_spec);

	param_spec = g_param_spec_ulong("frequency",
	                                "Frequency to tune (Hz)",
	                                "This is the frequency to tune, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_LOW,
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_FREQUENCY] = param_spec;
	g_object_class_install_property(gobject_class, PROP_FREQUENCY, param_spec);

	param_spec = g_param_spec_ulong("range-low",
	                                "Min frequency range (Hz)",
	                                "This is the lowest frequency that can be tuned, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_LOW,
	                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_property(gobject_class, PROP_RANGE_LOW, param_spec);
	properties[PROP_RANGE_LOW] = param_spec;
	param_spec = g_param_spec_ulong("range-high",
	                                "Max frequency range (Hz)",
	                                "This is the highest frequency that can be tuned, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_HIGH,
	                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_RANGE_HIGH] = param_spec;
	g_object_class_install_property(gobject_class, PROP_RANGE_HIGH, param_spec);
	param_spec = g_param_spec_uint("signal",
	                               "Signal strength",
	                               "The signal strength if known, ranging from 0 (worse) to 65536 (best)",
	                                0, 65536, 0,
	                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SIGNAL] = param_spec;
	g_object_class_install_property(gobject_class, PROP_SIGNAL, param_spec);
	param_spec = g_param_spec_string("rds-pi",
	                                 "RDS Program Identification",
	                                 "Current station's code",
	                                 "",
	                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_RDS_PI] = param_spec;
	g_object_class_install_property(gobject_class, PROP_RDS_PI, param_spec);
	param_spec = g_param_spec_string("rds-ps",
	                                 "RDS Program Server",
	                                 "Current station's name",
	                                 "",
	                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_RDS_PS] = param_spec;
	g_object_class_install_property(gobject_class, PROP_RDS_PS, param_spec);
	param_spec = g_param_spec_string("rds-rt",
	                                 "RDS Radio Text",
	                                 "Current station's radio text",
	                                 "",
	                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_RDS_RT] = param_spec;
	g_object_class_install_property(gobject_class, PROP_RDS_RT, param_spec);
}

CFmRadio* cfm_radio_new()
{
	return g_object_new(CFM_TYPE_RADIO, NULL);
}

