#include "rl.h"

void image_viewer()
{
	int i;
	static int init=1;
	static char *path=NULL, dir_path[PATH_MAX*4]={0}, **filepath=NULL;
	static int file_index=0, filecount=0;
	static mipmap_t image_mm={0};
	static double gain=NAN;

	// GUI layout (so far just one knob under the image)
	static gui_layout_t layout={0};
	const char *layout_src[] = {
		"elem 0", "type none", "label Options", "pos	0	0", "dim	3	3;6", "off	0	1", "",
		"elem 10", "type knob", "label Gain", "knob 0.01 1 1000 log", "pos	0;6	-3", "dim	2", "off	0	0", "",
	};

	gui_layout_init_pos_scale(&layout, xy(zc.limit_u.x+0.25, 8.), 0.7, XY0, 0);
	make_gui_layout(&layout, layout_src, sizeof(layout_src)/sizeof(char *), "Image viewer");

	// Getting an image path from the command line
	if (init)
	{
		// Open argv[1] if present
		int argc=0;
		char **argv = get_argv(&argc);
		if (argc >= 2)
			path = make_string_copy(argv[1]);
		free_2d(argv, argc);

		init = 0;
	}

	// Getting an image path from drag and drop
	if (dropfile_get_count())
		path = dropfile_pop_first();

	// Load the next or previous image (not threaded)
	int way = (mouse.key_state[RL_SCANCODE_RIGHT]>=2) - (mouse.key_state[RL_SCANCODE_LEFT]>=2);	// the left and right arrow keys change images

	if (way==0 && mouse.b.wheel && mouse.zoom_flag==0)						// the scroll wheel when not in zoom mode does that too
		way = -mouse.b.wheel / abs(mouse.b.wheel);

	if (way && path==NULL)
	{
		for (i=file_index+way; i < filecount && i >= 0; i+=way)
			if (is_path_image_file(filepath[i]))			// switch to the first image found
			{
				path = make_string_copy(filepath[i]);
				break;
			}
	}

	// Load image at path
	if (path)
	{
		free_2d(filepath, filecount);

		// List the files that the file at path is in
		fs_dir_t dir={0};

		remove_name_from_path(dir_path, path);
		load_dir_depth(dir_path, &dir, 0);

		file_index = 0;
		filecount = dir.subfile_count;
		filepath = calloc(filecount, sizeof(char *));
		// Go through each file of the folder and create the full path
		for (i=0; i < dir.subfile_count; i++)
		{
			filepath[i] = append_name_to_path(NULL, dir.path, dir.subfile[i].name);

			if (strcmp(filepath[i], path)==0)
				file_index = i;
		}

		free_dir(&dir);

		// Load the image as a tiled mipmap
		free_mipmap(&image_mm);
		image_mm = load_mipmap(path, IMAGE_USE_SQRGB);
	}

	free_null(&path);

	// Draw image
	drawq_bracket_open();
	blit_mipmap_in_rect(image_mm, sc_rect(make_rect_off(XY0, mul_xy(zc.limit_u, set_xy(2.)), xy(0.5, 0.5))), 1, LINEAR_INTERP);	// the mipmap image is fitted inside a rectangle that represents the default view
	draw_gain_parabolic(gain);		// the brackets make the parabolic gain effect only be applied to the mipmap
	drawq_bracket_close(DQB_ADD);

	// GUI window
	static flwindow_t window={0};
	flwindow_init_defaults(&window);
	draw_dialog_window_fromlayout(&window, NULL, &layout, 0);	// this handles and displays the window that contains the control

	// GUI controls
	ctrl_knob_fromlayout(&gain, &layout, 10);			// this both displays the control and updates the gain value
}

void main_loop()
{
	static int exit_flag=0, init=1;
	SDL_Event event;
	static uint32_t td=0;

	if (init)
	{
		init = 0;

		#ifdef RL_OPENCL
		fb.use_drawq = 1;
		#else
		fb.use_drawq = 0;
		#endif
		fb.r.use_frgb = fb.use_drawq;
		sdl_graphics_init_autosize("Mini rouziclib Picture Viewer", SDL_WINDOW_RESIZABLE, 0);
		SDL_MaximizeWindow(fb.window);

		zc = init_zoom(&mouse, drawing_thickness);
		calc_screen_limits(&zc);
		mouse = init_mouse();

		font = remake_font("vector_type/type_index.txt", font);		// when the type_index file is not found it just defaults to the builtin fallback simplified font

		#ifdef __EMSCRIPTEN__
		SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#document");
		#endif
	}

	#ifdef __EMSCRIPTEN__
	#else
	while (exit_flag==0)
	#endif
	{
		//********Input handling********

		mouse_pre_event_proc(&mouse);
		keyboard_pre_event_proc(&mouse);
		sdl_handle_window_resize(&zc);

		while (SDL_PollEvent(&event))
		{
			dropfile_event_proc(event);
			sdl_mouse_event_proc(&mouse, event, &zc);
			sdl_keyboard_event_proc(&mouse, event);

			if (event.type==SDL_QUIT)
				exit_flag = 1;
		}

		if (mouse.key_state[RL_SCANCODE_RETURN] == 2 && get_kb_alt())
			sdl_toggle_borderless_fullscreen();

		textedit_add(cur_textedit, NULL);	// processes the new keystrokes in the current text editor

		mouse_post_event_proc(&mouse, &zc);

		//-------------input-----------------

		image_viewer();

		mousecursor_logic_and_draw();

		sdl_flip_fb();
	}
}

int main(int argc, char *argv[])
{
	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(main_loop, 0, 1);
	#else
	main_loop();
	#endif

	SDL_DestroyRenderer(fb.renderer);
	SDL_DestroyWindow(fb.window);
	SDL_Quit();

	return 0;
}
