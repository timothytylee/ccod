/*
 Copyright (C) 2004 Joshua J Baker
 See the file COPYING for copying permission.
 */

#ifndef __CSP_H__
#define __CSP_H__


#if defined(WIN32) || defined(_WIN32) 
#	ifdef BUILDING_CSP_DLL
#		define DECLAPI __declspec(dllexport)
#	else
#		define DECLAPI __declspec(dllimport)
#	endif
#else 
#   define DECLAPI extern
#endif



#if defined(__STRICT_ANSI__)
#undef __STRICT_ANSI__
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>




#ifdef __cplusplus
extern "C" {
#endif
	
	
DECLAPI struct 
Response_type
{
	int p_buffer;
	void (*SetBuffer)(int);
	int (*GetBuffer)();
	void (*SetContentType)(const char *contype);
	char *(*GetContentType)();
	void (*SetStatus)(const char *status);
	char *(*GetStatus)();
	void (*AddHeader)(const char *header, const char *value);
	void (*Flush)();
	void (*Write)(const char *fmt, ...);
	void (*Clear)();
	void (*End)();
	void (*Redirect)(const char *location);
	void (*BinaryWrite)(const unsigned char *buf, int bufsize);
	struct Cookies_type2
	{
		void (*Set)(const char *key, const char *value);
		char *(*Get)(const char *key);
		int (*Exists)(const char *key);		
		int (*GetHasKeys)(const char *key);
		void (*SetKey)(const char *key, const char *skey, const char *value);
		char *(*GetKey)(const char *key, const char *skey);
		void (*SetExpires)(const char *key, time_t date);
		time_t (*GetExpires)(const char *key);
		void (*SetDomain)(const char *key, const char *domain);
		char *(*GetDomain)(const char *key);
		void (*SetPath)(const char *key, const char *path);
		char *(*GetPath)(const char *key);
		void (*SetSecure)(const char *key, int secure);
		int (*GetSecure)(const char *key);
	} Cookies;
} Response;

DECLAPI struct 
Request_type
{
	struct QueryString_type
	{
		char *qstring;
		int (*GetCount)();
		int Count;
		char *(*Get)(const char *key);
		char *(*Key)(int index);
		char *(*Value)(int index);
		int (*Exists)(const char *key);
	} QueryString;
	struct Form_type
	{
		char *fstring;
		int (*GetCount)();
		int Count;
		char *(*Get)(const char *key);
		char *(*Key)(int index);
		char *(*Value)(int index);
		int (*Exists)(const char *key);
	} Form;
	struct ServerVariables_type
	{
		int (*GetCount)();
		int Count;
		char *(*Get)(const char *key);
		char *(*Key)(int index);
		char *(*Value)(int index);
		int (*Exists)(const char *key);
	} ServerVariables;
	struct Cookies_type1
	{
		char *(*Get)(const char *key);
		int (*Exists)(const char *key);		
		int (*GetHasKeys)(const char *key);
		char *(*GetKey)(const char *key, const char *skey);
	} Cookies;
	int (*GetTotalBytes)();
	int (*BinaryRead)(void *buf, int size);
	char *(*Get)(const char *key);
	int (*Exists)(const char *key);
} Request;

DECLAPI struct 
Server_type
{
	char *(*URLEncode)(const char *str);
	char *(*HTMLEncode)(const char *str);
	char *(*MapPath)(const char *str);
	int (*GetScriptTimeout)();
	void (*SetScriptTimeout)(int NumSeconds);

} Server;

DECLAPI struct 
Session_type
{
	void (*Abandon)();
	void (*SetTimeout)(int minutes);
	int (*GetTimeout)();
	char *(*GetSessionID)();
	struct Contents_type
	{
		int (*GetCount)();
		char *(*Get)(const char *key);
		void (*Set)(const char *key, const char *value);
		char *(*Key)(int index);
		char *(*Value)(int index);
		int (*Exists)(const char *key);
		void (*Remove)(const char *key);
		void (*RemoveAll)();
	} Contents;
	
	int (*GetCount)();
	char *(*Get)(const char *key);
	void (*Set)(const char *key, const char *value);
	char *(*Key)(int index);
	char *(*Value)(int index);
	int (*Exists)(const char *key);
	void (*Remove)(const char *key);
	void (*RemoveAll)();
} Session;



DECLAPI int initcsp();
DECLAPI int deinitcsp();


#ifndef IGNORE_CSP_ENTRY
static int argc;
static char **argv;
void cspmain();
int 
main(int argcin, char **argvin)
{
	argc = argcin;
	argv = argvin;
	initcsp();
	cspmain();
	deinitcsp();
	return 0;
}
#endif



#ifdef __cplusplus
}
#endif




#endif
