LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE       := default.pa
LOCAL_MODULE_TAGS  := optional eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := default.pa
LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/pulse
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := daemon.conf
LOCAL_MODULE_TAGS  := optional eng
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := daemon.conf
LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/pulse
include $(BUILD_PREBUILT)

PA_DEFINES := \
  -DHAVE_GETADDRINFO \
  -DHAVE_NANOSLEEP \
  -DHAVE_SIGACTION \
  -DHAVE_ARPA_INET_H \
  -DHAVE_ALSA \
  -DHAVE_CLOCK_GETTIME \
  -DHAVE_CTIME_R \
  -DHAVE_FCHMOD \
  -DHAVE_FCHOWN \
  -DHAVE_FORK \
  -DHAVE_FSTAT \
  -DHAVE_GETPWNAM_R \
  -DHAVE_GETPWUID_R \
  -DHAVE_GETTIMEOFDAY \
  -DHAVE_GETUID \
  -DHAVE_GRP_H \
  -DHAVE_IPV6 \
  -DHAVE_LSTAT \
  -DHAVE_MLOCK \
  -DHAVE_NETDB_H \
  -DHAVE_NETINET_IN_H \
  -DHAVE_NETINET_IN_SYSTM_H \
  -DHAVE_NETINET_IP_H \
  -DHAVE_NETINET_TCP_H \
  -DHAVE_PIPE \
  -DHAVE_PIPE2 \
  -DHAVE_PTHREAD \
  -DHAVE_PWD_H \
  -DHAVE_READLINE \
  -DHAVE_REGEX_H \
  -DHAVE_SCHED_H \
  -DHAVE_SETRESGID \
  -DHAVE_SETRESUID \
  -DHAVE_SETSID \
  -DHAVE_SIGACTION \
  -DHAVE_STD_BOOL \
  -DHAVE_SYMLINK \
  -DHAVE_SYS_EVENTFD_H \
  -DHAVE_SYS_IOCTL_H \
  -DHAVE_SYSLOG_H \
  -DHAVE_SYS_PRCTL_H \
  -DHAVE_SYS_RESOURCE_H \
  -DHAVE_SYS_SELECT_H \
  -DHAVE_SYS_SOCKET_H \
  -DHAVE_SYS_SYSCALL_H \
  -DHAVE_SYS_UN_H \
  -DHAVE_SYS_WAIT_H \
  -DHAVE_UNAME \
  -DHAVE_STRERROR_R \
  -DHAVE_SYSCONF \
  -DHAVE_POLL_H \
  -DPAGE_SIZE=4096 \
  -DPA_BINARY=\"/system/bin/pulseaudio\" \
  -DPA_DEFAULT_CONFIG_DIR=\"/system/etc/pulse\" \
  -DATOMIC_ARM_LINUX_HELPERS \
  -DGETGROUPS_T=gid_t \
  -DPA_MACHINE_ID=\"/usr/var/lib/dbus/machine-id\" \
  -DPA_MACHINE_ID_FALLBACK=\"/usr/etc/machine-id\" \
  -DPA_SYSTEM_RUNTIME_PATH=\"/usr/var/run/pulse\" \
  -DPA_BUILDDIR=\"\" \
  -DPA_DLSEARCHPATH=\"/system/lib/pulse\" \
  -DPA_SYSTEM_USER=\"system\" \
  -DPA_SYSTEM_GROUP=\"system\" \
  -DPA_SYSTEM_STATE_PATH=\"/data/local/pulse\" \
  -DPA_SYSTEM_CONFIG_PATH=\"/system/etc/pulse\" \
  -DDISABLE_ORC \
  -DPACKAGE \
  -DPACKAGE_NAME=\"pulseaudio\" \
  -DPACKAGE_VERSION=\"1.99\" \
  -DCANONICAL_HOST=\"\" \
  -DPA_CFLAGS=\"\" \
  -DPA_ALSA_PROFILE_SETS_DIR=\"/system/etc/pulse/profiles\" \
  -DPA_ALSA_PATHS_DIR=\"/system/etc/pulse/paths\" \
  -DPA_ACCESS_GROUP=\"audio\" \
  -UNDEBUG

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
  pulse/client-conf.c  \
  pulse/fork-detect.c  \
  pulse/xmalloc.c  \
  pulse/proplist.c  \
  pulse/utf8.c pulse/utf8.h \
  pulse/channelmap.c  \
  pulse/sample.c  \
  pulse/util.c  \
  pulse/timeval.c  \
  pulse/rtclock.c  \
  pulsecore/authkey.c  \
  pulsecore/conf-parser.c  \
  pulsecore/core-error.c  \
  pulsecore/core-rtclock.c  \
  pulsecore/core-util.c  \
  pulsecore/dynarray.c  \
  pulsecore/flist.c  \
  pulsecore/hashmap.c  \
  pulsecore/i18n.c \
  pulsecore/idxset.c  \
  pulsecore/arpa-inet.c  \
  pulsecore/iochannel.c  \
  pulsecore/ioline.c  \
  pulsecore/ipacl.c  \
  pulsecore/lock-autospawn.c  \
  pulsecore/log.c  \
  pulsecore/ratelimit.c  \
  pulsecore/mcalign.c  \
  pulsecore/memblock.c  \
  pulsecore/memblockq.c  \
  pulsecore/memchunk.c  \
  pulsecore/once.c  \
  pulsecore/packet.c  \
  pulsecore/parseaddr.c  \
  pulsecore/pdispatch.c  \
  pulsecore/pid.c  \
  pulsecore/pipe.c  \
  pulsecore/poll.c  \
  pulsecore/memtrap.c  \
  pulsecore/aupdate.c  \
  pulsecore/proplist-util.c  \
  pulsecore/pstream-util.c  \
  pulsecore/pstream.c  \
  pulsecore/queue.c  \
  pulsecore/random.c  \
  pulsecore/shm.c  \
  pulsecore/bitset.c  \
  pulsecore/socket-client.c  \
  pulsecore/socket-server.c  \
  pulsecore/socket-util.c  \
  pulsecore/strbuf.c  \
  pulsecore/strlist.c  \
  pulsecore/tagstruct.c  \
  pulsecore/time-smoother.c  \
  pulsecore/tokenizer.c  \
  pulsecore/usergroup.c  \
  pulsecore/sndfile-util.c \
  pulsecore/mutex-posix.c \
  pulsecore/thread-posix.c \
  pulsecore/semaphore-posix.c

LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -DHAVE_SYS_MMAN_H

LOCAL_C_INCLUDES += \
  external/libsndfile/src
LOCAL_MODULE := libpulsecommon
LOCAL_SHARED_LIBRARIES:= libsalsa libsndfile
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
  pulse/channelmap.c \
  pulse/context.c \
  pulse/error.c \
  pulse/ext-device-manager.c \
  pulse/ext-device-restore.c \
  pulse/ext-stream-restore.c \
  pulse/format.c \
  pulse/introspect.c \
  pulse/mainloop-api.c \
  pulse/mainloop-signal.c \
  pulse/mainloop.c \
  pulse/operation.c \
  pulse/proplist.c \
  pulse/rtclock.c \
  pulse/sample.c \
  pulse/scache.c \
  pulse/stream.c \
  pulse/subscribe.c \
  pulse/thread-mainloop.c \
  pulse/timeval.c \
  pulse/utf8.c pulse/utf8.h \
  pulse/util.c \
  pulse/volume.c \
  pulse/xmalloc.c

LOCAL_C_INCLUDES += external/json-c
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -DHAVE_SYS_MMAN_H
LOCAL_MODULE := libpulse
LOCAL_SHARED_LIBRARIES:= libpulsecommon libjson
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
                pulsecore/asyncmsgq.c \
                pulsecore/asyncq.c \
                pulsecore/auth-cookie.c \
                pulsecore/cli-command.c \
                pulsecore/cli-text.c \
                pulsecore/client.c \
                pulsecore/card.c \
                pulsecore/core-scache.c \
                pulsecore/core-subscribe.c \
                pulsecore/core.c \
                pulsecore/fdsem.c \
                pulsecore/g711.c \
                pulsecore/hook-list.c \
                pulsecore/ltdl-helper.c \
                pulsecore/modargs.c \
                pulsecore/modinfo.c \
                pulsecore/module.c \
                pulsecore/msgobject.c \
                pulsecore/namereg.c \
                pulsecore/object.c \
                pulsecore/play-memblockq.c \
                pulsecore/play-memchunk.c \
                pulsecore/remap.c \
                pulsecore/remap_mmx.c pulsecore/remap_sse.c \
                pulsecore/resampler.c \
                pulsecore/rtpoll.c \
                pulsecore/sample-util.c \
                pulsecore/cpu-arm.c \
                pulsecore/cpu-x86.c \
                pulsecore/cpu-orc.c \
                pulsecore/svolume_c.c pulsecore/svolume_arm.c \
                pulsecore/svolume_mmx.c pulsecore/svolume_sse.c \
                pulsecore/sconv-s16be.c \
                pulsecore/sconv-s16le.c \
                pulsecore/sconv_sse.c \
                pulsecore/sconv.c \
                pulsecore/shared.c \
                pulsecore/sink-input.c \
                pulsecore/sink.c \
                pulsecore/device-port.c \
                pulsecore/sioman.c \
                pulsecore/sound-file-stream.c \
                pulsecore/sound-file.c \
                pulsecore/source-output.c \
                pulsecore/source.c \
                pulsecore/start-child.c \
                pulsecore/thread-mq.c \
                pulsecore/database-simple.c \
                pulsecore/protocol-native.c \
                pulsecore/protocol-cli.c \
                pulsecore/cli.c \
		pulsecore/ffmpeg/resample2.c

