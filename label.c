#include <gtk/gtk.h>
#include <stddef.h>

#include "label.h"

void update_usage_label(GtkLabel *label, double used, double total, const char *try_str, const char *fallback_str)
{
	char *text;
	size_t len;
	int units = 1; // /proc reports values in KiB
	
	while (total >= 1024.0 && units <= 4) {
		used /= 1024.0;
		total /= 1024.0;
		++units;
	}
	
	const char *unitstr = (units == 0 ? "B" :
							units == 1 ? "KiB" :
							units == 2 ? "MiB" :
							units == 3 ? "GiB" :
							units == 4 ? "TiB" :
							"???");
	
	len = snprintf(NULL, 0, try_str, used, unitstr, total, unitstr) + 1; // +1 for null char
	text = malloc(len);
	if (text != NULL) {
		snprintf(text, len, try_str, used, unitstr, total, unitstr);
		gtk_label_set_label(label, text);
		free(text);
	} else {
		gtk_label_set_label(label, fallback_str);
		g_warning("failed to allocate memory to format '%s' string", fallback_str);
	}
}
