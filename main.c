#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <assert.h>
#include <sys/time.h>

#include "main.h"
#include "table.h"
#include "tablesearch.h"
#include "tablesort.h"
#include "label.h"
#include "getapps.h"
#include "backend.h"
#include "caching.h"

#define GTK_GLIB_VERSIONS \
	gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(), \
	glib_major_version, glib_minor_version, glib_micro_version

#define PADDING {0}
#define VERSION_STRING "0.1.0"
#define MEMTREE_APPLICATION_ID "com.memtree"
#define ABOUT_WINDOW_SHOW_SYSTEM_INFORMATION TRUE
#define FORCE_APPLICATION_DESTROY_WINDOWS_ON_QUIT TRUE
#define SEARCH_CHANGED_STR "changed" // can be "changed" for instant updates or "search-changed" for updates after 150ms pause 
									// (waiting may feel more responsive if there is a very large number of processes and thus searching takes ages)

// build info
#if defined(__clang__)
#define COMPILER "Clang"
#elif defined(__INTEL_COMPILER)
#define COMPILER "Intel C Compiler"
#elif defined(__GNUC__) || defined(__GNUG__)
#define COMPILER "GCC"
#elif defined(_MSC_VER)
#define COMPILER "MSVC"
#else
#define COMPILER "Unknown Compiler"
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define PLATFORM "Windows"
#elif __APPLE__
#define PLATFORM "MacOS"
#elif __linux__
#define PLATFORM "Linux"
#elif __unix__
#define PLATFORM "Unix"
#else
#define PLATFORM "Unknown"
#endif

// be more native to MacOS
#if __APPLE__

#define PRIMARY_MASK GDK_META_MASK
#define PRIMARY_STRING "<Meta>"
#define SYSTEM_INFO_STRING "(MacOS Build)"

#else

#define PRIMARY_MASK GDK_CONTROL_MASK 
#define PRIMARY_STRING "<Control>"
#define SYSTEM_INFO_STRING "(Standard Build)"

#endif

static void not_implemented (
	__attribute__ ((unused)) GSimpleAction *action,
	__attribute__ ((unused)) GVariant *parameter,
	__attribute__ ((unused)) gpointer user_data
) {
	fprintf(stderr, "Not yet implemented.\n");
}

static void quit_activate (
	__attribute__ ((unused)) GSimpleAction *action,
	__attribute__ ((unused)) GVariant *parameter,
	gpointer user_data
) {
	g_application_quit(G_APPLICATION(user_data));
}

static void about_activate (
	__attribute__ ((unused)) GSimpleAction *action,
	__attribute__ ((unused)) GVariant *parameter,
	gpointer user_data
) {
	GApplication *app = G_APPLICATION(user_data);
	GtkAboutDialog *dialog = (GtkAboutDialog*)gtk_about_dialog_new();
	
	gtk_about_dialog_set_program_name(dialog, "Memtree");
	gtk_about_dialog_set_version(dialog, "v" VERSION_STRING " " SYSTEM_INFO_STRING);
	gtk_about_dialog_set_comments(dialog, "A Modern Memory Usage Monitor.");
	
#if ABOUT_WINDOW_SHOW_SYSTEM_INFORMATION
	const char format[] = "Memtree built using " COMPILER " (" __VERSION__ ")\nMemtree built at " __DATE__ " " __TIME__ "\nMemtree built for platform " PLATFORM "\nGTK %i.%i.%i\nGLib %i.%i.%i\n";
	int libvers_len = snprintf(NULL, 0, format,
		GTK_GLIB_VERSIONS
	);
	char *libvers = malloc(libvers_len + 1); // +1 for NULL termination
	if (libvers == NULL) {
		gtk_about_dialog_set_system_information(dialog, "Failed to allocate memory to format string to display library versions.\nTry reopening the dialog.");
	} else {
		snprintf(libvers, libvers_len + 1, format,
			GTK_GLIB_VERSIONS
		);
		gtk_about_dialog_set_system_information(dialog, libvers);
		free(libvers);
	}
	
#endif
	
	const char *authors[] = {"Oliver Hiorns", NULL};
	gtk_about_dialog_set_authors(dialog, authors);
	
	gtk_about_dialog_set_logo(dialog, (GdkPaintable*)gdk_texture_new_from_resource("/images/logo/logo.png")); // TODO: logo
	
	GtkWidget *w = gtk_window_get_titlebar(GTK_WINDOW(dialog));
	w = gtk_widget_get_first_child(w);
	w = gtk_widget_get_first_child(w);
	w = gtk_widget_get_first_child(w);
	w = gtk_widget_get_next_sibling(w);
	GtkWidget *x = gtk_widget_get_first_child(GTK_WIDGET(dialog));
	x = gtk_widget_get_first_child(x);
	gtk_window_set_titlebar(GTK_WINDOW(dialog), NULL);
	gtk_box_prepend(GTK_BOX(x), w);
	gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
	gtk_window_set_application(GTK_WINDOW(dialog), GTK_APPLICATION(app));
	gtk_widget_show(GTK_WIDGET(dialog));
}

