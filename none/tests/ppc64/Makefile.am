
include $(top_srcdir)/Makefile.tool-tests.am

dist_noinst_SCRIPTS = filter_stderr

noinst_HEADERS = ppc64_helpers.h isa_3_1_helpers.h

EXTRA_DIST = \
	jm-int.stderr.exp jm-int.stdout.exp jm-int.vgtest jm-int.stdout.exp-LE \
   jm-int.stdout.exp-LE-ISA3_0 \
	jm-int_other.stderr.exp jm-int_other.stdout.exp jm-int_other.vgtest \
	jm-int_other.stdout.exp-LE \
	jm-fp.stderr.exp  jm-fp.stdout.exp  jm-fp.vgtest jm-fp.stdout.exp-LE jm-fp.stdout.exp-LE2 jm-fp.stdout.exp-BE2 \
	jm-vmx.stderr.exp jm-vmx.stdout.exp jm-vmx.stdout.exp_Minus_nan jm-vmx.stdout.exp-LE \
	jm-vmx.vgtest \
	jm-misc.stderr.exp jm-misc.stdout.exp jm-misc.vgtest \
	lsw.stderr.exp lsw.stdout.exp lsw.vgtest \
	std_reg_imm.vgtest std_reg_imm.stderr.exp std_reg_imm.stdout.exp std_reg_imm.stdout.exp-LE \
	round.stderr.exp round.stdout.exp round.vgtest \
	twi_tdi.stderr.exp twi_tdi.stdout.exp twi_tdi.vgtest \
	tw_td.stderr.exp tw_td.stdout.exp tw_td.vgtest \
	opcodes.h \
	power6_bcmp.stderr.exp power6_bcmp.stdout.exp power6_bcmp.vgtest \
	power6_mf_gpr.stderr.exp power6_mf_gpr.stdout.exp power6_mf_gpr.vgtest \
	test_isa_2_06_part1.stderr.exp  test_isa_2_06_part1.stdout.exp  test_isa_2_06_part1.vgtest \
	test_isa_2_06_part1.stdout.exp-LE \
	test_isa_2_06_part2.stderr.exp  test_isa_2_06_part2.stdout.exp  test_isa_2_06_part2.vgtest \
	test_isa_2_06_part2-div.stderr.exp  test_isa_2_06_part2-div.stdout.exp \
	test_isa_2_06_part2-div.stdout.exp-LE-ISA3_0 test_isa_2_06_part2-div.vgtest \
	test_isa_2_06_part3.stderr.exp  test_isa_2_06_part3.stdout.exp  test_isa_2_06_part3.vgtest \
	test_isa_2_06_part3-div.stderr.exp  test_isa_2_06_part3-div.stdout.exp \
	test_isa_2_06_part3-div.stdout.exp-LE-ISA3_0  test_isa_2_06_part3-div.vgtest \
	test_dfp1.stderr.exp test_dfp1.stdout.exp test_dfp1.vgtest \
	test_dfp2.stderr.exp test_dfp2.stdout.exp test_dfp2.vgtest \
	test_dfp2.stdout.exp_Without_dcffix \
	test_dfp3.stderr.exp test_dfp3.stdout.exp test_dfp3.vgtest \
	test_dfp4.stderr.exp test_dfp4.stdout.exp test_dfp4.vgtest \
	test_dfp5.stderr.exp test_dfp5.stdout.exp test_dfp5.vgtest \
	jm_vec_isa_2_07.stderr.exp jm_vec_isa_2_07.stdout.exp jm_vec_isa_2_07.vgtest \
	jm_fp_isa_2_07.stderr.exp jm_fp_isa_2_07.stdout.exp jm_fp_isa_2_07.vgtest \
	jm_int_isa_2_07.stderr.exp jm_int_isa_2_07.vgtest \
	jm_int_isa_2_07.stdout.exp  jm_int_isa_2_07.stdout.exp-LE \
	test_isa_2_07_part2.stderr.exp test_isa_2_07_part2.stdout.exp test_isa_2_07_part2.vgtest \
	test_tm.stderr.exp test_tm.stdout.exp test_tm.vgtest \
	test_touch_tm.stderr.exp test_touch_tm.stdout.exp test_touch_tm.vgtest \
	ldst_multiple.stderr.exp ldst_multiple.stdout.exp ldst_multiple.vgtest \
	data-cache-instructions.stderr.exp data-cache-instructions.stdout.exp data-cache-instructions.vgtest \
	test_isa_3_0_altivec.stderr.exp  \
	test_isa_3_0_altivec.stdout.exp-LE test_isa_3_0_altivec.vgtest \
	test_isa_3_0_other.stderr.exp  \
	test_isa_3_0_other.stdout.exp-LE test_isa_3_0_other.vgtest \
	test_mod_instructions.stderr.exp test_mod_instructions.stdout.exp \
	test_mod_instructions.vgtest \
	test_isa_3_1_RT.vgtest test_isa_3_1_RT.stderr.exp test_isa_3_1_RT.stdout.exp \
	test_isa_3_1_XT.vgtest test_isa_3_1_XT.stderr.exp test_isa_3_1_XT.stdout.exp \
	test_isa_3_1_VRT.vgtest test_isa_3_1_VRT.stderr.exp test_isa_3_1_VRT.stdout.exp \
	test_isa_3_1_Misc.vgtest test_isa_3_1_Misc.stderr.exp test_isa_3_1_Misc.stdout.exp \
	test_isa_3_1_AT.vgtest test_isa_3_1_AT.stderr.exp test_isa_3_1_AT.stdout.exp \
	subnormal_test.stderr.exp  subnormal_test.stdout.exp \
	subnormal_test.vgtest

