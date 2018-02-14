#include "embedder.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>

#include "common.h"
#include "drm-common.h"

static const struct egl *egl;
static const struct gbm *gbm;
static const struct drm *drm;
static FlutterEngine engine = NULL;

struct timeval ps, pe;
static void perf_enter()
{
	gettimeofday(&ps, NULL);
}

static void perf_exit(const char* name)
{
	gettimeofday(&pe, NULL);
	int elapsed = ((pe.tv_sec - ps.tv_sec)*1000000) + (pe.tv_usec - ps.tv_usec);
	printf("%s took %dus\n", name, elapsed);
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec,
		unsigned int usec, void* data)
{
	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

void platform_message_callback(const FlutterPlatformMessage* message, void* user)
{
	printf("Platform message for channel %s, %d bytes\n", message->channel, message->message_size);
}

bool opengl_make_current(void* user)
{
	eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);
	return true;
}

bool opengl_clear_current(void* user)
{
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	return true;
}

struct gbm_bo *bo;
struct gbm_bo *next_bo;

bool display_init()
{
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();
	eglSwapBuffers(egl->display, egl->surface);

	bo = gbm_surface_lock_front_buffer(gbm->surface);
	struct drm_fb *fb = drm_fb_get_from_bo(bo);
	if (!fb)
	{
		printf("Failed to get a new framebuffer bo\n");
		return false;
	}
	int ret = drmModeSetCrtc(drm->fd, drm->crtc_id, fb->fb_id, 0, 0, &drm->connector_id, 1, drm->mode);
	if (ret)
	{
		printf("Failed to set mode: %s\n", strerror(errno));
		return false;
	}

	return true;
}

bool opengl_present(void* user)
{
	static bool first = true;
	static volatile int waiting_for_flip = 0;

	if (first)
	{
		perf_exit("time to first image");
		first = false;
	}

	while (waiting_for_flip)
	{
		drmEventContext ctx = { .version = 2, .page_flip_handler = page_flip_handler };
		drmHandleEvent(drm->fd, &ctx);
	}
	gbm_surface_release_buffer(gbm->surface, bo);
	bo = next_bo;

	eglSwapBuffers(egl->display, egl->surface);

	next_bo = gbm_surface_lock_front_buffer(gbm->surface);
	struct drm_fb *fb = drm_fb_get_from_bo(next_bo);
	if (!fb)
	{
		printf("Failed to get a new framebuffer bo\n");
		return false;
	}
	int ret = drmModeSetCrtc(drm->fd, drm->crtc_id, fb->fb_id, 0, 0, &drm->connector_id, 1, drm->mode);
	if (ret)
	{
		printf("Failed to set mode: %s\n", strerror(errno));
		return false;
	}
	waiting_for_flip = 1;
	ret = drmModePageFlip(drm->fd, drm->crtc_id, fb->fb_id, DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
	if (ret)
	{
		printf("failed to queue page flip: %s\n", strerror(errno));
		return false;
	}

	return true;
}

uint32_t opengl_fbo_callback(void* user) {
	return 0;
}

void update_window_size(int width, int height)
{
	FlutterWindowMetricsEvent event = {};
	event.struct_size = sizeof(event);
	event.width = width;
	event.height = height;
	event.pixel_ratio = 1.0;
	FlutterEngineSendWindowMetricsEvent(engine, &event);
}

void input_loop()
{
	int fd = open("/dev/input/event0", O_RDONLY);
	if (fd < 0)
	{
		printf("Failed to open input event0\n");
		return;
	}

	int curX = -1, curY = -1;
	bool down = false;
	while (1)
	{
		struct input_event event;
		if (read(fd, &event, sizeof(event)) <= 0)
		{
			printf("Error reading from event0\n");
			close(fd);
			return;
		}

		if (event.type == EV_ABS)
		{
			if (event.code == ABS_MT_POSITION_X)
				curX = event.value;
			else if (event.code == ABS_MT_POSITION_Y)
				curY = event.value;

			// pick one dimension since we will get two events for every move
			if (down && event.code == ABS_MT_POSITION_Y)
			{
				FlutterPointerEvent flutter_event;
				flutter_event.struct_size = sizeof(flutter_event);
				flutter_event.timestamp = (event.time.tv_sec * 1000000ull) + event.time.tv_usec;
				flutter_event.x = curX;
				flutter_event.y = curY;
				flutter_event.phase = kMove;
				FlutterEngineSendPointerEvent(engine, &flutter_event, 1);
			}
		}
		else if (event.type == EV_KEY && event.code == BTN_TOUCH)
		{
//			printf("%s on %d,%d\n", event.value == 0 ? "Up" : "Down", curX, curY);
			FlutterPointerEvent flutter_event;
			flutter_event.struct_size = sizeof(flutter_event);
			flutter_event.timestamp = (event.time.tv_sec * 1000000ull) + event.time.tv_usec;
			flutter_event.x = curX;
			flutter_event.y = curY;
			flutter_event.phase = event.value == 0 ? kUp : kDown;
			down = flutter_event.phase == kDown;
			FlutterEngineSendPointerEvent(engine, &flutter_event, 1);
		}
	}
}

int main()
{
	perf_enter();
	const char* device = "/dev/dri/card0";
	printf("Initializing DRM on %s\n", device);
	drm = init_drm_legacy(device);
	if (!drm)
	{
		printf("Failed to initialize DRM\n");
		return 1;
	}
	printf("Resolution: %dx%d\n", drm->mode->hdisplay, drm->mode->vdisplay);
	printf("Initializing GBM\n");
	uint64_t modifier = DRM_FORMAT_MOD_INVALID;
	gbm = init_gbm(drm->fd, drm->mode->hdisplay, drm->mode->vdisplay, modifier);
	if (!gbm)
	{
		printf("Failed to initialize GBM\n");
		return 1;
	}

	printf("Initializing EGL\n");
	egl = egl_init(gbm);
	if (!egl)
	{
		printf("Failed to initialize EGL\n");
		return 1;
	}

	printf("Initializing display\n");
	if (!display_init())
	{
		printf("Failed to initialize display\n");
		return 1;
	}
	perf_exit("Platform initialization");
	printf("Starting flutter\n");
	FlutterRendererConfig renderer;
	renderer.type = kOpenGL;
	renderer.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
	renderer.open_gl.make_current = opengl_make_current;
	renderer.open_gl.clear_current = opengl_clear_current;
	renderer.open_gl.present = opengl_present;
	renderer.open_gl.fbo_callback = opengl_fbo_callback;

	FlutterProjectArgs project;
	project.struct_size = sizeof(FlutterProjectArgs);
	project.assets_path = "project/app.flx";
	project.main_path = ""; //"project/main.dart";
	project.packages_path = ""; //"project/.packages";
	project.icu_data_path = ".";
	project.platform_message_callback = platform_message_callback;
	project.command_line_argc = 2;
	const char* argv[] = {"flutter_tester", "--aot-snapshot-path=."};
	project.command_line_argv = argv;

	perf_enter();
	FlutterResult res = FlutterEngineRun(FLUTTER_ENGINE_VERSION,
		&renderer, &project, NULL, &engine);

	if (res == kSuccess)
	{
		update_window_size(drm->mode->hdisplay, drm->mode->vdisplay);
		printf("Successfully started flutter engine: %p\n", engine);
		input_loop();
		FlutterEngineShutdown(engine);
		return 0;
	}
	else
	{
		printf("Flutter engine error: %d\n", res);
		return res;
	}
}