static void refresh_gui(__attribute__((unused)) GtkWidget* widget, __attribute__((unused)) GdkFrameClock* frame_clock, gpointer user_data)
{	
	RefreshWidgets *widgets = (RefreshWidgets*)user_data;
	struct timeval t;
	gettimeofday(&t, NULL);
	if (t.tv_sec == widgets->last_refresh.tv_sec) return;
	widgets->last_refresh = t;
	
	GtkTreeStore *treestore = widgets->treestore;
	GtkLevelBar *mem_levelbar = widgets->mem_levelbar;
	GtkLevelBar *swap_levelbar = widgets->swap_levelbar;
	GtkLabel *mem_label = widgets->mem_label;
	GtkLabel *swap_label = widgets->swap_label;
	
	widgets->last_pinfos = refresh_table(treestore, widgets->last_pinfos);
	
	queue_cache_update();
	
	SystemMemInfo info = get_system_memory_info();
	
	gtk_level_bar_set_value(mem_levelbar, info.mem_total - info.mem_available);
	gtk_level_bar_set_value(swap_levelbar, info.swap_total - info.swap_free);
	
	update_usage_label(mem_label, info.mem_total - info.mem_available, info.mem_total, "System Memory Usage (%.2f %s / %.2f %s)", "System Memory Usage");
	update_usage_label(swap_label, info.swap_total - info.swap_free, info.swap_total, "System Swap Usage (%.2f %s / %.2f %s)", "System Swap Usage");
}

static void free_refresh_widgets_data(RefreshWidgets *widgets)
{
	free_parsed(widgets->last_pinfos);
	free(widgets);
}

