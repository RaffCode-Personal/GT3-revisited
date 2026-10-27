#include "platform/shared_storage.h"

#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#endif

namespace gt3r::platform {

#if defined(SDL_PLATFORM_ANDROID)
namespace {

// Calls GT3Activity.<name>() (signature `sig`); `call` gets the env, activity and method.
template <class F>
bool call_activity(const char* name, const char* sig, F&& call) {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return false;
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = cls ? env->GetMethodID(cls, name, sig) : nullptr;
    bool ok = m != nullptr;
    if (m) call(env, activity, m);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ok = false;
    }
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ok;
}

void SDLCALL on_permission(void*, const char*, bool) {}

} // namespace

bool shared_storage_supported() { return true; }

std::string shared_storage_dir() {
    std::string out;
    // GT3Activity.sharedStorageDir(): the folder's path, or null when not mounted.
    call_activity("sharedStorageDir", "()Ljava/lang/String;", [&](JNIEnv* env, jobject a, jmethodID m) {
        auto s = static_cast<jstring>(env->CallObjectMethod(a, m));
        if (env->ExceptionCheck() || !s) return;
        if (const char* c = env->GetStringUTFChars(s, nullptr)) {
            out = c;
            env->ReleaseStringUTFChars(s, c);
        }
        env->DeleteLocalRef(s);
    });
    if (!out.empty() && out.back() != '/') out += '/';
    return out;
}

bool shared_storage_access() {
    bool yes = false;
    call_activity("hasSharedStorageAccess", "()Z",
                  [&](JNIEnv* env, jobject a, jmethodID m) { yes = env->CallBooleanMethod(a, m); });
    return yes;
}

void request_shared_storage_access() {
    if (SDL_GetAndroidSDKVersion() < 30) {
        SDL_RequestAndroidPermission("android.permission.WRITE_EXTERNAL_STORAGE", on_permission, nullptr);
        return;
    }
    call_activity("requestSharedStorageAccess", "()V",
                  [](JNIEnv* env, jobject a, jmethodID m) { env->CallVoidMethod(a, m); });
}
#else
bool shared_storage_supported() { return false; }
std::string shared_storage_dir() { return {}; }
bool shared_storage_access() { return false; }
void request_shared_storage_access() {}
#endif

} // namespace gt3r::platform
