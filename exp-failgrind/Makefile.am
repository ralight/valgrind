include $(top_srcdir)/Makefile.tool.am

EXTRA_DIST = docs/fg-manual.xml

#----------------------------------------------------------------------------
# Headers, etc
#----------------------------------------------------------------------------

pkginclude_HEADERS = failgrind.h
noinst_HEADERS = fg_global.h clg_global.h
#----------------------------------------------------------------------------
# exp_failgrind-<platform>
#----------------------------------------------------------------------------

noinst_PROGRAMS  = exp-failgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += exp-failgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

EXP_FAILGRIND_SOURCES_COMMON = fg_main.c fg_alloc.c fg_callstack.c fg_syscall.c clg_bb.c clg_bbcc.c clg_callstack.c clg_context.c clg_fn.c clg_jumps.c clg_main.c clg_threads.c

exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(EXP_FAILGRIND_SOURCES_COMMON)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(EXP_FAILGRIND_SOURCES_COMMON)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif

#----------------------------------------------------------------------------
# vgpreload_exp_failgrind-<platform>.so
#----------------------------------------------------------------------------

noinst_PROGRAMS += vgpreload_exp-failgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@.so
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += vgpreload_exp-failgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@.so
endif

if VGCONF_OS_IS_DARWIN
noinst_DSYMS = $(noinst_PROGRAMS)
endif

vgpreload_exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_SOURCES      =
vgpreload_exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CFLAGS       = \
	$(AM_CFLAGS_PSO_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_DEPENDENCIES = \
	$(LIBREPLACEMALLOC_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_LDFLAGS      = \
	$(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) \
	$(LIBREPLACEMALLOC_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)

if VGCONF_HAVE_PLATFORM_SEC
vgpreload_exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_SOURCES      =
vgpreload_exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CFLAGS       = \
	$(AM_CFLAGS_PSO_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_DEPENDENCIES = \
	$(LIBREPLACEMALLOC_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_exp_failgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_LDFLAGS      = \
	$(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@) \
	$(LIBREPLACEMALLOC_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
endif

bin_PROGRAMS = failgrind-helper

failgrind_helper_SOURCES = failgrind-helper.c
failgrind_helper_CPPFLAGS  = $(AM_CPPFLAGS_PRI)
failgrind_helper_CFLAGS    = $(AM_CFLAGS_PRI)
failgrind_helper_CCASFLAGS = $(AM_CCASFLAGS_PRI)
failgrind_helper_LDFLAGS   = $(AM_CFLAGS_PRI)
if VGCONF_PLATVARIANT_IS_ANDROID
failgrind_helper_CFLAGS    += -static
endif
# If there is no secondary platform, and the platforms include x86-darwin,
# then the primary platform must be x86-darwin.  Hence:
if ! VGCONF_HAVE_PLATFORM_SEC
if VGCONF_PLATFORMS_INCLUDE_X86_DARWIN
failgrind_helper_LDFLAGS   += -Wl,-read_only_relocs -Wl,suppress
endif
endif