LOCAL_C_INCLUDES += external/src external/libsndfile/src
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -D__INCLUDED_FROM_PULSE_AUDIO -DHAVE_SYS_MMAN_H
LOCAL_MODULE := libpulsecore
LOCAL_SHARED_LIBRARIES:= libpulsecommon libpulse libsndfile libdl
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
  modules/alsa/alsa-util.c \
  modules/alsa/alsa-mixer.c \
  modules/alsa/alsa-sink.c \
  modules/alsa/alsa-source.c \
  modules/reserve-wrap.c

LOCAL_C_INCLUDES += external/salsa-lib/src
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -D__INCLUDED_FROM_PULSE_AUDIO -DHAVE_SYS_MMAN_H
LOCAL_MODULE := libalsa-util
LOCAL_SHARED_LIBRARIES:= libsalsa libpulsecore libpulsecommon libpulse
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:=
LOCAL_SRC_FILES:= \
                daemon/caps.c \
                daemon/cmdline.c \
                daemon/cpulimit.c \
                daemon/daemon-conf.c \
                daemon/dumpmodules.c \
                daemon/ltdl-bind-now.c \
                daemon/main.c

LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES)
LOCAL_MODULE := pulseaudio
LOCAL_SHARED_LIBRARIES:= libpulsecore libpulsecommon libpulse
LOCAL_MODULE_TAGS := optional eng
include $(BUILD_EXECUTABLE)

#  module-cli-protocol-tcp \
  module-simple-protocol-tcp \
  module-http-protocol-tcp \
  module-native-protocol-tcp \
  module-native-protocol-fd \
  module-cli-protocol-unix \
  module-simple-protocol-unix \
  module-http-protocol-unix \
  module-native-protocol-unix \
  module-cli \
  module-device-manager \

define PA_MODULE_TEMPLATE
  include $$(CLEAR_VARS)
  LOCAL_MODULE := $(1)
  $$(LOCAL_PATH)/$(2)/$$(LOCAL_MODULE).c: $$(LOCAL_PATH)/$(2)/$$(LOCAL_MODULE)-symdef.h
  $$(LOCAL_PATH)/$(2)/$$(LOCAL_MODULE)-symdef.h: $$(LOCAL_PATH)/modules/module-defs.h.m4
	m4 -Dfname="$$@" $$< > $$@
  LOCAL_C_INCLUDES += external/salsa-lib/src
  LOCAL_SRC_FILES := $(2)/$$(LOCAL_MODULE).c
  LOCAL_CFLAGS := -std=gnu99 $$(PA_DEFINES) -D__INCLUDED_FROM_PULSE_AUDIO -DHAVE_SYS_MMAN_H
  LOCAL_SHARED_LIBRARIES := libpulsecore libpulsecommon libpulse $(3)
  LOCAL_MODULE_TAGS := optional eng
  LOCAL_PRELINK_MODULE := false
  LOCAL_MODULE_PATH := $$(TARGET_OUT_SHARED_LIBRARIES)/pulse
  include $$(BUILD_SHARED_LIBRARY)
