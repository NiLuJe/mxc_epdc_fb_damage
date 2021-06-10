# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pickup our cross-toolchains automatically...
# c.f., http://trac.ak-team.com/trac/browser/niluje/Configs/trunk/Kindle/Misc/x-compile.sh
#       https://github.com/NiLuJe/crosstool-ng
#       https://github.com/koreader/koxtoolchain
# NOTE: We want the "bare" variant of the TC env, to make sure we vendor the right stuff...
#       i.e., source ~SVN/Configs/trunk/Kindle/Misc/x-compile.sh kobo env bare
ifdef CROSS_TC
	# NOTE: If we have a CROSS_TC toolchain w/ CC set to Clang,
	#       assume we know what we're doing, and that everything is setup the right way already (i.e., via x-compile.sh tc env clang)...
	ifneq "$(CC)" "clang"
		CC:=$(CROSS_TC)-gcc
		CXX:=$(CROSS_TC)-g++
		STRIP:=$(CROSS_TC)-strip
		# NOTE: This relies on GCC plugins!
		#       Enforce AR & RANLIB to point to their real binary and not the GCC wrappers if your TC doesn't support that!
		AR:=$(CROSS_TC)-gcc-ar
		RANLIB:=$(CROSS_TC)-gcc-ranlib
	endif
else ifdef CROSS_COMPILE
	CC:=$(CROSS_COMPILE)cc
	CXX:=$(CROSS_COMPILE)cxx
	STRIP:=$(CROSS_COMPILE)strip
	AR:=$(CROSS_COMPILE)gcc-ar
	RANLIB:=$(CROSS_COMPILE)gcc-ranlib
else
	CC?=gcc
	CXX?=g++
	STRIP?=strip
	AR?=gcc-ar
	RANLIB?=gcc-ranlib
endif

DEBUG_CFLAGS:=-Og -fno-omit-frame-pointer -pipe -g
# Fallback CFLAGS, we honor the env first and foremost!
OPT_CFLAGS:=-O2 -fomit-frame-pointer -pipe

ifdef DEBUG
	OUT_DIR:=Debug
	CFLAGS?=$(DEBUG_CFLAGS)
	CXXFLAGS?=$(DEBUG_CFLAGS)
	EXTRA_CPPFLAGS+=-DDEBUG
	STRIP:=true
else
	OUT_DIR:=Release
	CFLAGS?=$(OPT_CFLAGS)
	CXXFLAGS?=$(OPT_CFLAGS)
	EXTRA_CPPFLAGS+=-DNDEBUG
endif

# Explictly enforce debug CFLAGS for the debug target (vs., simply having DEBUG set in the env)
ifdef DEBUGFLAGS
	CFLAGS:=$(DEBUG_CFLAGS)
endif

# Detect GCC version because reasons...
# (namely, GCC emitting an error instead of a warning on unknown -W options)
MOAR_WARNIGS:=0
# Tests heavily inspired from Linux's build system ;).
CC_IS_CLANG:=$(shell $(CC) -v 2>&1 | grep -q "clang version" && echo 1 || echo 0)
CC_VERSION:=$(shell printf "%02d%02d%02d" `echo __GNUC__ | $(CC) -E -x c - | tail -n 1` `echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1` `echo __GNUC_PATCHLEVEL__ | $(CC) -E -x c - | tail -n 1`)
# Detect Clang's SA, too...
ifeq "$(CC_IS_CLANG)" "0"
	ifeq "$(lastword $(subst /, ,$(CC)))" "ccc-analyzer"
		CC_IS_CLANG:=1
	endif
endif
ifeq "$(CC_IS_CLANG)" "1"
	# This is Clang
	MOAR_WARNIGS:=1
endif
ifeq "$(shell expr $(CC_VERSION) \>= 070000)" "1"
	# This is GCC >= 7
	MOAR_WARNIGS:=1
else
	# We may be silencing warnings unknown to older compilers via pragmas, so don't warn about those...
	EXTRA_CFLAGS+=-Wno-pragmas
endif

# Detect whether our TC is cross (at least as far as the target arch is concerned)
HOST_ARCH:=$(shell uname -m)
TARGET_ARCH:=$(shell $(CC) $(CFLAGS) -dumpmachine 2>/dev/null)
CC_IS_CROSS:=0
# Host doesn't match target, assume it's a cross TC
ifeq (,$(findstring $(HOST_ARCH),$(TARGET_ARCH)))
	CC_IS_CROSS:=1
endif

