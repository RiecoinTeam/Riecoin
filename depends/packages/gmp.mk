package=gmp
$(package)_version=6.2.0
$(package)_download_path=https://gmplib.org/download/gmp/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=f51c99cb114deb21a60075ffb494c1a210eb9d7cb729ed042ddb7de9534451ea

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-cxx
endef

define $(package)_config_cmds
  ./configure $($(package)_config_opts) --build=$(build) --host=$(host) --prefix=$(host_prefix)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