check_PROGRAMS = \
	allexec \
	jm-insns round \
	test_isa_2_06_part1 test_isa_2_06_part2 test_isa_2_06_part3 \
	test_dfp1 test_dfp2 test_dfp3 test_dfp4 test_dfp5 \
	test_isa_2_07_part1 test_isa_2_07_part2 \
	test_isa_3_0 test_mod_instructions \
	test_isa_3_1_RT test_isa_3_1_XT test_isa_3_1_VRT \
	test_isa_3_1_Misc test_isa_3_1_AT \
	subnormal_test \
	test_tm test_touch_tm data-cache-instructions \
	power6_mf_gpr std_reg_imm \
	twi_tdi tw_td power6_bcmp

# lmw, stmw, lswi, lswx, stswi, stswx compile (and run) only on big endian.
if VGCONF_PLATFORMS_INCLUDE_PPC64BE_LINUX
check_PROGRAMS += lsw ldst_multiple
endif

AM_CFLAGS    += @FLAG_M64@
AM_CXXFLAGS  += @FLAG_M64@
AM_CCASFLAGS += @FLAG_M64@

allexec_CFLAGS		= $(AM_CFLAGS) @FLAG_W_NO_NONNULL@

test_isa_3_1_XT_SOURCES = test_isa_3_1_XT.c test_isa_3_1_common.c
test_isa_3_1_RT_SOURCES = test_isa_3_1_RT.c test_isa_3_1_common.c
test_isa_3_1_VRT_SOURCES = test_isa_3_1_VRT.c test_isa_3_1_common.c
test_isa_3_1_Misc_SOURCES = test_isa_3_1_Misc.c test_isa_3_1_common.c
test_isa_3_1_AT_SOURCES = test_isa_3_1_AT.c test_isa_3_1_common.c

if HAS_ALTIVEC
BUILD_FLAG_ALTIVEC = -maltivec
ALTIVEC_FLAG = -DHAS_ALTIVEC
else
BUILD_FLAG_ALTIVEC =
ALTIVEC_FLAG =
endif

if HAS_VSX
BUILD_FLAG_VSX = -mvsx
VSX_FLAG = -DHAS_VSX
else
VSX_FLAG =
BUILD_FLAG_VSX =
endif

if HAS_DFP
BUILD_FLAGS_DFP = -mhard-dfp -mcpu=power6
DFP_FLAG = -DHAS_DFP
else
BUILD_FLAGS_DFP =
DFP_FLAG =
endif

