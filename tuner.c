/*
 * GPL 2
 */

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include "tuner.h"

G_DEFINE_TYPE(CFmTuner, cfm_tuner, GTK_TYPE_DRAWING_AREA);

#define CFM_TUNER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CFM_TYPE_TUNER, CFmTunerPrivate))

#define ABS_RANGE_LOW 60000000
#define ABS_RANGE_HIGH 140000000

#define SCALE_TENTH_MHZ_PIXELS 15.0
#define LABELS_FONT_SPEC "Nokia Sans 18"

struct _CFmTunerPrivate {
	gulong range_low, range_high;
	gulong freq;
	gboolean dragging;
	gdouble drag_start_x;
};

enum {
	PROP_0,
	PROP_FREQUENCY,
	PROP_RANGE_LOW,
	PROP_RANGE_HIGH,
	PROP_LAST
};

enum {
	SIGNAL_0,
	SIGNAL_FREQUENCY_CHANGED,
	SIGNAL_FREQUENCY_TUNED,
	SIGNAL_LAST
};

static GParamSpec *properties[PROP_LAST];
static guint signals[SIGNAL_LAST];

static void cfm_tuner_draw(CFmTuner *self, cairo_t *cr)
{
	const GtkAllocation *size = &( GTK_WIDGET(self)->allocation );
	const double w = size->width, h = size->height;
	const double midx = w / 2;
	const double cur_f = self->priv->freq / 100000.0; // Tenths of a Mhz
	const double left_f = cur_f - (midx / SCALE_TENTH_MHZ_PIXELS);

	double left_f_int;
	double left_f_frac = modf(left_f, &left_f_int);

	static gchar text[12];
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(LABELS_FONT_SPEC);
	double x = (1.0 - left_f_frac) * SCALE_TENTH_MHZ_PIXELS;
	gulong f = left_f_int + 1;
	cairo_set_source_rgb(cr, 1, 1, 1);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	while (x <= w) {
		if (f % 10 == 0) {
			gint tw, th;
			sprintf(text, "%lu", f / 10);
			pango_layout_set_text(layout, text, -1);
			pango_cairo_update_layout(cr, layout);
			pango_layout_get_size(layout, &tw, &th);
			cairo_move_to(cr, x - (tw / (PANGO_SCALE * 2)), 0.0);
			pango_cairo_show_layout(cr, layout);
			cairo_move_to(cr, x, 0.3 * h);
			cairo_line_to(cr, x, 0.7 * h);
		} else {
			cairo_move_to(cr, x, 0.4 * h);
			cairo_line_to(cr, x, 0.55 * h);
		}
		cairo_stroke(cr);
		x += SCALE_TENTH_MHZ_PIXELS;
		f += 1;
	}


	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_move_to(cr, w / 2, 0.25 * h);
	cairo_line_to(cr, w / 2, h);
	cairo_stroke(cr);
}

static void cfm_tuner_redraw(CFmTuner *tuner)
{
	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(tuner));
	GdkRegion *region = gdk_drawable_get_clip_region(GDK_DRAWABLE(window));
	gdk_window_invalidate_region(window, region, TRUE);
}

static void cfm_tuner_realize(GtkWidget *widget)
{
	GTK_WIDGET_CLASS(cfm_tuner_parent_class)->realize(widget);

	gtk_widget_add_events(widget,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
}


static gboolean cfm_tuner_button_press(GtkWidget *widget, GdkEventButton *event)
{
	CFmTuner *self = CFM_TUNER(widget);
	CFmTunerPrivate *priv = self->priv;

	priv->dragging = TRUE;
	priv->drag_start_x = event->x;

	return FALSE;
}

static gboolean cfm_tuner_button_release(GtkWidget *widget, GdkEventButton *event)
{
	CFmTuner *self = CFM_TUNER(widget);
	CFmTunerPrivate *priv = self->priv;

	if (priv->dragging) {
		priv->dragging = FALSE;
		priv->freq = lrint(priv->freq / 100000.0) * 100000; /* Round it. */
		cfm_tuner_redraw(self);
		g_signal_emit(G_OBJECT(self), signals[SIGNAL_FREQUENCY_TUNED], 0, NULL);
	}

	return FALSE;
}

static gboolean cfm_tuner_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	CFmTuner *self = CFM_TUNER(widget);
	CFmTunerPrivate *priv = self->priv;

	if (priv->dragging) {
		gdouble moved = (priv->drag_start_x - event->x) / SCALE_TENTH_MHZ_PIXELS;
		priv->freq += moved * 100000.0;
		if (priv->freq < priv->range_low) {
			priv->freq = priv->range_low;
		} else if (priv->freq > priv->range_high) {
			priv->freq = priv->range_high;
		}
		priv->drag_start_x = event->x;
		cfm_tuner_redraw(self);
		g_signal_emit(G_OBJECT(self), signals[SIGNAL_FREQUENCY_CHANGED], 0, NULL);
	}

	return FALSE;
}

