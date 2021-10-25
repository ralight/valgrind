
include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .
DIST_SUBDIRS = .

dist_noinst_SCRIPTS = filter_stderr

EXTRA_DIST = \
	alloc_atstart.vgtest alloc_atstart.stderr.exp \
	alloc_clreq1.vgtest alloc_clreq1.stderr.exp alloc_clreq1.stdout.exp \
	alloc_clreq2.vgtest alloc_clreq2.stderr.exp alloc_clreq2.stdout.exp \
	alloc_fail_chance.vgtest alloc_fail_chance.stderr.exp \
	alloc_input1.vgtest alloc_input1.stderr.exp \
	alloc_input2.vgtest alloc_input2.stderr.exp \
	alloc_max_fails.vgtest alloc_max_fails.stderr.exp \
	alloc_thresh_low.vgtest alloc_thresh_low.stderr.exp \
	alloc_thresh_high.vgtest alloc_thresh_high.stderr.exp \
	alloc_thresh_high_low.vgtest alloc_thresh_high_low.stderr.exp \
	alloc_thresh_low_invert.vgtest alloc_thresh_low_invert.stderr.exp \
	alloc_thresh_high_invert.vgtest alloc_thresh_high_invert.stderr.exp \
	alloc_thresh_high_low_invert1.vgtest alloc_thresh_high_low_invert1.stderr.exp \
	alloc_thresh_high_low_invert2.vgtest alloc_thresh_high_low_invert2.stderr.exp \
	alloc_toggle_1.vgtest alloc_toggle_1.stderr.exp \
	no_stats.vgtest no_stats.stderr.exp \
	syscall_atstart.vgtest syscall_atstart.stderr.exp syscall_atstart.stdout.exp \
	syscall_errno.vgtest syscall_errno.stderr.exp syscall_errno.stdout.exp \
	syscall_errno_specific1.vgtest syscall_errno_specific1.stderr.exp syscall_errno_specific1.stdout.exp \
	syscall_errno_specific2.vgtest syscall_errno_specific2.stderr.exp syscall_errno_specific2.stdout.exp \
	syscall_toggle_1.vgtest syscall_toggle_1.stderr.exp syscall_toggle_1.stdout.exp

check_PROGRAMS = alloc_atstart \
				 alloc_clreq1 alloc_clreq2 \
				 alloc_fail_chance \
				 alloc_input \
				 alloc_max_fails \
				 alloc_thresh_low alloc_thresh_low_invert \
				 alloc_thresh_high alloc_thresh_high_invert \
				 alloc_thresh_high_low alloc_thresh_high_low_invert1 alloc_thresh_high_low_invert2 \
				 alloc_toggle_1 \
				 no_stats \
				 syscall_atstart \
				 syscall_errno syscall_errno_specific1 syscall_errno_specific2 \
				 syscall_toggle_1

AM_CFLAGS   += $(AM_FLAG_M3264_PRI)
AM_CXXFLAGS += $(AM_FLAG_M3264_PRI)
