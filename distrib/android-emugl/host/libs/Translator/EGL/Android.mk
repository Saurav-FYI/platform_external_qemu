LOCAL_PATH := $(call my-dir)

host_OS_SRCS :=
host_common_LDLIBS :=

ifeq ($(HOST_OS),linux)
    host_OS_SRCS = EglOsApi_glx.cpp
    host_common_LDLIBS += -lGL -lX11 -ldl -lpthread
endif

ifeq ($(HOST_OS),darwin)
    host_OS_SRCS = EglOsApi_darwin.cpp \
                   MacNative.m   \
                   MacPixelFormatsAttribs.m

    host_common_LDLIBS += -Wl,-framework,AppKit
endif

ifeq ($(HOST_OS),windows)
    host_OS_SRCS = EglOsApi_wgl.cpp
    host_common_LDLIBS += -lgdi32
endif

host_common_SRC_FILES :=      \
     $(host_OS_SRCS)          \
     ThreadInfo.cpp           \
     EglImp.cpp               \
     EglConfig.cpp            \
     EglContext.cpp           \
     EglGlobalInfo.cpp        \
     EglValidate.cpp          \
     EglSurface.cpp           \
     EglWindowSurface.cpp     \
     EglPbufferSurface.cpp    \
     EglThreadInfo.cpp        \
     EglDisplay.cpp           \
     ClientAPIExts.cpp

### EGL host implementation ########################
$(call emugl-begin-host-shared-library,libEGL_translator)
$(call emugl-import,libGLcommon)

LOCAL_LDLIBS += $(host_common_LDLIBS)
LOCAL_SRC_FILES := $(host_common_SRC_FILES)

$(call emugl-end-module)

### EGL host implementation, 64-bit ########################
$(call emugl-begin-host64-shared-library,lib64EGL_translator)
$(call emugl-import,lib64GLcommon)

LOCAL_LDLIBS += $(host_common_LDLIBS)
LOCAL_SRC_FILES := $(host_common_SRC_FILES)

$(call emugl-end-module)
