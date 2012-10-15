/*
 * mudraw -- command line tool for drawing pdf/xps/cbz documents
 */

#include <fitz.h>

#include "lib/window.h"
#include "lib/graphics.h"
#include "lib/decorations.h"

gfx_context_t * gfx_ctx;
window_t * window;

void compress() {
	/* Workaround link failure due to zlib oddness */
}

int compressBound(int sourceLen) {
	/* Similarly */
	return sourceLen;
}

int getrusage(int who, void *usage) {
	/* We don't support getrusage, required by a dependency */
	return -1;
}

enum { TEXT_PLAIN = 1, TEXT_HTML = 2, TEXT_XML = 3 };

static char *output = NULL;
static float resolution = 72;
static int res_specified = 0;
static float rotation = 0;

static int windowed = 0;
static int width = 0;
static int height = 0;
static int fit = 0;
static int errored = 0;
static int ignore_errors = 0;
static int alphabits = 8;

static fz_text_sheet *sheet = NULL;
static fz_colorspace *colorspace;
static char *filename;
static int files = 0;

void draw_decors(int page, int epage) {
	char title[512] = {0};
	sprintf(title, "PDF Viewer - Page %d of %d", page, epage);
	render_decorations(window, window->buffer, title);
}

static void inplace_reorder(char * samples, int size) {
	for (int i = 0; i < size; ++i) {
		uint32_t c = ((uint32_t *)samples)[i];
		char b = _RED(c);
		char g = _GRE(c);
		char r = _BLU(c);
		char a = _ALP(c);
		((uint32_t *)samples)[i] = rgba(r,g,b,a);
	}
}

static int isrange(char *s)
{
	while (*s)
	{
		if ((*s < '0' || *s > '9') && *s != '-' && *s != ',')
			return 0;
		s++;
	}
	return 1;
}

static void drawpage(fz_context *ctx, fz_document *doc, int pagenum)
{
	fz_page *page;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	int start;
	fz_cookie cookie = { 0 };

	fz_var(list);
	fz_var(dev);

	fz_try(ctx)
	{
		page = fz_load_page(doc, pagenum - 1);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load page %d in file '%s'", pagenum, filename);
	}

	float zoom;
	fz_matrix ctm;
	fz_rect bounds, bounds2;
	fz_bbox bbox;
	fz_pixmap *pix = NULL;
	int w, h;

	fz_var(pix);

	bounds = fz_bound_page(doc, page);
	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);
	ctm = fz_concat(ctm, fz_rotate(rotation));
	bounds2 = fz_transform_rect(ctm, bounds);
	bbox = fz_round_rect(bounds2);
	/* Make local copies of our width/height */
	w = width;
	h = height;
	/* If a resolution is specified, check to see whether w/h are
	 * exceeded; if not, unset them. */
	if (res_specified)
	{
		int t;
		t = bbox.x1 - bbox.x0;
		if (w && t <= w)
			w = 0;
		t = bbox.y1 - bbox.y0;
		if (h && t <= h)
			h = 0;
	}
	/* Now w or h will be 0 unless then need to be enforced. */
	if (w || h)
	{
		float scalex = w/(bounds2.x1-bounds2.x0);
		float scaley = h/(bounds2.y1-bounds2.y0);

		if (fit)
		{
			if (w == 0)
				scalex = 1.0f;
			if (h == 0)
				scaley = 1.0f;
		}
		else
		{
			if (w == 0)
				scalex = scaley;
			if (h == 0)
				scaley = scalex;
		}
		if (!fit)
		{
			if (scalex > scaley)
				scalex = scaley;
			else
				scaley = scalex;
		}
		ctm = fz_concat(ctm, fz_scale(scalex, scaley));
		bounds2 = fz_transform_rect(ctm, bounds);
	}
	bbox = fz_round_rect(bounds2);

	/* TODO: banded rendering and multi-page ppm */

	fz_try(ctx)
	{
		pix = fz_new_pixmap_with_bbox(ctx, colorspace, bbox);

		fz_clear_pixmap_with_value(ctx, pix, 255);

		dev = fz_new_draw_device(ctx, pix);
		if (list)
			fz_run_display_list(list, dev, ctm, bbox, &cookie);
		else
			fz_run_page(doc, page, dev, ctm, &cookie);
		fz_free_device(dev);
		dev = NULL;

		int size = fz_pixmap_height(ctx, pix) * fz_pixmap_width(ctx, pix);
		inplace_reorder(fz_pixmap_samples(ctx, pix), size);
		if (fz_pixmap_width(ctx, pix) != gfx_ctx->width) {
			size_t x_offset = (width - fz_pixmap_width(ctx, pix)) / 2;
			size_t y_offset = (height - fz_pixmap_height(ctx, pix)) / 2;;
			if (window) {
				x_offset += decor_left_width;
				y_offset += decor_top_height;
			}
			for (int i = 0; i < fz_pixmap_height(ctx, pix); ++i) {
				memcpy(&GFX(gfx_ctx, x_offset, y_offset + i), &fz_pixmap_samples(ctx, pix)[fz_pixmap_width(ctx, pix) * i * 4], fz_pixmap_width(ctx, pix) * 4);
			}
		} else {
			memcpy(gfx_ctx->backbuffer, fz_pixmap_samples(ctx, pix), size * 4);
		}

	}
	fz_always(ctx)
	{
		fz_free_device(dev);
		dev = NULL;
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		fz_free_display_list(ctx, list);
		fz_free_page(doc, page);
		fz_rethrow(ctx);
	}

	if (list)
		fz_free_display_list(ctx, list);

	fz_free_page(doc, page);

	fz_flush_warnings(ctx);

	if (cookie.errors)
		errored = 1;
}