ifndef DEBUG
	# Enable loop unrolling & vectorization
	EXTRA_CFLAGS+=-ftree-vectorize
	EXTRA_CFLAGS+=-funroll-loops
	# Always match GCC >= 10 new default
	ifeq "$(CC_IS_CLANG)" "0"
		EXTRA_CFLAGS+=-fno-common
	endif
endif

# Moar warnings!
EXTRA_CFLAGS+=-Wall
EXTRA_CFLAGS+=-Wextra -Wunused
EXTRA_CFLAGS+=-Wformat=2
ifeq "$(MOAR_WARNIGS)" "1"
	EXTRA_CFLAGS+=-Wformat-signedness
	# NOTE: -Wformat-truncation=2 is still a tad too aggressive w/ GCC 9, so, tone it down to avoid false-positives...
	EXTRA_CFLAGS+=-Wformat-truncation=1
	# NOTE: This doesn't really play nice w/ FORTIFY, leading to an assload of false-positives, unless LTO is enabled
	ifeq (,$(findstring flto,$(CFLAGS)))
		# NOTE: GCC 9 is more verbose, so nerf that, too, when building w/o LTO on native systems...
		ifeq "$(CC_IS_CROSS)" "0"
			EXTRA_CFLAGS+=-Wno-stringop-truncation
		endif
	endif
	EXTRA_CFLAGS+=-Wnull-dereference
endif
EXTRA_CFLAGS+=-Wuninitialized
ifeq (flto,$(findstring flto,$(CFLAGS)))
	# NOTE: Inlining put_pixel in fbink_print_ot triggers a few -Wmaybe-uninitialized when we pass grayscale pixels...
	#       Actually harmless, because they trip in an RGB565 codepath, which we make sure always get fed RGB32.
	#       Unfortunately, since they're tripped at link-time, I can't pragma'em away :/.
	EXTRA_CFLAGS+=-Wno-maybe-uninitialized
endif
ifeq "$(MOAR_WARNIGS)" "1"
	EXTRA_CFLAGS+=-Wduplicated-branches -Wduplicated-cond
endif
EXTRA_CFLAGS+=-Wundef
EXTRA_CFLAGS+=-Wbad-function-cast
EXTRA_CFLAGS+=-Wwrite-strings
EXTRA_CFLAGS+=-Wjump-misses-init
EXTRA_CFLAGS+=-Wlogical-op
EXTRA_CFLAGS+=-Wstrict-prototypes -Wold-style-definition
EXTRA_CFLAGS+=-Wshadow
EXTRA_CFLAGS+=-Wmissing-prototypes -Wmissing-declarations
EXTRA_CFLAGS+=-Wnested-externs
EXTRA_CFLAGS+=-Winline
EXTRA_CFLAGS+=-Wcast-qual
# NOTE: GCC 8 introduces -Wcast-align=strict to warn regardless of the target architecture (i.e., like clang)
EXTRA_CFLAGS+=-Wcast-align
EXTRA_CFLAGS+=-Wconversion
# Output padding info when debugging (NOTE: Clang is slightly more verbose)
# As well as function attribute hints
ifdef DEBUG
	EXTRA_CFLAGS+=-Wpadded
	EXTRA_CFLAGS+=-Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format -Wmissing-format-attribute
endif
# And disable this, because it obviously doesn't play well with using goto to handle cleanup on error codepaths...
EXTRA_CFLAGS+=-Wno-jump-misses-init
# And just because that's annoying...
ifeq "$(CC_IS_CLANG)" "1"
	EXTRA_CFLAGS+=-Wno-ignored-optimization-argument -Wno-unknown-warning-option
endif

# We're Linux-bound anyway...
EXTRA_CPPFLAGS+=-D_GNU_SOURCE

# NOTE: Always use as-needed to avoid unecessary DT_NEEDED entries :)
LDFLAGS?=-Wl,--as-needed

##
# Now that we're done fiddling with flags, let's build stuff!
CMD_SRCS:=damage_report.c


default: all

CMD_OBJS:=$(addprefix $(OUT_DIR)/, $(CMD_SRCS:.c=.o))


# CLI
$(OUT_DIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

outdir:
	mkdir -p $(OUT_DIR)

$(CMD_OBJS): | outdir
$(BTN_OBJS): | outdir

all: utils

utils: $(CMD_OBJS)
	$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) -o$(OUT_DIR)/damage_report $(CMD_OBJS) $(LIBS)

strip: utils
	$(STRIP) --strip-unneeded $(OUT_DIR)/damage_report

debug:
	$(MAKE) utils DEBUG=true DEBUGFLAGS=true


clean:
	rm -rf Release/*.o
	rm -rf Release/damage_report
	rm -rf Debug/*.o
	rm -rf Debug/damage_report

.PHONY: default outdir all utils strip clean distclean
