package=gmp
$(package)_version=6.3.0
$(package)_download_path=https://gmplib.org/download/gmp/
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-cxx --enable-fat CFLAGS="-O2 -fPIE" CXXFLAGS="-O2 -fPIE"
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
