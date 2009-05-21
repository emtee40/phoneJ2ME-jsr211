/*
 * 
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation. 
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt). 
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA 
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions. 
 */

/**
 * @file
 * @brief Content Handler RegistryStore native functions implementation
 */


#include <jni.h>
#include <jsr211_registry.h>
#include <jsr211_result.h>

#include <memory.h>
#include <stdlib.h>

// initialization counter
static jint initialized = 0;

/** Classes fields */

/** com.sun.midp.content.ContentHandlerImpl internal fields */
static jfieldID chImplId = 0;       // ID,
             // also it is an uninitiaized state indicator

static jfieldID chImplSuiteId;      // storageId    : int
static jfieldID chImplClassname;    // classname    : String
static jfieldID chImplregMethod;    // registrationMethod : int
static jfieldID chImplTypes;        // types        : String[]
static jfieldID chImplSuffixes;     // suffixes     : String[]
static jfieldID chImplActions;      // actions      : String[]
static jfieldID chImplActionnames;  // actionnames  : ActionNameMap[]
static jfieldID chImplAccesses;     // accesses     : String[]

/** javax.microedition.content.ActionNameMap fields */
static jfieldID anMapLocale;        // locale
static jfieldID anMapActionnames;   // [] actionnames


/**  JSR 211 exception messages */

#define DECLARE_MSG(n, s) static const char n[] = s;
#define EXCEPTION_MSG(s) s

DECLARE_MSG(fcFindHandler,   "Could not find ContentHandler for field value")
DECLARE_MSG(fcFindInvocation,   "Invocation not found")
DECLARE_MSG(fcFindForSuite,   "Could not find ContentHandler for suite ID")
DECLARE_MSG(fcFindGetValues, "Could not read ContentHandler values");
DECLARE_MSG(fcGetHandlerField, "Could not read ContentHandler fields");
DECLARE_MSG(fcGetHandler, "Could not find ContentHandler");
DECLARE_MSG(fcHandlerByURL, "Could not find ContentHandler for URL");
DECLARE_MSG(fcUnexpectedFinilize,   "Unexpected RegistryStore finalization")
DECLARE_MSG(fcNoClassFields,   "Could not initialize JSR211 class fields")
DECLARE_MSG(fcRegister,   "Could not register ContentHandler")



/** 
 * Throw a Java exception by name. Similar to SignalError. 
 */
static void JNICALL 
JNU_ThrowByName(JNIEnv *env, const char *name, const char *msg)
{
    jclass cls = (*env)->FindClass(env, name);

    if (cls != 0) /* Otherwise an exception has already been thrown */
        (*env)->ThrowNew(env, cls, msg);

    /* It's a good practice to clean up the local references. */
    (*env)->DeleteLocalRef(env, cls);
}

static void JNICALL 
JNU_ThrowInternalError(JNIEnv *env, const char *msg)
{
    JNU_ThrowByName(env, "java/lang/InternalError", msg);
}


typedef struct JSR211_CONTENT_HANDLER_ {
    const jchar* id;
	jstring j_id;

    int suite_id;

    const jchar* class_name;
	jstring j_class_name;

    int flag; 

    const jchar** types;
	jstring* j_types;
	int nTypes;

    const jchar** suffixes;
	jstring* j_suffixes;
	int nSuffixes;

    const jchar** actions;
	jstring* j_actions;
	int nActions;

    const jchar** locales;
	jstring* j_locales;
	int nLocales; 

    const jchar** action_names;
	jstring* j_action_names;
	int nActionNames; 

    const jchar** accesses;
	jstring* j_accesses;
	int nAccesses;
} JSR211_CONTENT_HANDLER;


static jstring result2string(JNIEnv *env, JSR211_RESULT_BUFFER buffer){
	if (jsr211_get_result_data_length(buffer)) {
		return (*env)->NewString(env, jsr211_get_result_data(buffer), jsr211_get_result_data_length(buffer));
	} else {
		return NULL;
	}
}

static void cleanStringArray(JNIEnv *env, jstring* j_strings, const jchar** strings, int n) {
	int i = 0;
	if (j_strings && strings){
		for (;i<n;++i){
			(*env)->ReleaseStringChars(env, j_strings[i], strings[i]);
		}
	}
	if (j_strings) free(j_strings);
	if (strings) free((void*)strings);
}


