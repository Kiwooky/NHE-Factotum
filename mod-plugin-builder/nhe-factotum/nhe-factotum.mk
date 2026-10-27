######################################
#
# nhe-factotum  (path B package file)
#
# Builds the plugin from a GitHub repo at a given commit. Upload this file at
# https://builder.mod.audio/buildroot for an install link, or put it in
# mod-plugin-builder at plugins/package/nhe-factotum/nhe-factotum.mk.
#
# 1. DPF is a git submodule at dpf/; the hook below fetches it.
# 2. Push, then set NHE_FACTOTUM_VERSION to the FULL 40-character commit hash
#    (GitHub: Commits -> copy button, or `git rev-parse HEAD`).
# The filename must match the variable prefix: nhe-factotum.mk -> NHE_FACTOTUM_.
#
######################################

# <- after your first push, replace the zeros with the full 40-character commit hash
NHE_FACTOTUM_VERSION = 0000000000000000000000000000000000000000
NHE_FACTOTUM_SITE = https://github.com/Kiwooky/NHE-Factotum.git
NHE_FACTOTUM_SITE_METHOD = git
NHE_FACTOTUM_GIT_SUBMODULES = y
NHE_FACTOTUM_BUNDLES = nhe-factotum.lv2

# fetch git submodules (DPF), as MOD's own packages do (mod-plugin-builder)
NHE_FACTOTUM_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES

NHE_FACTOTUM_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true -C $(@D)

define NHE_FACTOTUM_BUILD_CMDS
	$(NHE_FACTOTUM_TARGET_MAKE)
endef

define NHE_FACTOTUM_INSTALL_TARGET_CMDS
	$(NHE_FACTOTUM_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) PREFIX=/usr
endef

$(eval $(generic-package))
