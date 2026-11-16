# PsiPDF's engine: MuPDF 1.3 cut down (no JavaScript, CJK fonts, XPS, CBZ,
# forms, saving, JBIG2 or JPEG 2000), with PsiPDF's own drawing and colour
# code (engine/), built for the PC or for EPOC R5.
#   ./fetch.sh                    MuPDF's source into third/mupdf (not in the repository)
#   make TARGET=host              build/host/pphost: the engine on the PC, for tests
#   make TARGET=epoc PSION_SDK=.. build/epoc/psipdf.exe (build.sh does this in psibuild)
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
MU      := third/mupdf
CORE    ?= core
TARGET  ?= host
O       := build/$(TARGET)
GEN     := build/gen

# MuPDF's files left out: other document kinds and outputs, forms, saving
FITZ_OUT := getopt load-jpx load-png load-tiff filter-jbig2 output-pcl output-pwg svg-device \
	trace-device stext-output stext-paragraph document
PDF_OUT  := pdf-write pdf-form pdf-field pdf-appearance pdf-event pdf-device pdf-pkcs7
FITZ_SRC := $(filter-out $(addprefix $(MU)/source/fitz/,$(FITZ_OUT:=.c)),$(wildcard $(MU)/source/fitz/*.c))
PDF_SRC  := $(filter-out $(addprefix $(MU)/source/pdf/,$(PDF_OUT:=.c)),$(wildcard $(MU)/source/pdf/*.c)) \
	$(MU)/source/pdf/js/pdf-js-none.c
FT_SRC   := base/ftbase base/ftbbox base/ftbitmap base/ftgasp base/ftglyph base/ftinit base/ftstroke \
	base/ftsynth base/ftsystem base/fttype1 base/ftxf86 cff/cff psaux/psaux pshinter/pshinter \
	psnames/psnames raster/raster sfnt/sfnt smooth/smooth truetype/truetype type1/type1 cid/type1cid
JPEG_SRC := jaricom jcomapi jdapimin jdapistd jdarith jdatasrc jdcoefct jdcolor jddctmgr jdhuff \
	jdinput jdmainct jdmarker jdmaster jdmerge jdpostct jdsample jdtrans jerror jidctflt jidctfst \
	jidctint jmemmgr jmemnobs jquant1 jquant2 jutils
Z_SRC    := adler32 crc32 inflate inftrees inffast zutil

MUDEFS  := -DNOCJK -DNODROIDFONT -DFZ_LARGEFILE=0 -I$(MU)/include -I$(MU)/scripts -I$(GEN) \
	-I$(MU)/thirdparty/freetype/include -I$(MU)/thirdparty/jpeg -I$(CORE)/ssh/zlib
FTDEFS  := -DFT2_BUILD_LIBRARY '-DFT_CONFIG_MODULES_H="slimftmodules.h"' '-DFT_CONFIG_OPTIONS_H="slimftoptions.h"' \
	-I$(MU)/scripts -I$(MU)/thirdparty/freetype/include

ifeq ($(TARGET),host)
CC      := gcc
CFLAGS  := -O2 -g -w -std=gnu99 -fcommon -fno-strict-aliasing
else
SDK     ?= $(PSION_SDK)
E       := $(SDK)/epoc_cpp_sdk/epoc32
REL     := $(E)/release/marm/rel
export PATH := /usr/bin:/bin:$(SDK)/gcc-3.0-psion-98r2-9/bin:$(E)/tools
export WINEDEBUG := -all
export TMP := /tmp/
CC      := arm-pe-gcc
# The ARM710's instruction set, as PsiWeb's engine is built: one psipdf.exe for
# the Series 5mx (ARM710T) and the netBook (SA-1100, which runs it as it is;
# libgcc's BX made MOV PC by v4bx.mk). Built for ARMv4 (halfword loads and
# the rest), the engine brought down the whole 5mx in the emulator as it
# opened an encrypted file. Soft float.
ARCH    := -O2 -fomit-frame-pointer -fno-strict-aliasing -nostdinc -mcpu=arm710 -mapcs-32 -mshort-load-bytes -msoft-float
DEFS    := -D__SYMBIAN32__ -D__PSISOFT32__ -D__GCC32__ -D__EPOC32__ -D__MARM__ -D__EXE__ -DNDEBUG
CFLAGS  := $(ARCH) $(DEFS) -std=gnu99 -fno-builtin -w -Iengine/mu/inc -I$(CORE)/web/compat -I$(E)/include/libc -I$(E)/include \
	-include engine/mu/ppcompat.h
CXXFLAGS := $(ARCH) $(DEFS) -Wno-ctor-dtor-privacy -fcheck-new -fvtable-thunks -w -I. -Iengine -I$(CORE)/ssh -I$(E)/include/libc -I$(E)/include
endif

objs = $(addprefix $(O)/$(1)/,$(notdir $(2:.c=.o)))
MU_OBJS := $(call objs,fitz,$(FITZ_SRC)) $(call objs,pdf,$(PDF_SRC)) $(O)/mu/ppstubs.o \
	$(addprefix $(O)/ft/,$(notdir $(FT_SRC:=.o))) $(addprefix $(O)/jpeg/,$(JPEG_SRC:=.o)) \
	$(addprefix $(O)/z/,$(Z_SRC:=.o))
PP_OBJS := $(O)/pp/ppcore.o $(O)/pp/ppquant.o

all: $(if $(filter host,$(TARGET)),$(O)/pphost,$(O)/psipdf.exe)

$(GEN)/gen_font_base14.h: tools/fontdump.py
	@mkdir -p $(GEN)
	python3 tools/fontdump.py $@ $(MU)/resources/fonts
$(GEN)/pppal.h: tools/mkpal.py
	@mkdir -p $(GEN)
	python3 tools/mkpal.py $@

$(O)/fitz/%.o: $(MU)/source/fitz/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(MUDEFS) $< -o $@
$(O)/pdf/pdf-fontfile.o: $(GEN)/gen_font_base14.h
$(O)/pdf/%.o: $(MU)/source/pdf/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(MUDEFS) $< -o $@
$(O)/pdf/%.o: $(MU)/source/pdf/js/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(MUDEFS) $< -o $@
$(O)/mu/%.o: engine/mu/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(MUDEFS) -Iengine/mu $< -o $@
$(O)/pp/ppquant.o: $(GEN)/pppal.h
$(O)/pp/%.o: engine/%.c engine/ppcore.h
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(MUDEFS) $< -o $@
define ftrule
$(O)/ft/$(notdir $(1)).o: $(MU)/thirdparty/freetype/src/$(1).c
	@mkdir -p $$(dir $$@)
	$$(CC) -c $$(CFLAGS) $$(FTDEFS) $$< -o $$@
endef
$(foreach f,$(FT_SRC),$(eval $(call ftrule,$(f))))
$(O)/jpeg/%.o: $(MU)/thirdparty/jpeg/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -I$(MU)/scripts -I$(MU)/thirdparty/jpeg -Dmain=xxxmain $< -o $@
$(O)/z/%.o: $(CORE)/ssh/zlib/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -DNO_GZIP $< -o $@

ifeq ($(TARGET),host)
$(O)/pphost: $(MU_OBJS) $(PP_OBJS) host/pphost.c
	$(CC) $(CFLAGS) -Iengine -o $@ host/pphost.c $(PP_OBJS) $(MU_OBJS) -lm
else
V4BX_DIR := $(O)
include $(CORE)/tools/v4bx.mk
SYSLIBS := $(REL)/estlib.lib $(REL)/euser.lib $(REL)/efsrv.lib $(REL)/fbscli.lib $(LIBGCC)
$(O)/epoc/%.o: engine/%.cpp engine/ppcore.h psipdf.h
	@mkdir -p $(dir $@)
	$(CC) -c $(CXXFLAGS) $< -o $@
EPOC_OBJS := $(O)/epoc/ppepoc.o $(O)/mu/ppcompat.o $(O)/mu/nsprintf.o
$(O)/mu/nsprintf.o: $(CORE)/web/compat/nsprintf.c
	@mkdir -p $(dir $@)
	$(CC) -c $(ARCH) $(DEFS) -fno-builtin -w -I$(E)/include/libc $< -o $@
LINKOBJS = $(REL)/eexe.o $(EPOC_OBJS) $(PP_OBJS) $(MU_OBJS) $(REL)/ecrt0.o $(SYSLIBS)
# heap 64 KB up to 24 MB (MuPDF takes what a page needs: a photo page about
# 4 MB), 64 KB stack; UID3 0x01000ADA (PsiPDF's engine)
$(O)/psipdf.exe: $(MU_OBJS) $(PP_OBJS) $(EPOC_OBJS) $(LIBGCC)
	arm-pe-ld -s -e _E32Startup --base-file $(O)/psipdf.bas -o $(O)/psipdf.tmp.exe $(LINKOBJS)
	arm-pe-dlltool --as=arm-pe-as --output-exp $(O)/psipdf.exp --base-file $(O)/psipdf.bas $(LINKOBJS)
	arm-pe-ld -s -e _E32Startup -Map $(O)/psipdf.map -o $(O)/psipdf.tmp.exe $(O)/psipdf.exp $(LINKOBJS)
	python3 $(V4BX_PY) --check $(O)/psipdf.tmp.exe
	wine $(E)/tools/petran.exe $(O)/psipdf.tmp.exe $@ -nocall -uid1 0x1000007a -uid2 0x00000000 -uid3 0x01000ADA -heap 0x10000 0x1800000 -stack 0x10000
	@ls -la $@
endif

.PHONY: all