static void cleanHandlerData(JNIEnv *env, JSR211_CONTENT_HANDLER* handler) {
	(*env)->ReleaseStringChars(env, handler->j_id, handler->id);
	(*env)->ReleaseStringChars(env, handler->j_class_name, handler->class_name);

	cleanStringArray(env,handler->j_types,handler->types,handler->nTypes);
	cleanStringArray(env,handler->j_suffixes,handler->suffixes,handler->nSuffixes);
	cleanStringArray(env,handler->j_actions,handler->actions,handler->nActions);
	cleanStringArray(env,handler->j_locales,handler->locales,handler->nLocales);
	cleanStringArray(env,handler->j_action_names,handler->action_names,handler->nActionNames);
	cleanStringArray(env,handler->j_accesses,handler->accesses,handler->nAccesses);
}

/**
 * Retrieves fields IDs for classes:
 * <BR> <code>com.sun.midp.content.ContentHandlerImpl</code> and
 * <BR> <code>javax.microedition.content.ActionNameMap</code>
 * @return JNI_OK - if successfully get all fields, JNI_ERR - otherwise
 */
static int initializeFields(JNIEnv *env) {
    static const char* STRING_TYPE = "Ljava/lang/String;";
    static const char* S_ARRAY_TYPE = "[Ljava/lang/String;";
    static const char* CONTENT_HANDLER_CLASS = 
                            "com/sun/j2me/content/ContentHandlerImpl";

    static const char* ANM_ARRAY_TYPE = 
                            "[Ljavax/microedition/content/ActionNameMap;";
    static const char* ANM_CLASS_NAME = 
                            "javax/microedition/content/ActionNameMap";
    int ret;    // returned result code

    do {
        // 1. initialize ContentHandlerImpl fields
        jobject clObj = (*env)->FindClass(env, CONTENT_HANDLER_CLASS);
		if (!clObj)  {
			ret = JNI_ERR;
			break;
		}
        chImplId =          (*env)->GetFieldID(env, clObj, "ID", STRING_TYPE);
        chImplSuiteId =     (*env)->GetFieldID(env, clObj,  "storageId", "I");
        chImplClassname =   (*env)->GetFieldID(env, clObj,  "classname", STRING_TYPE);
        chImplregMethod =   (*env)->GetFieldID(env, clObj,  "registrationMethod", "I");
        chImplTypes =       (*env)->GetFieldID(env, clObj,  "types", S_ARRAY_TYPE);
        chImplSuffixes =    (*env)->GetFieldID(env, clObj,  "suffixes", S_ARRAY_TYPE);
        chImplActions =     (*env)->GetFieldID(env, clObj,  "actions", S_ARRAY_TYPE);
        chImplActionnames = (*env)->GetFieldID(env, clObj,  "actionnames", 
                                                            ANM_ARRAY_TYPE);
        chImplAccesses =    (*env)->GetFieldID(env, clObj,  "accessRestricted", 
                                                            S_ARRAY_TYPE);
    
        if (chImplId == 0 || chImplSuiteId == 0 || chImplClassname == 0 || 
            chImplregMethod == 0 || chImplTypes == 0 || 
            chImplSuffixes == 0 || chImplActions == 0 || 
            chImplActionnames == 0 || chImplAccesses == 0) {

            ret = JNI_ERR;
            break;
        }
    
        // 2. initialize ActionName fields
        clObj = (*env)->FindClass(env, ANM_CLASS_NAME);  // clObj = ActionNameMap class
        if (!clObj) {
            ret = JNI_ERR;
            break;
        }
    
        anMapLocale =       (*env)->GetFieldID(env, clObj,  "locale", STRING_TYPE);
        anMapActionnames =  (*env)->GetFieldID(env, clObj,  "actionnames", S_ARRAY_TYPE);
    
        if (anMapLocale == 0 || anMapActionnames == 0) {
            ret = JNI_ERR;
            break;
        }
        
        ret = JNI_OK;   // that's all right.
    } while (0);

    return ret;
}

/**
 * Fetch a JNI String array object into the string array.
 *
 * @param arrObj JNI Java String object handle
 * @param arrPtr the String array pointer for values storing
 * @return number of retrieved strings
 * <BR>JNI_ENOMEM - indicates memory allocation error
 */
