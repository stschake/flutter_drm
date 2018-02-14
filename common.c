#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static struct gbm gbm;
static struct egl egl;

const struct gbm * init_gbm(int drm_fd, int w, int h, uint64_t modifier)
{
	gbm.dev = gbm_create_device(drm_fd);

	if (modifier != DRM_FORMAT_MOD_INVALID) {
		fprintf(stderr, "Modifiers requested but support isn't available\n");
		return NULL;
	}
	gbm.surface = gbm_surface_create(gbm.dev, w, h, GBM_FORMAT_XRGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		return NULL;
	}

	gbm.width = w;
	gbm.height = h;

	return &gbm;
}

static bool has_ext(const char *extension_list, const char *ext)
{
	const char *ptr = extension_list;
	int len = strlen(ext);

	if (ptr == NULL || *ptr == '\0')
		return false;

	while (true) {
		ptr = strstr(ptr, ext);
		if (!ptr)
			return false;

		if (ptr[len] == ' ' || ptr[len] == '\0')
			return true;

		ptr += len;
	}
}

struct egl* egl_init(const struct gbm *gbm)
{
	EGLint major, minor, n;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	const char *egl_exts_client, *egl_exts_dpy, *gl_exts;

#define get_proc_client(ext, name) do { \
		if (has_ext(egl_exts_client, #ext)) \
			egl.name = (void *)eglGetProcAddress(#name); \
	} while (0)
#define get_proc_dpy(ext, name) do { \
		if (has_ext(egl_exts_dpy, #ext)) \
			egl.name = (void *)eglGetProcAddress(#name); \
	} while (0)

#define get_proc_gl(ext, name) do { \
		if (has_ext(gl_exts, #ext)) \
			egl.name = (void *)eglGetProcAddress(#name); \
	} while (0)

	egl_exts_client = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	get_proc_client(EGL_EXT_platform_base, eglGetPlatformDisplayEXT);

	if (egl.eglGetPlatformDisplayEXT) {
		egl.display = egl.eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR,
				gbm->dev, NULL);
	} else {
		egl.display = eglGetDisplay((void *)gbm->dev);
	}

	if (!eglInitialize(egl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return NULL;
	}

	egl_exts_dpy = eglQueryString(egl.display, EGL_EXTENSIONS);
	get_proc_dpy(EGL_KHR_image_base, eglCreateImageKHR);
	get_proc_dpy(EGL_KHR_image_base, eglDestroyImageKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglCreateSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglDestroySyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglWaitSyncKHR);
	get_proc_dpy(EGL_KHR_fence_sync, eglClientWaitSyncKHR);
	printf("Using display %p with EGL version %d.%d\n",
			egl.display, major, minor);

	printf("===================================\n");
	printf("EGL information:\n");
	printf("  version: \"%s\"\n", eglQueryString(egl.display, EGL_VERSION));
	printf("  vendor: \"%s\"\n", eglQueryString(egl.display, EGL_VENDOR));
	printf("  client extensions: \"%s\"\n", egl_exts_client);
	printf("  display extensions: \"%s\"\n", egl_exts_dpy);
	printf("===================================\n");

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return NULL;
	}

	if (!eglChooseConfig(egl.display, config_attribs, &egl.config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return NULL;
	}

	egl.context = eglCreateContext(egl.display, egl.config,
			EGL_NO_CONTEXT, context_attribs);
	if (egl.context == NULL) {
		printf("failed to create context\n");
		return NULL;
	}

	egl.surface = eglCreateWindowSurface(egl.display, egl.config,
			(EGLNativeWindowType)gbm->surface, NULL);
	if (egl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return NULL;
	}

	/* connect the context to the surface */
	eglMakeCurrent(egl.display, egl.surface, egl.surface, egl.context);

	gl_exts = (char *) glGetString(GL_EXTENSIONS);
	printf("OpenGL ES 2.x information:\n");
	printf("  version: \"%s\"\n", glGetString(GL_VERSION));
	printf("  shading language version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
	printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
	printf("  extensions: \"%s\"\n", gl_exts);
	printf("===================================\n");

	get_proc_gl(GL_OES_EGL_image, glEGLImageTargetTexture2DOES);

	return &egl;
}
