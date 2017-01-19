/*
 * mudraw -- command line tool for drawing pdf/xps/cbz documents
 */

#include <fitz.h>

#include "lib/graphics.h"

static float resolution = 72;
static int res_specified = 0;
static float rotation = 0;
static int fit = 0;
static int errored = 0;
static int ignore_errors = 0;
static int alphabits = 8;

static fz_colorspace *colorspace;

int getrusage(int who, void *usage) {
	/* We don't support getrusage, required by a dependency */
	return -1;
}


void draw_page(fz_context *ctx, fz_document *doc, int pagenum, gfx_context_t * gfx_ctx)
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
		fz_throw(ctx, "cannot load page %d", pagenum);
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
	w = gfx_ctx->width;
	h = gfx_ctx->height;
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

		size_t x_offset = (gfx_ctx->width - fz_pixmap_width(ctx, pix)) / 2;
		size_t y_offset = (gfx_ctx->height - fz_pixmap_height(ctx, pix)) / 2;;
		for (int i = 0; i < fz_pixmap_height(ctx, pix); ++i) {
			memcpy(&GFX(gfx_ctx, x_offset, y_offset + i), &fz_pixmap_samples(ctx, pix)[fz_pixmap_width(ctx, pix) * i * 4], fz_pixmap_width(ctx, pix) * 4);
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

void * init_fitz(void) {
	colorspace = fz_device_bgr;
	return fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
}

void * load_document(fz_context * ctx, char * path) {
	fz_try(ctx) {
		return fz_open_document(ctx, path);
	}
	fz_catch(ctx) {
		return NULL;
	}
}

void set_fit(int _fit) {
	fit = _fit;
}

int page_count(fz_document * doc) {
	return fz_count_pages(doc);
}
