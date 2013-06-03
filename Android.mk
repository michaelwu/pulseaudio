# This file is the top android makefile for all sub-modules.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

pulseaudio_TOP := $(LOCAL_PATH)

PA_BUILT_SOURCES := src/Android.mk

PA_BUILT_SOURCES := $(patsubst %, $(abspath $(pulseaudio_TOP))/%, $(PA_BUILT_SOURCES))

CONFIGURE_CFLAGS += -I$(PULSEAUDIO_TOP)/libtool/libltdl

SPEEX_CFLAGS := -Iexternal/speex/include
SPEEX_LIBS := -lspeexresampler

.PHONY: pulseaudio-configure pulseaudio-configure-real
pulseaudio-configure-real:
	echo $(PA_BUILT_SOURCES)
	cd $(pulseaudio_TOP) ; \
	CC="$(CONFIGURE_CC)" \
	CFLAGS="$(CONFIGURE_CFLAGS)" \
	CXX="$(CONFIGURE_CXX)" \
	CXXFLAGS="$(CONFIGURE_CXXFLAGS)" \
	LD="$(TARGET_LD)" \
	LDFLAGS="$(CONFIGURE_LDFLAGS)" \
	CPP="$(CONFIGURE_CPP)" \
	CPPFLAGS="$(CONFIGURE_CPPFLAGS)" \
	CXXCPP="$(CONFIGURE_CXXCPP)" \
	CXXCPPFLAGS="$(CONFIGURE_CXXCPPFLAGS)" \
	PKG_CONFIG_LIBDIR="$(CONFIGURE_PKG_CONFIG_LIBDIR)" \
	PKG_CONFIG_TOP_BUILD_DIR="/" \
	LIBSPEEX_CFLAGS="$(SPEEX_CFLAGS)" LIBSPEEX_LIBS="$(SPEEX_LIBS)" \
	ac_cv_header_sys_un_h=yes \
	$(abspath $(pulseaudio_TOP))/$(CONFIGURE) --host=$(CONFIGURE_HOST) \
	--prefix=/system \
	--localstatedir=/data/pulse \
	--disable-nls \
	--disable-largefile \
	--disable-rpath \
	--disable-x11 \
	--disable-samplerate \
	--disable-oss-output \
	--disable-oss-wrapper \
	--disable-coreaudio-output \
	--enable-alsa \
	--disable-esound \
	--disable-solaris \
	--disable-waveout \
	--disable-glib2 \
	--disable-gtk2 \
	--disable-gconf \
	--disable-avahi \
	--disable-jack \
	--disable-asyncns \
	--disable-tcpwrap \
	--disable-lirc \
	--disable-dbus \
	--disable-hal \
	--disable-bluez \
	--disable-udev \
	--disable-hal-compat \
	--disable-ipv6 \
	--disable-openssl \
	--disable-orc \
	--disable-manpages \
	--enable-per-user-esound-socket \
	--disable-webrtc-aec \
	--disable-default-build-tests \
	--disable-legacy-runtime-dir \
	--disable-legacy-database-entry-format \
	--disable-static-bins \
	--disable-force-preopen \
	--without-caps \
	--with-database=simple \
	--without-fftw \
	--with-system-user=root \
	--with-system-group=root \
	&& \
	for file in $(PA_BUILT_SOURCES); do \
		rm -f $$file && \
		make -C $$(dirname $$file) $$(basename $$file) ; \
	done

pulseaudio-configure: pulseaudio-configure-real

PA_CONFIGURE_TARGETS += pulseaudio-configure

-include $(pulseaudio_TOP)/src/Android.mk
