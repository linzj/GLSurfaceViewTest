#include <jni.h>
#include <stdlib.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <arm_neon.h>

#define TAG "LINZJ"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
static ANativeWindow* gWindow;

static void nativeOnSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
   if (gWindow) {
       ANativeWindow_release(gWindow);
   }
   ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
   int32_t err = ANativeWindow_setBuffersGeometry(window, 640, 480, WINDOW_FORMAT_RGBA_8888);
   if (err) {
       LOGD("nativeOnSurfaceCreated: err: %d\n", err);
   } else {
       gWindow = window;
       ANativeWindow_acquire(window);
   }
}

static void nativeOnDraw(JNIEnv* env, jobject thiz) {
    ANativeWindow_Buffer windowBuffer;
    int32_t err;
    err = ANativeWindow_lock(gWindow, &windowBuffer, nullptr);
    if (err) {
        LOGE("failed to lock window");
        return;
    }
    LOGD("lock window buffer succeed");
    if (windowBuffer.format != WINDOW_FORMAT_RGBA_8888) {
        LOGD("incorrect window format");
        abort();
    }
    if (windowBuffer.width & (4 - 1)) {
        LOGE("windowBuffer.width :%d is not 4 byte align", windowBuffer.width);
        abort();
    }
    uint8_t* line_start = static_cast<uint8_t*>(windowBuffer.bits);
    static const uint8_t allRed[] = {
        0xff, 0, 0, 0,
        0xff, 0xff, 0, 0,
        0xff, 0, 0, 0,
        0xff, 0xff, 0, 0,
    };
    uint8x8x2_t data = vld2_u8(allRed);
    for (int j = 0; j < windowBuffer.height; ++j, line_start += windowBuffer.stride * 4) {
        uint8_t* store = line_start;
        for (int i = 0; i < windowBuffer.width / 4; ++i, store += sizeof(uint8x8x2_t)) {
            // *store = data;
            vst2_u8(store, data);
        }
    }
    ANativeWindow_unlockAndPost(gWindow);
}

static const char* gRendererClass = "com/alibaba/uc/GLSurfaceViewTestRenderer";
static JNINativeMethod gMethods[] = {
    {"nOnSurfaceCreated", "(Landroid/view/Surface;)V",
     (void*)nativeOnSurfaceCreated},
    {"nOnDraw",
     "()V",
     (void*)nativeOnDraw},
    };

static int registerNativeMethods(JNIEnv* env, const char* className,
                                 JNINativeMethod* methods, int numMethods) {
  jclass clazz = (env)->FindClass(className);
  if (clazz == NULL) {
    LOGE("registerNativeMethods failed to find class '%s'", className);
    return JNI_FALSE;
  }
  if ((env)->RegisterNatives(clazz, methods, numMethods) < 0) {
    LOGE(
        "registerNativeMethods failed to register native methods for class "
        "'%s'",
        className);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

static int registerNatives(JNIEnv* env) {
  return registerNativeMethods(env, gRendererClass, gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0]));
}

/**
 * This function will be call when the library first be load.
 * You can do some init in the lib. return which version jni it support.
 */

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  LOGD("begin JNI_OnLoad");
  JNIEnv* env;
  /* Get environment */
  if ((vm)->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
    return JNI_FALSE;
  }

  if (registerNatives(env) != JNI_TRUE) {
    return JNI_FALSE;
  }

  LOGD("end JNI_OnLoad");
  return JNI_VERSION_1_4;
}
