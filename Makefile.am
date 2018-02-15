bin_PROGRAMS = run_engine

run_engine_LDADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	$(GLES2_LIBS) \
	-lm \
	-lflutter_engine

run_engine_CFLAGS = \
	-O2 -g \
	-Wall -Wextra \
	-std=c99 \
	$(DRM_CFLAGS) \
	$(GBM_CFLAGS) \
	$(EGL_CFLAGS) \
	$(GLES2_CFLAGS)

run_engine_SOURCES = \
	common.c \
	common.h \
	drm-common.c \
	drm-common.h \
	drm-legacy.c \
	drm-mode.c \
	main.c