static void activate_application(GApplication *app, __attribute__ ((unused)) gpointer user_data)
{
	// create builder from ui file - aborts upon error, but should be fine since it's statically compiled right in
	GtkBuilder *builder = gtk_builder_new_from_resource("/builder/memtree.ui");
	
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder,"memtree_window"));
	GtkSearchEntry *search_entry = GTK_SEARCH_ENTRY(gtk_builder_get_object(builder, "search-entry"));
	GtkTreeView *treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview"));
	GtkTreeStore *treestore = GTK_TREE_STORE(gtk_builder_get_object(builder, "programs_tree"));
	GtkLevelBar *mem_levelbar = GTK_LEVEL_BAR(gtk_builder_get_object(builder, "memory-levelbar"));
	GtkLevelBar *swap_levelbar = GTK_LEVEL_BAR(gtk_builder_get_object(builder, "swap-levelbar"));
	GtkLabel *mem_label = GTK_LABEL(gtk_builder_get_object(builder, "memory-usage-label"));
	GtkLabel *swap_label = GTK_LABEL(gtk_builder_get_object(builder, "swap-usage-label"));
	
	g_object_unref(builder);
	
	gtk_window_set_application(GTK_WINDOW(window), GTK_APPLICATION(app));
	gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), TRUE);
	
	set_sort_funcs(treestore);
	
	//AppList app_list = get_apps();
	//free_app_list(app_list);
	
	g_signal_connect(search_entry, SEARCH_CHANGED_STR, G_CALLBACK(handle_search_changed), treeview);
	
	SystemMemInfo info = get_system_memory_info();
	gtk_level_bar_set_max_value(mem_levelbar, info.mem_total);
	gtk_level_bar_set_max_value(swap_levelbar, info.swap_total);
	
	gtk_level_bar_add_offset_value(mem_levelbar, "low",      0.6 * info.mem_total);
	gtk_level_bar_add_offset_value(mem_levelbar, "low-mid",  0.8 * info.mem_total);
	gtk_level_bar_add_offset_value(mem_levelbar, "high-mid", 0.9 * info.mem_total);
	gtk_level_bar_add_offset_value(mem_levelbar, "high",     1.0 * info.mem_total);
	
	gtk_level_bar_add_offset_value(swap_levelbar, "low",      0.6 * info.swap_total);
	gtk_level_bar_add_offset_value(swap_levelbar, "low-mid",  0.8 * info.swap_total);
	gtk_level_bar_add_offset_value(swap_levelbar, "high-mid", 0.9 * info.swap_total);
	gtk_level_bar_add_offset_value(swap_levelbar, "high",     1.0 * info.swap_total);
	
	gtk_level_bar_set_value(mem_levelbar, info.mem_total - info.mem_available);
	gtk_level_bar_set_value(swap_levelbar, info.swap_total - info.swap_free);
	
	update_usage_label(mem_label, info.mem_total - info.mem_available, info.mem_total, "System Memory Usage (%.2f %s / %.2f %s)", "System Memory Usage");
	update_usage_label(swap_label, info.swap_total - info.swap_free, info.swap_total, "System Swap Usage (%.2f %s / %.2f %s)", "System Swap Usage");
	
	PInfo *pinfos = init_table(treestore);
	
	RefreshWidgets *widgets = malloc(sizeof(RefreshWidgets));
	if (widgets == NULL) g_critical("failed to allocate memory for RefreshWidgets struct");
	else {
		struct timeval t;
		gettimeofday(&t, NULL);
		
		*widgets = (RefreshWidgets){
			treestore, mem_levelbar, swap_levelbar, mem_label, swap_label, t, pinfos
		};
		
		gtk_widget_add_tick_callback(window, (GtkTickCallback)refresh_gui, widgets, (GDestroyNotify)free_refresh_widgets_data);
	}
	
	gtk_widget_show(window);
}

#if FORCE_APPLICATION_DESTROY_WINDOWS_ON_QUIT
static void window_destroy_gfunc(void *window, __attribute__ ((unused)) void *user_data)
{
	gtk_window_destroy(GTK_WINDOW(window));
}

static void force_destroy_windows(GApplication *app)
{
	GList *list = gtk_application_get_windows(GTK_APPLICATION(app));
	g_list_foreach(list, window_destroy_gfunc, NULL); 
	// about the above: could just pass (GFunc)gtk_window_destroy but this is better practice (calling-convention independent)...
	// don't mention all the other places this program assumes it's fine to drop excess arguments
}
#endif