static int getStringArray(JNIEnv *env, jobjectArray arrObj, jstring** j_arrPtr, const jchar*** arrPtr) {
    int i, n = 0;
	jstring* jarr;
	const jchar** arr;

	if (!arrObj) return 0;
    n = (*env)->GetArrayLength(env, arrObj);

	if (!n) return 0;

    arr = *arrPtr = malloc(sizeof(jchar*) * n);
    if (arr == NULL) {
        return JNI_ENOMEM;
    }

	jarr = *j_arrPtr = malloc(sizeof(jstring) * n);
    if (jarr == NULL) {
        return JNI_ENOMEM;
    }

    for (i = 0; i < n; i++) {
        jarr[i] = (jstring)((*env)->GetObjectArrayElement(env, arrObj, i));
		if (!jarr[i]) return JNI_ENOMEM;

		arr[i] = (*env)->GetStringChars (env, jarr[i], NULL);
		if (!arr[i]) return JNI_ENOMEM;
    }
	
    return n;
}


/**
 * Fills <code>MidpString</code> arrays for locales and action_maps from 
 * <code>ActionMap</code> objects.
 * <BR>Length of <code>actionnames</code> array must be the same as in
 * <code>act_num</code> parameter for each element of <code>ActionMap</code>
 * array.
 *
 * @param o <code>ActionMap[]</code> object 
 * @param handler pointer on <code>JSR211_content_handler</code> structure
 * being filled up
 * @return JNI_OK - if successfully get all fields, 
 * JNI_ERR or JNI_ENOMEM - otherwise
 */
static int fillActionMap(JNIEnv *env, jobjectArray actionNamesMap,  JSR211_CONTENT_HANDLER* handler) {
    int ret = JNI_OK;   // returned result
    int len;            // number of locales
    int i, j;
    int n = handler->nActions;  // number of actions
    const jchar** locs = NULL;   // fetched locales
    const jchar** nams = NULL;   // fetched action names

    jstring* j_locs = NULL;   
    jstring* j_nams = NULL;   


	if (!actionNamesMap) return 0;

    len = (int)(*env)->GetArrayLength(env,actionNamesMap);

	if (!len) return 0;

    do {
        // allocate buffers
        locs = handler->locales = malloc(sizeof(jchar*) * len);
        if (handler->locales == NULL) {
            ret = JNI_ENOMEM;
            break;
        }
        j_locs = handler->j_locales = malloc(sizeof(jstring) * len);
        if (handler->j_locales == NULL) {
            ret = JNI_ENOMEM;
            break;
        }
        handler->nLocales = len;

        nams = handler->action_names = malloc(sizeof(jchar*) * len * n);
        if (handler->action_names == NULL) {
            ret = JNI_ENOMEM;
            break;
        }
        j_nams = handler->j_action_names = malloc(sizeof(jstring) * len * n);
        if (handler->j_action_names == NULL) {
            ret = JNI_ENOMEM;
            break;
        }
		handler->nActionNames = len * n;

        for (i = 0; i < len && ret == JNI_OK; i++) {
			jstring str;
			jobject arr;
            jobject map = (*env)->GetObjectArrayElement(env,actionNamesMap,i);
			str = (jstring)(*j_locs = (*env)->GetObjectField(env, map, anMapLocale));
			if (!j_locs++){
				ret = JNI_ENOMEM;
				break;
			}
			*locs = (*env)->GetStringChars (env, (jstring)str, NULL);
			if (!locs++){
				ret = JNI_ENOMEM;
				break;
			}
            arr = (*env)->GetObjectField(env, map, anMapActionnames);
            for (j = 0; j < n; j++) {
                str = (jstring)(*j_nams = (*env)->GetObjectArrayElement(env, arr, j));
                if (!j_nams++) {
                    ret = JNI_ENOMEM;
                    break;
                }
				*nams = (*env)->GetStringChars (env, (jstring)str, NULL);
                if (!nams++) {
                    ret = JNI_ENOMEM;
                    break;
                }
            }
		}
     } while (0);
        
    return ret;
}


/**
 * Fills <code>JSR211_content_handler</code> structure with data from 
 * <code>ContentHandlerImpl</code> object.
 * <BR>Fields <code>ID, storageId</code> and <code>classname</code>
 * are mandatory. They must have not 0 length.
 *
 * @param o <code>ContentHandlerImpl</code> object
 * @param handler pointer on <code>JSR211_content_handler</code> structure
 * to be filled up
 * @return JNI_OK - if successfully get all fields, 
 * JNI_ERR or JNI_ENOMEM - otherwise
 */