if HAS_ISA_2_06
BUILD_FLAGS_ISA_2_06 = -mcpu=power7
ISA_2_06_FLAG = -DHAS_ISA_2_06
else
BUILD_FLAGS_ISA_2_06 =
ISA_2_06_FLAG =
endif

if HAS_ISA_2_07
BUILD_FLAGS_ISA_2_07 = -mcpu=power8
ISA_2_07_FLAG = -DHAS_ISA_2_07
else
BUILD_FLAGS_ISA_2_07 =
ISA_2_07_FLAG =
endif

if SUPPORTS_HTM
HTM_FLAG = -mhtm -DSUPPORTS_HTM
else
HTM_FLAG =
endif

jm_insns_CFLAGS = $(AM_CFLAGS) -Wl,-z,norelro -Winline -Wall -O -g -mregnames \
			@FLAG_M64@ $(ALTIVEC_FLAG) $(BUILD_FLAG_ALTIVEC)

if HAS_ISA_3_00
BUILD_FLAGS_ISA_3_00 = -mcpu=power8
ISA_3_00_FLAG = -DHAS_ISA_3_00
else
BUILD_FLAGS_ISA_3_00 =
ISA_3_00_FLAG =
endif

if HAS_ISA_3_1
BUILD_FLAGS_ISA_3_1 = -mcpu=power10
ISA_3_1_FLAG = -DHAS_ISA_3_1
else
BUILD_FLAGS_ISA_3_1 =
ISA_3_1_FLAG =
endif

test_isa_2_06_part1_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(VSX_FLAG) \
			@FLAG_M64@ $(ALTIVEC_FLAG) $(BUILD_FLAG_VSX)

test_isa_2_06_part2_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(VSX_FLAG) \
			@FLAG_M64@ $(ALTIVEC_FLAG) $(BUILD_FLAG_VSX)

test_isa_2_06_part3_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(VSX_FLAG) \
			@FLAG_M64@ $(ALTIVEC_FLAG) $(BUILD_FLAG_VSX)

test_dfp1_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(DFP_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_DFP)
test_dfp2_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(DFP_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_DFP)
test_dfp3_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(DFP_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_DFP)
test_dfp4_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(DFP_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_DFP)
test_dfp5_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(DFP_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_DFP)

test_isa_2_07_part1_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(ISA_2_07_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_2_07)
test_isa_2_07_part2_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(ISA_2_07_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_2_07)

test_tm_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(HTM_FLAG) $(ISA_2_07_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_2_07)
test_touch_tm_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(HTM_FLAG) $(ISA_2_07_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_2_07)

test_isa_3_0_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(HTM_FLAG) $(ISA_3_00_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_3_00)

test_isa_3_1_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames  $(ISA_3_1_FLAG) \
			@FLAG_M64@ $(BUILD_FLAGS_ISA_3_1)
test_isa_3_1_RT_CFLAGS = $(test_isa_3_1_CFLAGS)
test_isa_3_1_XT_CFLAGS = $(test_isa_3_1_CFLAGS)
test_isa_3_1_VRT_CFLAGS = $(test_isa_3_1_CFLAGS)
test_isa_3_1_Misc_CFLAGS = $(test_isa_3_1_CFLAGS)
test_isa_3_1_AT_CFLAGS = $(test_isa_3_1_CFLAGS)

subnormal_test_CFLAGS = $(AM_CFLAGS) -Winline -Wall -O -g -mregnames $(VSX_FLAG) $(ISA_2_06_FLAG) \
			@FLAG_M64@ $(ALTIVEC_FLAG) $(BUILD_FLAG_VSX) $(BUILD_FLAGS_ISA_2_06)

test_isa_2_06_part3_LDADD = -lm
test_dfp1_LDADD = -lm
test_dfp2_LDADD = -lm
test_dfp3_LDADD = -lm
test_dfp4_LDADD = -lm
test_dfp5_LDADD = -lm
test_isa_2_07_part1_LDADD = -lm
test_isa_2_07_part2_LDADD = -lm
test_tm_LDADD = -lm
test_touch_tm_LDADD = -lm
test_isa_3_0_LDADD = -lm

