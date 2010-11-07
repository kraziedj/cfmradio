#include <glib.h>

int main(int argc, char **argv) {
	GError *error = NULL;
	gchar *data;
	if (!g_file_get_contents("codetables.utf8", &data, NULL, &error)) {
		g_printerr("Failed to open %s: %s\n", error->message);
		return 1;
	}

	gint i;
	guint count = 0;

	for (i = 0; i < 0x20; i++) {
		g_print("\t%u,\t/* %u */\n", 0, count);
		count++;
	}

	gchar * c = data;
	while (*c) {
		if (*c != '\n') {
			gunichar u = g_utf8_get_char(c);
			g_print("\t%u,\t/* %u */\n", u, count);
			count++;
		}
		c = g_utf8_next_char(c);
	}

	g_printerr("Total elements = %d\n", count);

	return 0;
}
