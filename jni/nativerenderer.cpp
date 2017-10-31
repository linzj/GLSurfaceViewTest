#include <jni.h>
#include <stdlib.h>
#include <pthread.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <arm_neon.h>

#define TAG "LINZJ"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static pthread_t* gRenderThread;
static volatile bool gRenderThreadShouldEnd;
static pthread_mutex_t gFrameMutex = PTHREAD_MUTEX_INITIALIZER;
static volatile size_t gFrameAvailableCount = 0;
static pthread_cond_t gFrameCond = PTHREAD_COND_INITIALIZER;

static void waitTillFrameAvailable() {
    LOGD("waitTillFrameAvailable: %u", gFrameAvailableCount);
    pthread_mutex_lock(&gFrameMutex);
wait_loop:
    if (gFrameAvailableCount > 0) {
        --gFrameAvailableCount;
        if (gFrameAvailableCount > 0) {
            LOGE("gFrameAvailableCount bad value: %u\n", gFrameAvailableCount);
            abort();
        }
        goto exit;
    }
    pthread_cond_wait(&gFrameCond, &gFrameMutex);
    goto wait_loop;
exit:
    pthread_mutex_unlock(&gFrameMutex);
}

static void* rendererThread(void* w) {
    ANativeWindow* window = static_cast<ANativeWindow*>(w);
    while (true) {
        ANativeWindow_Buffer windowBuffer;
        waitTillFrameAvailable();
        if (gRenderThreadShouldEnd)
            return nullptr;
        int32_t err;
        err = ANativeWindow_lock(window, &windowBuffer, nullptr);
        if (err) {
            LOGE("failed to lock window, err:%d", err);
            return nullptr;
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
        err = ANativeWindow_unlockAndPost(window);
        if (err) {
            LOGE("ANativeWindow_unlockAndPost: err: %d", err);
        }
    }
    return nullptr;
}

static void doConsumeOneFrame() {
    pthread_mutex_lock(&gFrameMutex);
    gFrameAvailableCount++;
    LOGD("doConsumeOneFrame: %u", gFrameAvailableCount);
    pthread_cond_signal(&gFrameCond);
    pthread_mutex_unlock(&gFrameMutex);
}

static void nativeOnSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
   if (gRenderThread) {
       gRenderThreadShouldEnd = true;
       void* ret;
       doConsumeOneFrame();
       pthread_join(*gRenderThread, &ret);
       pthread_detach(*gRenderThread);
       delete gRenderThread;
       gRenderThread = nullptr;
       gFrameAvailableCount = 0;
   }
   ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
   int32_t err = ANativeWindow_setBuffersGeometry(window, 640, 480, WINDOW_FORMAT_RGBA_8888);
   if (err) {
       LOGD("nativeOnSurfaceCreated setBuffersGeometry: err: %d\n", err);
   } else {
       LOGD("nativeOnSurfaceCreated");
       ANativeWindow_acquire(window);
       pthread_t* newThread = new pthread_t;
       gRenderThreadShouldEnd = false;
       int err = pthread_create(newThread, nullptr, rendererThread, window);
       if (err) {
           LOGE("failed to create renderer thread: %d", err);
           abort();
       } else {
           gRenderThread = newThread;
       }
   }
}

static void nativeConsumeOneFrame(JNIEnv* env, jobject thiz) {
    doConsumeOneFrame();
}

static const char* gRendererClass = "com/alibaba/uc/GLSurfaceViewTestRenderer";

static JNINativeMethod gMethods[] = {
    {"nOnSurfaceCreated", "(Landroid/view/Surface;)V",
     (void*)nativeOnSurfaceCreated},
    {"nConsumeOneFrame", "()V",
     (void*)nativeConsumeOneFrame},
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
