/*
 *  A JNI wrapper for the Hamlib library (https://github.com/Hamlib/Hamlib)
 *  Copyright (c) 2025-2026 by Dr Christos Bohoris
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
 
#include <jni.h>
#include <hamlib/rig.h>
#include <string.h>
#include <stdio.h>

#ifndef RIG_PATH_MAX
#  ifdef HAMLIB_FILPATHLEN
#    define RIG_PATH_MAX HAMLIB_FILPATHLEN
#  elif defined(FILPATHLEN)
#    define RIG_PATH_MAX FILPATHLEN
#  else
#    define RIG_PATH_MAX 128   /* fallback */
#  endif
#endif

// Simple logging macro for C - replace with your logging system if available
#define LOGGER_INFO(fmt, ...) fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__)
#define LOGGER_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

static RIG *rig = NULL;
static JavaVM *gVM = NULL;
static jobject gCallback = NULL;

// Helper: Throw RigConnectionException from native code
static void throwRigConnectionException(JNIEnv *env, const char* message) {
    jclass exceptionClass = (*env)->FindClass(env, "uk/co/connectina/picondo/model/hamlib/HamlibRig$RigConnectionException");
    if (exceptionClass != NULL) {
        (*env)->ThrowNew(env, exceptionClass, message);
    }
}

// Helper: Check if rig is valid and connected
static int checkRigConnection(JNIEnv *env) {
    if (!rig) {
        throwRigConnectionException(env, "Rig not initialized");
        return 0;
    }
    return 1;
}

// Helper: convert jstring → const char*
static const char* jstringToNative(JNIEnv *env, jstring jstr) {
    return jstr ? (*env)->GetStringUTFChars(env, jstr, NULL) : NULL;
}

// Helper: validate frequency range for safety
static int isFrequencyValid(freq_t freq) {
    // Basic sanity checks - reject obviously invalid frequencies
    if (freq <= 0 || freq > 300000000000LL) { // 300 GHz upper limit
        return 0;
    }
    return 1;
}

// Helper: validate mode string
static rmode_t parseMode(const char* modeStr) {
    if (!modeStr) return RIG_MODE_NONE;
    
    // Convert common mode strings to Hamlib mode constants
    if (strcasecmp(modeStr, "USB") == 0) return RIG_MODE_USB;
    if (strcasecmp(modeStr, "LSB") == 0) return RIG_MODE_LSB;
    if (strcasecmp(modeStr, "CW") == 0) return RIG_MODE_CW;
    if (strcasecmp(modeStr, "CWR") == 0) return RIG_MODE_CWR;
    if (strcasecmp(modeStr, "AM") == 0) return RIG_MODE_AM;
    if (strcasecmp(modeStr, "FM") == 0) return RIG_MODE_FM;
    if (strcasecmp(modeStr, "RTTY") == 0) return RIG_MODE_RTTY;
    if (strcasecmp(modeStr, "RTTYR") == 0) return RIG_MODE_RTTYR;
    if (strcasecmp(modeStr, "PSK") == 0) return RIG_MODE_PSK;
    if (strcasecmp(modeStr, "PSKR") == 0) return RIG_MODE_PSKR;
    
    return RIG_MODE_NONE;
}

JNIEXPORT jint JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_init
  (JNIEnv *env, jobject obj,
   jint jmodel, jstring jport, jint jbaud,
   jint jdatabits, jint jstopbits, jstring jparity) 
{
    int model = (int) jmodel;
    const char *port = jstringToNative(env, jport);
    const char *parity = jstringToNative(env, jparity);

    rig_set_debug(RIG_DEBUG_NONE);
    rig = rig_init(model);
    if (!rig) {
        if (port) (*env)->ReleaseStringUTFChars(env, jport, port);
        if (parity) (*env)->ReleaseStringUTFChars(env, jparity, parity);
        return -1;
    }

    // Port path
    strncpy(rig->state.rigport.pathname, port, RIG_PATH_MAX - 1);

    // Serial settings
    rig->state.rigport.parm.serial.rate     = jbaud;
    rig->state.rigport.parm.serial.data_bits= jdatabits;
    rig->state.rigport.parm.serial.stop_bits= jstopbits;

    if (parity && strlen(parity) > 0) {
        switch (parity[0]) {
            case 'N': case 'n': rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE; break;
            case 'O': case 'o': rig->state.rigport.parm.serial.parity = RIG_PARITY_ODD;  break;
            case 'E': case 'e': rig->state.rigport.parm.serial.parity = RIG_PARITY_EVEN; break;
            default: rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE; break;
        }
    } else {
        rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE;
    }

    int ret = rig_open(rig);

    if (port) (*env)->ReleaseStringUTFChars(env, jport, port);
    if (parity) (*env)->ReleaseStringUTFChars(env, jparity, parity);

    return ret;
}

