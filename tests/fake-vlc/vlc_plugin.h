#ifndef TEST_VLC_PLUGIN_H
#define TEST_VLC_PLUGIN_H
#define vlc_module_begin() static void fake_module_definition(void) {
#define set_shortname(...) ((void)0);
#define set_description(...) ((void)0);
#define set_category(...) ((void)0);
#define set_subcategory(...) ((void)0);
#define set_capability(...) ((void)0);
#define set_callbacks(...) ((void)0);
#define add_shortcut(...) ((void)0);
#define add_float(...) ((void)0);
#define change_float_range(...) ((void)0);
#define vlc_module_end() }
#endif