static int fillHandlerData(JNIEnv *env, jobject jhandler, JSR211_CONTENT_HANDLER* handler) {
    int ret;    // returned result code
	jobjectArray objArr;

	memset(handler,0,sizeof(JSR211_CONTENT_HANDLER));

    do {
        // ID
        handler->j_id = (jstring)(*env)->GetObjectField(env, jhandler, chImplId);

        // check mandatory field
        if (!handler->j_id || !(*env)->GetStringLength(env, handler->j_id)) {
            ret = JNI_ERR;
            break;
        }
		handler->id = (*env)->GetStringChars (env, handler->j_id, NULL);

        // suiteId
        handler->suite_id = (*env)->GetIntField(env, jhandler, chImplSuiteId);

        // classname
        handler->j_class_name = (jstring)(*env)->GetObjectField(env, jhandler, chImplClassname);
		if (!handler->j_class_name){
            ret = JNI_ERR;
            break;
		} 
		handler->class_name = (*env)->GetStringChars (env, handler->j_class_name, NULL);
		

        // flag
        handler->flag = (*env)->GetIntField(env, jhandler, chImplregMethod);

        // types
        objArr = (jobjectArray)(*env)->GetObjectField(env, jhandler,	chImplTypes);

	    handler->nTypes = getStringArray(env, objArr, &(handler->j_types), &(handler->types));
		if (handler->nTypes < 0) {
			ret = handler->nTypes;
			handler->nTypes = 0;
			break;
		}

        // suffixes        
        objArr = (jobjectArray)(*env)->GetObjectField(env, jhandler, chImplSuffixes);

        handler->nSuffixes = getStringArray(env, objArr, &(handler->j_suffixes), &(handler->suffixes));
        if (handler->nSuffixes < 0) {
            ret = handler->nSuffixes;
			handler->nSuffixes = 0;
            break;
        }

        // actions
        objArr = (jobjectArray)(*env)->GetObjectField(env, jhandler, chImplActions);

        handler->nActions = getStringArray(env, objArr, &(handler->j_actions), &(handler->actions));
        if (handler->nActions < 0) {
            ret = handler->nActions;
			handler->nActions = 0;
            break;
        }


        if (handler->nActions > 0) {

			// action names
			objArr = (jobjectArray)(*env)->GetObjectField(env, jhandler, chImplActionnames);

			ret = fillActionMap(env, objArr, handler);
            if (JNI_OK != ret) {
                break;
            }
		}

        // accesses
        objArr = (jobjectArray)(*env)->GetObjectField(env, jhandler, chImplAccesses);
        handler->nAccesses = getStringArray(env, objArr, &(handler->j_accesses), &(handler->accesses));
        if (handler->nAccesses < 0) {
            ret = handler->nAccesses;
			handler->nAccesses = 0;
            break;
        }

        ret = JNI_OK;

    } while (0);

    return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    findHandler0
 * Signature: (Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_findHandler0
  (JNIEnv *env, jobject regStore, jstring callerId, jint searchBy, jstring value){
	jstring ret;
	const jchar* caller_id_ = callerId !=NULL ? (*env)->GetStringChars(env,callerId, NULL) : NULL;
    const jchar* value_ = (const jchar*)(*env)->GetStringChars(env, value, NULL);
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
    jint status = jsr211_find_handler((const jchar*)caller_id_, searchBy, value_, (JSR211_RESULT_CHARRAY)result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcFindHandler);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}

/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    forSuite0
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_forSuite0
  (JNIEnv *env, jobject regStore, int suiteId){
	jstring ret;
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
	jint status = jsr211_find_for_suite(suiteId, result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcFindForSuite);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    getValues0
 * Signature: (Ljava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_getValues0
  (JNIEnv *env, jobject regStore, jstring callerId, jint searchBy){
	jstring ret;
    const jchar* caller_id_ = callerId !=NULL ? (*env)->GetStringChars(env,callerId, NULL) : NULL;
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
	jint status = jsr211_get_all(caller_id_, searchBy, result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcFindGetValues);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    getHandler0
 * Signature: (Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_getHandler0
  (JNIEnv *env, jobject regStore, jstring callerId, jstring id, jint mode){
	jstring ret;
	const jchar* caller_id_ = callerId !=NULL ? (*env)->GetStringChars(env,callerId, NULL) : NULL;
	const jchar* _id_ = (*env)->GetStringChars(env,id, NULL);
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
	jint status = jsr211_get_handler((const jchar*)caller_id_, (const jchar*)_id_, mode, result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcGetHandler);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    loadFieldValues0
 * Signature: (Ljava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_loadFieldValues0
  (JNIEnv *env, jobject regStore, jstring handlerId, jint fieldId){
	jstring ret;
	const jchar* handler_id_ = (*env)->GetStringChars(env,handlerId, NULL);
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
	jint status = jsr211_get_handler_field((const jchar*)handler_id_, fieldId, result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcGetHandlerField);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    getByURL0
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_sun_j2me_content_RegistryStore_getByURL0
  (JNIEnv *env, jobject regStore, jstring callerId, jstring url, jstring action){
	jstring ret;
	const jchar* caller_id_ = callerId !=NULL ? (*env)->GetStringChars(env,callerId, NULL) : NULL;
	const jchar* url_ = (*env)->GetStringChars(env,url, NULL);
	const jchar* action_ = (*env)->GetStringChars(env, action, NULL);
	JSR211_RESULT_BUFFER result = jsr211_create_result_buffer();
	jint status = jsr211_handler_by_URL((const jchar*)caller_id_, (const jchar*)url_, (const jchar*)action_,result);
	if (status!=JSR211_OK){
		JNU_ThrowInternalError(env, fcHandlerByURL);
		return NULL;
	}
	ret = result2string(env,result);
	jsr211_release_result_buffer(result);
	return ret;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    launch0
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_sun_j2me_content_RegistryStore_launch0
  (JNIEnv *env, jobject regStore, jstring handlerId){
	return JSR211_LAUNCH_ERR_NOTSUPPORTED;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    init
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_j2me_content_RegistryStore_init
  (JNIEnv *env, jobject regStore){

   if (!initialized) {
	   if (JSR211_OK != jsr211_initialize()) return JNI_FALSE;
   } else {
	   //if (JSR211_OK != jsr211_check_internal_handlers()) return JNI_FALSE;
   }
   
   initialized++;
   return JNI_TRUE;
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_sun_j2me_content_RegistryStore_finalize
  (JNIEnv *env, jobject regStore){

	if (!initialized){
		JNU_ThrowInternalError(env, fcUnexpectedFinilize);
	} else 	if (!--initialized) {
		jsr211_finalize();
	}
}


/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    register0
 * Signature: (Lcom/sun/j2me/content/ContentHandlerImpl;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_j2me_content_RegistryStore_register0
  (JNIEnv *env, jobject regStore, jobject contentHandler){

    int res = JNI_OK;
    JSR211_CONTENT_HANDLER handler;

    do {
        if (chImplId == 0) {
            res = initializeFields(env);
            if (res != JNI_OK) {
				JNU_ThrowInternalError(env, fcNoClassFields);
                break;
            }
        }

        res = fillHandlerData(env, contentHandler, &handler);
        if (res != JNI_OK) {
			JNU_ThrowInternalError(env, fcGetHandlerField);
            break;
        }

		res = jsr211_register_handler((const jchar*)handler.id, 
											  handler.suite_id,
											  (const jchar*)handler.class_name,
											  handler.flag,
											  handler.nTypes,
											  (const jchar**)handler.types,
											  handler.nSuffixes,
											  (const jchar**)handler.suffixes,
											  handler.nActions,
											  (const jchar**)handler.actions,
											  handler.nLocales,
											  (const jchar**)handler.locales,
											  (const jchar**)handler.action_names,
											  handler.nAccesses,
											  (const jchar**)handler.accesses);

		if (res!=JSR211_OK){
			JNU_ThrowInternalError(env, fcRegister);
		}

    } while (0);
    

    
    cleanHandlerData(env,&handler);

    return (res==JSR211_OK ? JNI_TRUE: JNI_FALSE);
}



/*
 * Class:     com_sun_j2me_content_RegistryStore
 * Method:    unregister0
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_j2me_content_RegistryStore_unregister0
  (JNIEnv *env, jobject regStore, jstring handlerId){
	int res;
	const jchar* id = (*env)->GetStringChars(env,handlerId, NULL);
	if (!id) return JNI_FALSE;

    res = jsr211_unregister_handler((const jchar*)id); 

	(*env)->ReleaseStringChars(env, handlerId, id);

	return (res==JSR211_OK? JNI_TRUE: JNI_FALSE);
}