JNIEXPORT jint JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_initWithHandshake
  (JNIEnv *env, jobject obj,
   jint jmodel, jstring jport, jint jbaud,
   jint jdatabits, jint jstopbits, jstring jparity,
   jboolean jdtr, jboolean jrts)
{
    int model = (int) jmodel;
    const char *port = jstringToNative(env, jport);
    const char *parity = jstringToNative(env, jparity);

    rig_set_debug(RIG_DEBUG_NONE);
    rig = rig_init(model);
    if (!rig) {
        if (port) (*env)->ReleaseStringUTFChars(env, jport, port);
        if (parity) (*env)->ReleaseStringUTFChars(env, jparity, parity);
        return -1;
    }

    // Port path
    strncpy(rig->state.rigport.pathname, port, RIG_PATH_MAX - 1);

    // Serial settings
    rig->state.rigport.parm.serial.rate     = jbaud;
    rig->state.rigport.parm.serial.data_bits= jdatabits;
    rig->state.rigport.parm.serial.stop_bits= jstopbits;

    // Parity setting
    if (parity && strlen(parity) > 0) {
        switch (parity[0]) {
            case 'N': case 'n': rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE; break;
            case 'O': case 'o': rig->state.rigport.parm.serial.parity = RIG_PARITY_ODD;  break;
            case 'E': case 'e': rig->state.rigport.parm.serial.parity = RIG_PARITY_EVEN; break;
            default: rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE; break;
        }
    } else {
        rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE;
    }

    // DTR/RTS individual signal control
    // Disable automatic handshaking so we can control DTR/RTS individually
    rig->state.rigport.parm.serial.handshake = RIG_HANDSHAKE_NONE;

    // Set individual DTR and RTS states
    // This is what actually matters for amateur radio applications
    rig->state.rigport.parm.serial.dtr_state = jdtr ? RIG_SIGNAL_ON : RIG_SIGNAL_OFF;
    rig->state.rigport.parm.serial.rts_state = jrts ? RIG_SIGNAL_ON : RIG_SIGNAL_OFF;

    int ret = rig_open(rig);

    if (port) (*env)->ReleaseStringUTFChars(env, jport, port);
    if (parity) (*env)->ReleaseStringUTFChars(env, jparity, parity);

    return ret;
}

JNIEXPORT void JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_cleanup
  (JNIEnv *env, jobject obj) {
    if (rig) {
        rig_close(rig);
        rig_cleanup(rig);
        rig = NULL;
    }
}

JNIEXPORT jdouble JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getFrequency
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return -1;
    
    freq_t freq;
    int ret = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to get frequency: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return -1;
    }
    return (jdouble)freq;
}

JNIEXPORT jstring JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getMode
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return (*env)->NewStringUTF(env, "");
    
    rmode_t mode;
    pbwidth_t width;
    int ret = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to get mode: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return (*env)->NewStringUTF(env, "");
    }
    return (*env)->NewStringUTF(env, rig_strrmode(mode));
}

JNIEXPORT jstring JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getVFO
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return (*env)->NewStringUTF(env, "");
    
    vfo_t vfo;
    int ret = rig_get_vfo(rig, &vfo);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to get VFO: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return (*env)->NewStringUTF(env, "");
    }
    return (*env)->NewStringUTF(env, rig_strvfo(vfo));
}

JNIEXPORT jdouble JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getPower
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return -1;
    
    value_t val;
    int ret = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &val);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to get power: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return -1;
    }
    return (jdouble)val.f;
}

// Set frequency with safety checks and exception handling
JNIEXPORT jint JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_setFrequency
  (JNIEnv *env, jobject obj, jdouble jfreq) {
    if (!checkRigConnection(env)) return -1;
    
    freq_t freq = (freq_t)jfreq;
    
    // Safety check: validate frequency
    if (!isFrequencyValid(freq)) {
        throwRigConnectionException(env, "Invalid frequency value");
        return -1;
    }
    
    // Basic range check using rig capabilities
    if (rig->caps) {
        const freq_range_t *range = rig->caps->rx_range_list1;
        int in_range = 0;
        
        // Check all frequency ranges
        for (int i = 0; range[i].startf != 0; i++) {
            if (freq >= range[i].startf && freq <= range[i].endf) {
                in_range = 1;
                break;
            }
        }
        
        if (!in_range) {
            throwRigConnectionException(env, "Frequency outside supported range");
            return -1;
        }
    }
    
    int ret = rig_set_freq(rig, RIG_VFO_CURR, freq);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to set frequency: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
    }
    return ret;
}

