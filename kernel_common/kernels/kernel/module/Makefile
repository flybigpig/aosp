# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for linux kernel module support
#

# These are called from save_stack_trace() on slub debug path,
# and produce insane amounts of uninteresting coverage.
KCOV_INSTRUMENT_main.o := n

obj-y += main.o
obj-y += strict_rwx.o
obj-y += kmod.o
obj-$(CONFIG_MODULE_DEBUG_AUTOLOAD_DUPS) += dups.o
obj-$(CONFIG_MODULE_DECOMPRESS) += decompress.o
obj-$(CONFIG_MODULE_SIG) += signing.o
obj-$(CONFIG_MODULE_SIG_PROTECT) += gki_module.o
obj-$(CONFIG_LIVEPATCH) += livepatch.o
obj-$(CONFIG_MODULES_TREE_LOOKUP) += tree_lookup.o
obj-$(CONFIG_DEBUG_KMEMLEAK) += debug_kmemleak.o
obj-$(CONFIG_KALLSYMS) += kallsyms.o
obj-$(CONFIG_PROC_FS) += procfs.o
obj-$(CONFIG_SYSFS) += sysfs.o
obj-$(CONFIG_KGDB_KDB) += kdb.o
obj-$(CONFIG_MODVERSIONS) += version.o
obj-$(CONFIG_MODULE_UNLOAD_TAINT_TRACKING) += tracking.o
obj-$(CONFIG_MODULE_STATS) += stats.o

#
# ANDROID: GKI: Generate headerfiles required for gki_module.o
#
# Dependencies on generated files need to be listed explicitly
$(obj)/gki_module.o: include/generated/gki_module_protected_exports.h \
			include/generated/gki_module_unprotected.h

ifneq ($(CONFIG_UNUSED_KSYMS_WHITELIST),)
ALL_KMI_SYMBOLS := $(CONFIG_UNUSED_KSYMS_WHITELIST)
else
ALL_KMI_SYMBOLS := include/config/abi_gki_kmi_symbols
$(ALL_KMI_SYMBOLS):
	: > $@
endif

include/generated/gki_module_unprotected.h: $(ALL_KMI_SYMBOLS) \
				$(srctree)/scripts/gen_gki_modules_headers.sh
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/gen_gki_modules_headers.sh $@ \
	"$(srctree)" \
	$(ALL_KMI_SYMBOLS)

# ABI protected exports list file specific to ARCH if exists else empty
ABI_PROTECTED_EXPORTS_FILE :=
ifeq ($(ARCH),arm64)
	ABI_PROTECTED_EXPORTS_FILE := $(wildcard $(srctree)/android/abi_gki_protected_exports_aarch64)
else
	ABI_PROTECTED_EXPORTS_FILE := $(wildcard $(srctree)/android/abi_gki_protected_exports_$(ARCH))
endif

include/generated/gki_module_protected_exports.h: $(ABI_PROTECTED_EXPORTS_FILE) \
				$(srctree)/scripts/gen_gki_modules_headers.sh
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/gen_gki_modules_headers.sh $@ \
	"$(srctree)" \
	$(ABI_PROTECTED_EXPORTS_FILE)