endef

PA_MODULES := \
  module-null-sink \
  module-null-source \
  module-detect \
  module-device-restore \
  module-stream-restore \
  module-card-restore \
  module-default-device-restore \
  module-always-sink \
  module-rescue-streams \
  module-intended-roles \
  module-suspend-on-idle \
  module-sine \
  module-combine \
  module-combine-sink \
  module-remap-sink \
  module-role-cork \
  module-loopback \
  module-virtual-sink \
  module-virtual-source \
  module-virtual-surround-sink \
  module-switch-on-connect \
  module-switch-on-port-available \
  module-filter-apply \
  module-filter-heuristics \
  module-cli

PA_ALSA_MODULES := \
  module-alsa-sink \
  module-alsa-source \
  module-alsa-card

$(foreach pamod,$(PA_MODULES),$(eval $(call PA_MODULE_TEMPLATE,$(pamod),modules,)))
$(foreach pamod,$(PA_ALSA_MODULES),$(eval $(call PA_MODULE_TEMPLATE,$(pamod),modules/alsa,libalsa-util libsalsa)))

include $(CLEAR_VARS)
LOCAL_MODULE := module-native-protocol-unix
$(LOCAL_PATH)/modules/module-protocol-stub.c: $(LOCAL_PATH)/modules/$(LOCAL_MODULE)-symdef.h
$(LOCAL_PATH)/modules/$(LOCAL_MODULE)-symdef.h: $(LOCAL_PATH)/modules/module-defs.h.m4
	m4 -Dfname="$@" $< > $@
LOCAL_C_INCLUDES += external/salsa-lib/src
LOCAL_SRC_FILES := modules/module-protocol-stub.c
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -D__INCLUDED_FROM_PULSE_AUDIO -DHAVE_SYS_MMAN_H -DUSE_UNIX_SOCKETS -DUSE_PROTOCOL_NATIVE
LOCAL_SHARED_LIBRARIES := libpulsecore libpulsecommon libpulse
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/pulse
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := module-cli-protocol-unix
$(LOCAL_PATH)/modules/module-protocol-stub.c: $(LOCAL_PATH)/modules/$(LOCAL_MODULE)-symdef.h
$(LOCAL_PATH)/modules/$(LOCAL_MODULE)-symdef.h: $(LOCAL_PATH)/modules/module-defs.h.m4
	m4 -Dfname="$@" $< > $@
LOCAL_C_INCLUDES += external/salsa-lib/src
LOCAL_SRC_FILES := modules/module-protocol-stub.c
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES) -D__INCLUDED_FROM_PULSE_AUDIO -DHAVE_SYS_MMAN_H -DUSE_UNIX_SOCKETS -DUSE_PROTOCOL_CLI
LOCAL_SHARED_LIBRARIES := libpulsecore libpulsecommon libpulse
LOCAL_MODULE_TAGS := optional eng
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/pulse
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:=
LOCAL_SRC_FILES:= utils/pacmd.c
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES)
LOCAL_MODULE := pacmd
LOCAL_SHARED_LIBRARIES:= libpulsecommon libpulse
LOCAL_MODULE_TAGS := optional eng
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/libsndfile/src
LOCAL_SRC_FILES:= utils/pactl.c
LOCAL_CFLAGS := -std=gnu99 $(PA_DEFINES)
LOCAL_MODULE := pactl
LOCAL_SHARED_LIBRARIES:= libpulsecommon libpulse libsndfile
LOCAL_MODULE_TAGS := optional eng
include $(BUILD_EXECUTABLE)