// Set mode with safety checks and exception handling
JNIEXPORT jint JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_setMode
  (JNIEnv *env, jobject obj, jstring jmode) {
    if (!checkRigConnection(env)) return -1;
    
    const char *modeStr = jstringToNative(env, jmode);
    if (!modeStr) {
        throwRigConnectionException(env, "Invalid mode string");
        return -1;
    }
    
    rmode_t mode = parseMode(modeStr);
    (*env)->ReleaseStringUTFChars(env, jmode, modeStr);
    
    if (mode == RIG_MODE_NONE) {
        throwRigConnectionException(env, "Unsupported mode");
        return -1;
    }
    
    // Check if mode is supported by checking rig capabilities
    if (rig->caps) {
        const freq_range_t *range = rig->caps->rx_range_list1;
        int mode_supported = 0;
        
        // Check if any frequency range supports this mode
        for (int i = 0; range[i].startf != 0; i++) {
            if (range[i].modes & mode) {
                mode_supported = 1;
                break;
            }
        }
        
        if (!mode_supported) {
            throwRigConnectionException(env, "Mode not supported by rig");
            return -1;
        }
    }
    
    // Use appropriate bandwidth for the mode
    pbwidth_t width = RIG_PASSBAND_NORMAL;
    int ret = rig_set_mode(rig, RIG_VFO_CURR, mode, width);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to set mode: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
    }
    return ret;
}

// Set frequency and mode together with exception handling
JNIEXPORT jint JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_setFrequencyAndMode
  (JNIEnv *env, jobject obj, jdouble jfreq, jstring jmode) {
    if (!checkRigConnection(env)) return -1;
    
    freq_t freq = (freq_t)jfreq;
    
    // Safety check: validate frequency
    if (!isFrequencyValid(freq)) {
        throwRigConnectionException(env, "Invalid frequency value");
        return -1;
    }
    
    const char *modeStr = jstringToNative(env, jmode);
    if (!modeStr) {
        throwRigConnectionException(env, "Invalid mode string");
        return -1;
    }
    
    rmode_t mode = parseMode(modeStr);
    (*env)->ReleaseStringUTFChars(env, jmode, modeStr);
    
    if (mode == RIG_MODE_NONE) {
        throwRigConnectionException(env, "Unsupported mode");
        return -1;
    }
    
    // Validate both frequency and mode using rig capabilities
    if (rig->caps) {
        const freq_range_t *range = rig->caps->rx_range_list1;
        int valid_combo = 0;
        
        // Check if frequency and mode combination is supported
        for (int i = 0; range[i].startf != 0; i++) {
            if (freq >= range[i].startf && freq <= range[i].endf && (range[i].modes & mode)) {
                valid_combo = 1;
                break;
            }
        }
        
        if (!valid_combo) {
            throwRigConnectionException(env, "Frequency/mode combination not supported");
            return -1;
        }
    }
    
    // Set frequency first, then mode
    int ret = rig_set_freq(rig, RIG_VFO_CURR, freq);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to set frequency: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return ret;
    }
    
    pbwidth_t width = RIG_PASSBAND_NORMAL;
    ret = rig_set_mode(rig, RIG_VFO_CURR, mode, width);
    if (ret != RIG_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to set mode: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
    }
    return ret;
}

// Get minimum supported frequency with exception handling
JNIEXPORT jdouble JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getMinFrequency
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return -1;
    
    if (!rig->caps) {
        throwRigConnectionException(env, "Rig capabilities not available");
        return -1;
    }
    
    const freq_range_t *range = rig->caps->rx_range_list1;
    if (range[0].startf != 0) {
        return (jdouble)range[0].startf;
    }
    return -1;
}

// Get maximum supported frequency with exception handling
JNIEXPORT jdouble JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getMaxFrequency
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return -1;
    
    if (!rig->caps) {
        throwRigConnectionException(env, "Rig capabilities not available");
        return -1;
    }
    
    const freq_range_t *range = rig->caps->rx_range_list1;
    freq_t max_freq = 0;
    
    // Find the highest end frequency across all ranges
    for (int i = 0; range[i].startf != 0; i++) {
        if (range[i].endf > max_freq) {
            max_freq = range[i].endf;
        }
    }
    
    return max_freq > 0 ? (jdouble)max_freq : -1;
}