int current_page  = 0;
int current_epage = 0;
fz_document * current_doc = NULL;
fz_context  * current_ctx = NULL;

static void drawrange(fz_context *ctx, fz_document *doc, char *range) {
	int page, spage, epage, pagecount;
	char *spec, *dash;

	pagecount = fz_count_pages(doc);
	spec = fz_strsep(&range, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = pagecount;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = pagecount;
		}

		spage = fz_clampi(spage, 1, pagecount);
		epage = fz_clampi(epage, 1, pagecount);

		current_doc = doc;
		current_ctx = ctx;
		current_epage = epage;

		for (page = spage; page <= epage; ) {
			if (page == 0) page = 1;

			current_page = page;

			if (window) {
				drawpage(ctx, doc, page);
				draw_decors(page, epage);

				char c;
				w_keyboard_t * kbd;
				while (1) {
					kbd = poll_keyboard();
					if (kbd != NULL) {
						c = kbd->key;
						free(kbd);
						if (c == 'a') {
							page--;
							break;
						} else if (c == 's') {
							page++;
							if (page > epage) page = epage;
							break;
						} else if (c == 'q') {
							teardown_windowing();
							exit(0);
						}
					}
				}

			} else {
				printf("\033[H\033[1560z");
				fflush(stdout);
				drawpage(ctx, doc, page);
				printf("[Page %d of %d]", page, epage);

				fflush(stdout);
				while (1) {
					char c = fgetc(stdin);
					if (c == 'a') {
						page--;
						break;
					} else if (c == 's') {
						page++;
						if (page > epage) page = epage;
						break;
					} else if (c == 'q') {
						printf("\n");
						exit(0);
					}
				}
			}
		}

		spec = fz_strsep(&range, ",");
	}
}

void resize_callback(window_t * win) {
	width  = win->width  - decor_left_width - decor_right_width;
	height = win->height - decor_top_height - decor_bottom_height;

	reinit_graphics_window(gfx_ctx, window);
	draw_fill(gfx_ctx, rgb(0,0,0));

	if (current_doc) {
		/* redraw the page */
		draw_decors(current_page, current_epage);
		drawpage(current_ctx, current_doc, current_page);
	}
}

int main(int argc, char **argv) {
	fz_document *doc = NULL;
	int c;
	fz_context *ctx;

	fz_var(doc);

	char * _windowed = getenv("DISPLAY");
	if (_windowed) {
		char * _width  = getenv("WIDTH");
		char * _height = getenv("HEIGHT");
		if (_width)  { width  = atoi(_width);  } else { width = 512; }
		if (_height) { height = atoi(_height); } else { height = 512; }

		setup_windowing();
		resize_window_callback = resize_callback;

		init_decorations();
		window = window_create(50,50, width + decor_left_width + decor_right_width, height + decor_top_height + decor_bottom_height);
		gfx_ctx = init_graphics_window(window);
		draw_fill(gfx_ctx,rgb(0,0,0));
	} else {
		gfx_ctx = init_graphics_fullscreen();
		width = gfx_ctx->width;
		height = gfx_ctx->height;
	}

	while ((c = fz_getopt(argc, argv, "wf")) != -1) {
		switch (c) {
			case 'w':
				windowed = 1;
				break;
			case 'f':
				fit = 1;
				break;
		}
	}

	output = "yes";

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		fprintf(stderr, "Could not initialize fitz context.\n");
		exit(1);
	}

	fz_set_aa_level(ctx, alphabits);
	colorspace = fz_device_rgb;

	fz_try(ctx) {
		while (fz_optind < argc)
		{
			fz_try(ctx)
			{
				filename = argv[fz_optind++];
				files++;

				fz_try(ctx)
				{
					doc = fz_open_document(ctx, filename);
				}
				fz_catch(ctx)
				{
					fz_throw(ctx, "cannot open document: %s", filename);
				}

				if (fz_optind == argc || !isrange(argv[fz_optind]))
					drawrange(ctx, doc, "1-");
				if (fz_optind < argc && isrange(argv[fz_optind]))
					drawrange(ctx, doc, argv[fz_optind++]);

				fz_close_document(doc);
				doc = NULL;
			}
			fz_catch(ctx)
			{
				if (!ignore_errors)
					fz_rethrow(ctx);

				fz_close_document(doc);
				doc = NULL;
				fz_warn(ctx, "ignoring error in '%s'", filename);
			}
		}
	}
	fz_catch(ctx) {
		fz_close_document(doc);
		fprintf(stderr, "error: cannot draw '%s'\n", filename);
		errored = 1;
	}

	fz_free_context(ctx);

	if (window) {
		teardown_windowing();
	}

	return (errored != 0);
}