static void cfm_tuner_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->width = 120;
	requisition->height = 120;
}

static gboolean cfm_tuner_expose(GtkWidget *widget, GdkEventExpose *event)
{
	CFmTuner *self = CFM_TUNER(widget);
	cairo_t *cr = gdk_cairo_create(widget->window);
	cairo_rectangle(cr, event->area.x, event->area.y, event->area.width,
		event->area.height);
	cairo_clip(cr);

	cfm_tuner_draw(self, cr);

	cairo_destroy(cr);
	return FALSE;
}

static void cfm_tuner_set_property(GObject *object, guint property_id,
	const GValue *value, GParamSpec *pspec)
{
	CFmTuner *self = CFM_TUNER(object);
	switch (property_id) {
	case PROP_FREQUENCY:
		self->priv->freq = g_value_get_ulong(value);
		cfm_tuner_redraw(self);
		break;
	case PROP_RANGE_LOW:
		self->priv->range_low = g_value_get_ulong(value);
		cfm_tuner_redraw(self);
		break;
	case PROP_RANGE_HIGH:
		self->priv->range_high = g_value_get_ulong(value);
		cfm_tuner_redraw(self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_tuner_get_property(GObject *object, guint property_id,
	GValue *value, GParamSpec *pspec)
{
	CFmTuner *self = CFM_TUNER(object);
	switch (property_id) {
	case PROP_FREQUENCY:
		g_value_set_ulong(value, self->priv->freq);
		break;
	case PROP_RANGE_LOW:
		g_value_set_ulong(value, self->priv->range_low);
		break;
	case PROP_RANGE_HIGH:
		g_value_set_ulong(value, self->priv->range_high);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_tuner_init(CFmTuner *self)
{
	CFmTunerPrivate *priv;

	self->priv = priv = CFM_TUNER_GET_PRIVATE(self);
}

static void cfm_tuner_dispose(GObject *object)
{
}

static void cfm_tuner_finalize(GObject *object)
{
}

static void cfm_tuner_class_init(CFmTunerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamSpec *param_spec;

	gobject_class->set_property = cfm_tuner_set_property;
	gobject_class->get_property = cfm_tuner_get_property;
	gobject_class->dispose = cfm_tuner_dispose;
	gobject_class->finalize = cfm_tuner_finalize;
	widget_class->realize = cfm_tuner_realize;
	widget_class->button_press_event = cfm_tuner_button_press;
	widget_class->button_release_event = cfm_tuner_button_release;
	widget_class->motion_notify_event = cfm_tuner_motion_notify;
	widget_class->size_request = cfm_tuner_size_request;
	widget_class->expose_event = cfm_tuner_expose;

	g_type_class_add_private (klass, sizeof(CFmTunerPrivate));

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
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_property(gobject_class, PROP_RANGE_LOW, param_spec);
	properties[PROP_RANGE_LOW] = param_spec;
	param_spec = g_param_spec_ulong("range-high",
	                                "Max frequency range (Hz)",
	                                "This is the highest frequency that can be tuned, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_HIGH,
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_RANGE_HIGH] = param_spec;
	g_object_class_install_property(gobject_class, PROP_RANGE_HIGH, param_spec);

	signals[SIGNAL_FREQUENCY_CHANGED] = g_signal_new("frequency-changed",
		G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, 
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	signals[SIGNAL_FREQUENCY_TUNED] = g_signal_new("frequency-tuned",
		G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, 
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

CFmTuner* cfm_tuner_new()
{
	return g_object_new(CFM_TYPE_TUNER, NULL);
}

