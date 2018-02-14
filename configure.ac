AC_PREREQ([2.60])
AC_INIT([run_engine], [0.0.1])
AC_CONFIG_AUX_DIR([build-aux])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_PROG_CC

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Obtain compiler/linker options for depedencies
PKG_CHECK_MODULES(DRM, [libdrm >= 2.4.71])
PKG_CHECK_MODULES(GBM, gbm >= 13.0)
PKG_CHECK_MODULES(EGL, egl)
PKG_CHECK_MODULES(GLES2, glesv2)

AC_CHECK_LIB([gbm], [gbm_bo_get_modifier], [gbm_modifiers=yes], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
