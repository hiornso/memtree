#include <stdlib.h>

#include "getapps.h"

#define DISABLE_GETAPPS_PRINTF 1

#if NDEBUG || DISABLE_GETAPPS_PRINTF
#define cprintf(...)
#else
#define cprintf(...) printf(__VA_ARGS__)
#endif

AppList get_apps()
{
	GList *apps = g_app_info_get_all();
	apps = g_list_first(apps); // ensure we are at the beginning
	size_t len = g_list_length(apps);
	AppList app_list = {
		len,
		malloc(sizeof(char*) * len),
		malloc(sizeof(char*) * len),
		malloc(sizeof(gchar*) * len)
	};
	
	if (app_list.app_names == NULL || app_list.app_executables == NULL || app_list.app_icons_as_string == NULL) {
		free(app_list.app_names);
		free(app_list.app_executables);
		g_free(app_list.app_icons_as_string);
		return (AppList){0, NULL, NULL, NULL};
	}
	
	GList *app = apps;
	for (size_t i = 0; app != NULL; app = app->next, ++i) {
		GAppInfo *appinfo = G_APP_INFO(app->data);
		
		if (g_app_info_should_show(appinfo)) {
			const char *app_name = g_app_info_get_display_name(appinfo);
			app_list.app_names[i] = app_name == NULL ? NULL : strdup(app_name);
			const char *executable_name = g_app_info_get_executable(appinfo);
			app_list.app_executables[i] = executable_name == NULL ? NULL : strdup(executable_name);
			GIcon *icon = g_app_info_get_icon(appinfo);
			if (icon != NULL) app_list.app_icons_as_string[i] = g_icon_to_string(icon);
			else app_list.app_icons_as_string[i] = NULL;
			
			cprintf("processed app {\n\tname = '%s'\n\texecutable = '%s'\n\tcommandline = '%s'\n\ticon_as_string = '%s' (from %p)\n}\n", app_list.app_names[i], app_list.app_executables[i], g_app_info_get_commandline(appinfo), app_list.app_icons_as_string[i], (void*)icon);
		} else {
			app_list.app_names[i] = app_list.app_executables[i] = app_list.app_icons_as_string[i] = NULL;
		}
		
		g_object_unref(appinfo);
	}
	
	g_list_free(apps);
	
	return app_list;
}

void free_app_list(AppList app_list)
{
	for (size_t i = 0; i < app_list.n; ++i) {
		free(app_list.app_names[i]);
		free(app_list.app_executables[i]);
		g_free(app_list.app_icons_as_string[i]);
	}
}
