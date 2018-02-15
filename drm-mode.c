#include <math.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define MARGIN_PERCENT 1.8 /* % of active vertical image*/
#define CELL_GRAN 8.0 /* assumed character cell granularity*/
#define MIN_PORCH 1 /* minimum front porch */
#define V_SYNC_RQD 3 /* width of vsync in lines */
#define H_SYNC_PERCENT 8.0 /* width of hsync as % of total line */
#define MIN_VSYNC_PLUS_BP 550.0 /* min time of vsync + back porch (microsec) */
#define M 600.0 /* blanking formula gradient */
#define C 40.0 /* blanking formula offset */
#define K 128.0 /* blanking formula scaling factor */
#define J 20.0 /* blanking formula scaling factor */
/* C' and M' are part of the Blanking Duty Cycle computation */
#define C_PRIME (((C - J) * K / 256.0) + J)
#define M_PRIME (K / 256.0 * M)

drmModeModeInfoPtr drm_generate_mode(int h_pixels, int v_lines, float freq) {
	float h_pixels_rnd;
	float v_lines_rnd;
	float v_field_rate_rqd;
	float top_margin;
	float bottom_margin;
	float interlace;
	float h_period_est;
	float vsync_plus_bp;
	float v_back_porch;
	float total_v_lines;
	float v_field_rate_est;
	float h_period;
	float v_field_rate;
	float v_frame_rate;
	float left_margin;
	float right_margin;
	float total_active_pixels;
	float ideal_duty_cycle;
	float h_blank;
	float total_pixels;
	float pixel_freq;
	float h_freq;
	float h_sync;
	float h_front_porch;
	float v_odd_front_porch_lines;
	int interlaced = 0;
	int margins = 0;
	drmModeModeInfoPtr m = malloc(sizeof(drmModeModeInfo));
	h_pixels_rnd = rint((float) h_pixels / CELL_GRAN) * CELL_GRAN;
	v_lines_rnd = interlaced ? rint((float) v_lines) / 2.0 : rint((float) v_lines);
	v_field_rate_rqd = interlaced ? (freq * 2.0) : (freq);
	top_margin = margins ? rint(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);
	bottom_margin = margins ? rint(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);
	interlace = interlaced ? 0.5 : 0.0;
	h_period_est = (((1.0 / v_field_rate_rqd) - (MIN_VSYNC_PLUS_BP / 1000000.0)) / (v_lines_rnd + (2 * top_margin) + MIN_PORCH + interlace) * 1000000.0);
	vsync_plus_bp = rint(MIN_VSYNC_PLUS_BP / h_period_est);
	v_back_porch = vsync_plus_bp - V_SYNC_RQD;
	total_v_lines = v_lines_rnd + top_margin + bottom_margin + vsync_plus_bp + interlace + MIN_PORCH;
	v_field_rate_est = 1.0 / h_period_est / total_v_lines * 1000000.0;
	h_period = h_period_est / (v_field_rate_rqd / v_field_rate_est);
	v_field_rate = 1.0 / h_period / total_v_lines * 1000000.0;
	v_frame_rate = interlaced ? v_field_rate / 2.0 : v_field_rate;
	left_margin = margins ? rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN : 0.0;
	right_margin = margins ? rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN : 0.0;
	total_active_pixels = h_pixels_rnd + left_margin + right_margin;
	ideal_duty_cycle = C_PRIME - (M_PRIME * h_period / 1000.0);
	h_blank = rint(total_active_pixels * ideal_duty_cycle / (100.0 - ideal_duty_cycle) / (2.0 * CELL_GRAN)) * (2.0 * CELL_GRAN);
	total_pixels = total_active_pixels + h_blank;
	pixel_freq = total_pixels / h_period;
	h_freq = 1000.0 / h_period;
	h_sync = rint(H_SYNC_PERCENT / 100.0 * total_pixels / CELL_GRAN) * CELL_GRAN;
	h_front_porch = (h_blank / 2.0) - h_sync;
	v_odd_front_porch_lines = MIN_PORCH + interlace;
	m->clock = ceil(pixel_freq) * 1000;
	m->hdisplay = (int) (h_pixels_rnd);
	m->hsync_start = (int) (h_pixels_rnd + h_front_porch);
	m->hsync_end = (int) (h_pixels_rnd + h_front_porch + h_sync);
	m->htotal = (int) (total_pixels);
	m->hskew = 0;
	m->vdisplay = (int) (v_lines_rnd);
	m->vsync_start = (int) (v_lines_rnd + v_odd_front_porch_lines);
	m->vsync_end = (int) (int) (v_lines_rnd + v_odd_front_porch_lines + V_SYNC_RQD);
	m->vtotal = (int) (total_v_lines);
	m->vscan = 0;
	m->vrefresh = freq;
	m->flags = 10;
	m->type = 64;
	char* name = "(generated)";
	strncpy(m->name, name, strlen(name));
	return (m);
}