// Get supported modes with exception handling
JNIEXPORT jobjectArray JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_getSupportedModes
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return NULL;
    
    if (!rig->caps) {
        throwRigConnectionException(env, "Rig capabilities not available");
        return NULL;
    }
    
    const freq_range_t *range = rig->caps->rx_range_list1;
    rmode_t all_modes = 0;
    
    // Collect all supported modes across all frequency ranges
    for (int i = 0; range[i].startf != 0; i++) {
        all_modes |= range[i].modes;
    }
    
    // Count supported modes
    int modeCount = 0;
    const char* modeNames[32]; // Max 32 modes
    
    if (all_modes & RIG_MODE_USB) modeNames[modeCount++] = "USB";
    if (all_modes & RIG_MODE_LSB) modeNames[modeCount++] = "LSB";
    if (all_modes & RIG_MODE_CW) modeNames[modeCount++] = "CW";
    if (all_modes & RIG_MODE_CWR) modeNames[modeCount++] = "CWR";
    if (all_modes & RIG_MODE_AM) modeNames[modeCount++] = "AM";
    if (all_modes & RIG_MODE_FM) modeNames[modeCount++] = "FM";
    if (all_modes & RIG_MODE_RTTY) modeNames[modeCount++] = "RTTY";
    if (all_modes & RIG_MODE_RTTYR) modeNames[modeCount++] = "RTTYR";
    if (all_modes & RIG_MODE_PSK) modeNames[modeCount++] = "PSK";
    if (all_modes & RIG_MODE_PSKR) modeNames[modeCount++] = "PSKR";
    
    // Create Java string array
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    if (stringClass == NULL) {
        throwRigConnectionException(env, "Failed to create string array");
        return NULL;
    }
    
    jobjectArray result = (*env)->NewObjectArray(env, modeCount, stringClass, NULL);
    if (result == NULL) {
        throwRigConnectionException(env, "Failed to allocate mode array");
        return NULL;
    }
    
    for (int i = 0; i < modeCount; i++) {
        jstring modeString = (*env)->NewStringUTF(env, modeNames[i]);
        if (modeString == NULL) {
            throwRigConnectionException(env, "Failed to create mode string");
            return NULL;
        }
        (*env)->SetObjectArrayElement(env, result, i, modeString);
        (*env)->DeleteLocalRef(env, modeString);
    }
    
    return result;
}

JNIEXPORT void JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_registerCallback
  (JNIEnv *env, jobject obj, jobject cb) {
    if (gCallback) {
        (*env)->DeleteGlobalRef(env, gCallback);
        gCallback = NULL;
    }
    if (cb) {
        gCallback = (*env)->NewGlobalRef(env, cb);
    }
}

JNIEXPORT void JNICALL Java_uk_co_connectina_picondo_model_hamlib_HamlibRig_pollOnceProtected
  (JNIEnv *env, jobject obj) {
    if (!checkRigConnection(env)) return;
    
    if (!gCallback) {
        throwRigConnectionException(env, "No callback registered for polling");
        return;
    }

    jclass cbCls = (*env)->GetObjectClass(env, gCallback);
    jmethodID mid = (*env)->GetMethodID(env, cbCls, "onRigUpdate",
                                        "(DLjava/lang/String;Ljava/lang/String;D)V");
    if (!mid) {
        throwRigConnectionException(env, "Callback method not found");
        return;
    }

    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    vfo_t vfo;
    value_t val;
    double freqHz = -1, power = -1;
    const char *modeStr = "", *vfoStr = "";
    int connection_failed = 0;

    // Try to get frequency
    int ret = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    if (ret == RIG_OK) {
        freqHz = freq;
    } else {
        connection_failed = 1;
        LOGGER_INFO("Failed to get frequency during polling: %s", rigerror(ret));
    }

    // Try to get mode if frequency succeeded
    if (!connection_failed) {
        ret = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
        if (ret == RIG_OK) {
            modeStr = rig_strrmode(mode);
        } else {
            connection_failed = 1;
        }
    }

    // Try to get VFO if mode succeeded
    if (!connection_failed) {
        ret = rig_get_vfo(rig, &vfo);
        if (ret == RIG_OK) {
            vfoStr = rig_strvfo(vfo);
        }
        // VFO failure is not critical, continue
    }

    // Try to get power level
    if (!connection_failed) {
        ret = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &val);
        if (ret == RIG_OK) {
            power = val.f;
        }
        // Power failure is not critical, continue
    }

    // If critical operations failed, throw exception
    if (connection_failed) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Rig communication failed during polling: %s", rigerror(ret));
        throwRigConnectionException(env, error_msg);
        return;
    }

    jstring jmode = (*env)->NewStringUTF(env, modeStr ? modeStr : "");
    jstring jvfo  = (*env)->NewStringUTF(env, vfoStr ? vfoStr : "");

    if (jmode == NULL || jvfo == NULL) {
        throwRigConnectionException(env, "Failed to create strings for callback");
        if (jmode) (*env)->DeleteLocalRef(env, jmode);
        if (jvfo) (*env)->DeleteLocalRef(env, jvfo);
        return;
    }

    (*env)->CallVoidMethod(env, gCallback, mid, (jdouble)freqHz, jmode, jvfo, (jdouble)power);

    (*env)->DeleteLocalRef(env, jmode);
    (*env)->DeleteLocalRef(env, jvfo);
    
    // Check for any Java exceptions that occurred during callback
    if ((*env)->ExceptionCheck(env)) {
        throwRigConnectionException(env, "Exception occurred in polling callback");
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    gVM = vm;
    return JNI_VERSION_1_6;
}
