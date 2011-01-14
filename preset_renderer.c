/*
 * GPL 2
 */

#include <gtk/gtk.h>

#include "preset_renderer.h"

G_DEFINE_TYPE (CFmPresetRenderer, cfm_preset_renderer, GTK_TYPE_CELL_RENDERER);

#define ABS_RANGE_LOW 60000000
#define ABS_RANGE_HIGH 140000000

enum {
	PROP_0,
	PROP_FREQUENCY,
	PROP_NAME,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void cfm_preset_renderer_get_size_internal(GtkCellRenderer *cell,
	GtkWidget *widget, GdkRectangle *cell_area, PangoLayout *layout,
	gint *x_offset, gint *y_offset, gint *width, gint *height)
{
	CFmPresetRenderer *self = CFM_PRESET_RENDERER(cell);
	PangoRectangle rect;
	gint min_width, min_height;

	if (layout) {
		g_object_ref(layout);
	} else {
		layout = gtk_widget_create_pango_layout(widget, NULL);
	}

	pango_layout_set_text(layout, self->freq_text, -1);
	pango_layout_get_pixel_extents(layout, NULL, &rect);
	min_height = cell->ypad * 2 + rect.height;
	min_width = cell->xpad * 2 + rect.width;

	if (self->name && self->name[0] != '\0') {
		pango_layout_set_text(layout, self->name, -1);
		pango_layout_get_pixel_extents(layout, NULL, &rect);
		min_height = MAX(min_height, cell->ypad * 2 + rect.height);
		min_width += rect.width;
	}

	if (cell_area) {
		if (x_offset) *x_offset = 0; /* This renderer always expands horiz. */
		if (y_offset) {
			*y_offset = cell->yalign * (cell_area->height - min_height);
		}
	} else {
		if (x_offset) *x_offset = 0;
		if (y_offset) *y_offset = 0;
	}

	if (width) *width = min_width;
	if (height) *height = min_height;

	g_object_unref(layout);
}

static void cfm_preset_renderer_get_size(GtkCellRenderer *cell,
	GtkWidget *widget, GdkRectangle *cell_area, gint *x_offset, gint *y_offset,
	gint *width, gint *height)
{
	PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);

	cfm_preset_renderer_get_size_internal(cell, widget, cell_area, layout,
		x_offset, y_offset, width, height);

	g_object_unref(layout);
}

static void cfm_preset_renderer_render(GtkCellRenderer *cell, GdkWindow *window,
	GtkWidget *widget, GdkRectangle *background_area, GdkRectangle *cell_area,
	GdkRectangle *expose_area, GtkCellRendererState flags)
{
	CFmPresetRenderer *self = CFM_PRESET_RENDERER(cell);
	PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
	gint avail_width = cell_area->width - (2*cell->xpad);
	gint x_offset, y_offset, x, y;

	cfm_preset_renderer_get_size_internal(cell, widget, cell_area, layout,
		&x_offset, &y_offset, NULL, NULL);

	x = cell_area->x + x_offset + cell->xpad;
	y = cell_area->y + y_offset + cell->ypad;

	pango_layout_set_text(layout, self->freq_text, -1);
	gtk_paint_layout(widget->style, window, GTK_STATE_NORMAL, TRUE, expose_area,
		widget, "cellrenderertext", x, y, layout);

	if (self->name && self->name[0] != '\0') {
		pango_layout_set_text(layout, self->name, -1);
		x += avail_width / 2;
		gtk_paint_layout(widget->style, window, GTK_STATE_NORMAL, TRUE,
			expose_area, widget, "cellrenderertext", x, y, layout);
	}

	g_object_unref(layout);
}

static void cfm_preset_renderer_init(CFmPresetRenderer *self)
{

}

static void cfm_preset_renderer_set_property(GObject *object, guint property_id,
	const GValue *value, GParamSpec *pspec)
{
	CFmPresetRenderer *self = CFM_PRESET_RENDERER(object);
	switch (property_id) {
	case PROP_NAME:
		g_free(self->name);
		self->name = g_value_dup_string(value);
		break;
	case PROP_FREQUENCY:
		g_free(self->freq_text);
		self->frequency = g_value_get_ulong(value);
		self->freq_text = g_strdup_printf("%.1f MHz",
		                                  self->frequency / 1000000.0f);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_preset_renderer_get_property(GObject *object, guint property_id,
	GValue *value, GParamSpec *pspec)
{
	CFmPresetRenderer *self = CFM_PRESET_RENDERER(object);
	switch (property_id) {
	case PROP_NAME:
		g_value_set_string(value, self->name);
		break;
	case PROP_FREQUENCY:
		g_value_set_ulong(value, self->frequency);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void cfm_preset_renderer_finalize(GObject *object)
{
	CFmPresetRenderer *self = CFM_PRESET_RENDERER(object);
	g_free(self->name);
	g_free(self->freq_text);

	G_OBJECT_CLASS(cfm_preset_renderer_parent_class)->finalize(object);
}

static void cfm_preset_renderer_class_init (CFmPresetRendererClass *klass)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
	GtkCellRendererClass* parent_class = GTK_CELL_RENDERER_CLASS (klass);
	GParamSpec *param_spec;

	parent_class->get_size = cfm_preset_renderer_get_size;
	parent_class->render = cfm_preset_renderer_render;
	gobject_class->set_property = cfm_preset_renderer_set_property;
	gobject_class->get_property = cfm_preset_renderer_get_property;
	gobject_class->finalize = cfm_preset_renderer_finalize;

	param_spec = g_param_spec_ulong("frequency",
	                                "Frequency",
	                                "This is the frequency of this preset, in Hz",
	                                ABS_RANGE_LOW, ABS_RANGE_HIGH, ABS_RANGE_LOW,
	                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_FREQUENCY] = param_spec;
	g_object_class_install_property(gobject_class, PROP_FREQUENCY, param_spec);
	param_spec = g_param_spec_string("name",
	                                 "Name",
	                                 "Name for this preset",
	                                 "",
	                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_NAME] = param_spec;
	g_object_class_install_property(gobject_class, PROP_NAME, param_spec);
}

GtkCellRenderer* cfm_preset_renderer_new()
{
	return g_object_new(CFM_TYPE_PRESET_RENDERER, NULL);
}