static void initialise_application(GApplication *app, __attribute__ ((unused)) gpointer user_data)
{
#if FORCE_APPLICATION_DESTROY_WINDOWS_ON_QUIT
	g_signal_connect(app, "shutdown", G_CALLBACK(force_destroy_windows), NULL);
	g_signal_connect(app, "shutdown", G_CALLBACK(deinit_cache), NULL);
#endif
	
	// menu action-callback linking stuff, blatantly stolen from an example gtk4 program.
	const GActionEntry app_entries[] = {
		{ "preferences",       not_implemented,      NULL, NULL, NULL, PADDING },
		{ "quit",              quit_activate,        NULL, NULL, NULL, PADDING },
		
		{ "about",             about_activate,       NULL, NULL, NULL, PADDING },
		{ "help",              not_implemented,      NULL, NULL, NULL, PADDING }
	};
	
	g_action_map_add_action_entries(G_ACTION_MAP (app),
									app_entries, G_N_ELEMENTS (app_entries),
									app);
	
	char *accels[2] = {NULL, NULL};
	
	accels[0] = PRIMARY_STRING "comma";
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.preferences", (const char * const *)accels);
	accels[0] = PRIMARY_STRING "q";
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", (const char * const *)accels);
	
	accels[0] = "F1";
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.help", (const char * const *)accels);
	accels[0] = "F7";
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.about", (const char * const *)accels);
	
	// this could be done automatically by placing the menu resource at /com/memtree/gtk/menus.ui but this way seems clearer
	GtkBuilder *builder = gtk_builder_new_from_resource("/builder/menu.ui");
	GMenuModel *model = G_MENU_MODEL(gtk_builder_get_object(builder, "menubar"));
	
	gtk_application_set_menubar(GTK_APPLICATION(app), model);
	g_object_unref(builder);
	
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(provider, "/css/systemlevelbar.css");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static void line_wrap(char *str)
{
#define LINE_LEN 150
	for (int i = 0, last = 0; str[i] != '\0'; ++i) {
		if (i - last > LINE_LEN && str[i] == ' ') {
			str[i] = '\n';
			last = i;
		} else if (str[i] == '\n') {
			last = i;
		}
	}
}

static int handle_local_cmdline_args(__attribute__ ((unused)) GApplication *application, __attribute__ ((unused)) GVariantDict *dict, gpointer user_data)
{
	CmdLineOptions *options = (CmdLineOptions*)user_data;
	
	if (options->print_version) {
		fprintf(stdout,
			"Memtree " VERSION_STRING " " SYSTEM_INFO_STRING "\nMemtree built using " COMPILER " (" __VERSION__ ") at " __DATE__ " " __TIME__ " for platform " PLATFORM "\nGTK %i.%i.%i\nGLib %i.%i.%i\n",
			GTK_GLIB_VERSIONS
		);
		
		return 0;
	}
	
	return -1;
}

int main(int argc, char *argv[])
{
	if (init_backend() != 0) return -1;
	
	GtkApplication *app = gtk_application_new(MEMTREE_APPLICATION_ID, G_APPLICATION_DEFAULT_FLAGS);
	
	CmdLineOptions cmd_line_options = (CmdLineOptions){FALSE};
	const GOptionEntry options[] = {
		{
			"version",
			'v',
			G_OPTION_FLAG_NONE,
			
			G_OPTION_ARG_NONE,
			&cmd_line_options.print_version,
			
			"Print the version and exit.",
			NULL
		},
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};
	
	g_application_add_main_option_entries(G_APPLICATION(app), options);
	g_application_set_option_context_summary(G_APPLICATION(app), "A Modern Memory Usage Monitor.");
	char memtree_description[] =
		"Memtree is a memory usage monitor which provides a more graphically orientated "
		"view than other system monitors. Specifically, it provides recursive memory usage "
		"and associates processes with graphical programs via desktop files, to give a more "
		"useful picture of what programs are using memory, even if their usage is spread "
		"over many subprocesses.\n"
		"\n"
		"To report any bugs, make a feature request or contribute to the project, see "
		"https://github.com/hiornso/memtree";
	line_wrap(memtree_description);
	g_application_set_option_context_description(G_APPLICATION(app), memtree_description);
	
	g_signal_connect(app, "startup", G_CALLBACK(initialise_application), NULL);
	g_signal_connect(app, "activate", G_CALLBACK(activate_application), NULL);
	g_signal_connect(app, "handle-local-options", G_CALLBACK(handle_local_cmdline_args), &cmd_line_options);
	
	int status = g_application_run(G_APPLICATION (app), argc, argv);
	
	g_object_unref(app);
	
	return status;
}
