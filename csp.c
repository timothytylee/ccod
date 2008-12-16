/*
 Copyright (C) 2004 Joshua J Baker
 See the file COPYING for copying permission.
 */

#define IGNORE_CSP_ENTRY
#define BUILDING_CSP_DLL
#include "csp.h"



#if defined(WIN32) || defined(_WIN32)
	#include <fcntl.h>
    #include <io.h>
#else
	#include <pthread.h>
#endif

#if defined(__CYGWIN__)
	#include <sys/time.h>
#endif

#if defined(WIN32) || defined(_WIN32)
#ifdef IGNORE_CSP_ENTRY
	#include <windows.h>
#endif
	#define mkdir2(name) mkdir(name)
#else
	#define mkdir2(name) mkdir(name,0755)
#endif




struct Response_type Response;
struct Request_type Request;
struct Server_type Server;
struct Session_type Session;



static char *response_cookies_get(const char *key);
static char *response_cookies_getkey(const char *key, const char *skey);
static int response_cookies_exists(const char *key);
static int response_cookies_gethaskeys(const char *key);
static void response_cookies_setraw(const char *key, const char *value);
static void unescape(char *str);
static char *escape(const char *str);
static void errFINAL_oom();
static void session_save();
static char *get_session_filepath(const char *sessid);
static char *htmlencode(const char *str);
static int readbyte();
int initcsp();
int deinitcsp();
static void response_flush();


static int scripttimeout = 30;
static clock_t scriptstart;



typedef struct 
memarea_type
{
	unsigned char *mem;
	size_t memlen;
	size_t memsiz;
	size_t memgro;
} memarea;
static int
addmembyte(memarea *mem, unsigned char byte)
{
	unsigned char *memalloc;
	if (mem->memlen >= mem->memsiz)
	{
		memalloc = (unsigned char*)realloc(mem->mem, mem->memsiz+mem->memgro+1);
		if (!memalloc) 
		{
			errFINAL_oom();
			return 0;
		}
		mem->memsiz += mem->memgro;
		mem->mem = memalloc;
	}
	mem->mem[mem->memlen++] = byte;
	mem->mem[mem->memlen] = 0;
	return 1;
}
static int
addmembytes(memarea *mem, unsigned char *byte, size_t num)
{
	size_t i;
	for (i=0;i<num;i++)
	{
		if (!addmembyte(mem, byte[i])) return 0;
	}
	return 1;
}
#define NEW_MEMAREA(name) memarea name = {0,0,0,256}
#define DELETE_MEMAREA(name) if ((name).mem) { free((name).mem); (name).mem = 0; }
#define MEMAREA_ADDBYTE(name, byte) addmembyte(&(name), byte)
#define MEMAREA_ADDBYTES(name, bytes, len) addmembytes(&(name), (unsigned char *)bytes, len)
#define MEMAREA_ADDSTR(name, str) addmembytes(&(name), (unsigned char *)str, strlen(str))
#define MEMAREA_CLEAR(name) { (name).memlen = 0; if ((name).mem) { *(name).mem = 0; } }


static NEW_MEMAREA(uploadedfiles);

static char *
strcopy(const char *key)
{
	char *str;
	if (!key) return 0;
	str = (char*)malloc(strlen(key)+1);
	if (!str) errFINAL_oom();
	strcpy(str, key);
	return str;
}



#define MAXULSIZ (1024*512)
static size_t totalread = 0;
static size_t contentlength = 0;
static size_t maxcontentlength = MAXULSIZ; /* 250K MAX*/
static int igmaxule = 0;


static int resflushed = 0;
static char *sessionid = 0;
static int sessionsenabled = 0;

#define BASESIZE 128
static int
dyn_vsnprintf(char **ret, const char *fmt, va_list ap)
{
	char *buf = 0, *newbuf;
	size_t nextsize = 0, bufsize;
	int outsize;

	bufsize = 0;
	for (;;) 
	{
		if (!bufsize) 
		{
			if (!(buf = (char *)malloc(BASESIZE))) 
			{
				*ret = 0;
				return -1;
			}
			bufsize = 1;
		} 
		else if ((newbuf = (char *)realloc(buf, nextsize))) 
		{
			buf = newbuf;
			bufsize = nextsize;
		} 
		else 
		{
			free(buf);
			*ret = 0;
			return -1;
		}
		outsize = vsnprintf(buf, bufsize, fmt, ap);
		if ((outsize == -1)||(outsize == bufsize)||(outsize == bufsize - 1))
		{
			nextsize = bufsize * 2;
		} 
		else if (outsize > bufsize) 
		{
			nextsize = outsize + 2;
		}
		else break;
	}
	*ret = buf;
	return 0;
}
#undef BASESIZE


static unsigned long crctab[] = {
	0x00000000,
	0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
	0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
	0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
	0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
	0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
	0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
	0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
	0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
	0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
	0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
	0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
	0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
	0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
	0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
	0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
	0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
	0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
	0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
	0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
	0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
	0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
	0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
	0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
	0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
	0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
	0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
	0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
	0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
	0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
	0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
	0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
	0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
	0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
	0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
	0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static unsigned long
cksumbin(const unsigned char *bin, size_t n)
{
	register unsigned   i, c, s = 0;
	for (i = n; i > 0; --i) 
	{
		c = (unsigned)*(bin++);
		s = (s << 8) ^ crctab[(s >> 24) ^ c];
	}
	/* Extend with the length of the string. */
	while (n != 0) 
	{
		c = n & 0377;
		n >>= 8;
		s = (s << 8) ^ crctab[(s >> 24) ^ c];
	}
	return ~s;
}

static int csp_done = 0;

static void delete_all_temp_files()
{
	csp_done = 1;
	if (uploadedfiles.mem)
	{
		char *p, *fn;
		p = (char*)(uploadedfiles.mem);
		fn = p;
		while (*p)
		{
			if (*p==':')
			{
				*p = 0;
				if (*fn)
				{
					remove(fn);
				}
				fn = p+1;
			}
			p++;
		}
	}
}
		   


static void cspexit()
{
	delete_all_temp_files();
	
#if defined(_WIN32) || defined(WIN32)
	/* Must read all remaining bytes from stdin. */
	scripttimeout = 1;
	scriptstart = clock();
	while(readbyte());
#endif
	
	exit(0);
}

/*
  Dictionary Object
*/

typedef struct 
CCODDict_type
{
	char *key;
	char *val;
	int isbinary;
	int binsize;
	struct CCODDict_type *next;
} CCODDict;


static CCODDict *
CCODDict_new()
{
	CCODDict *dict;
	dict = (CCODDict*)malloc(sizeof(CCODDict));
	if (!dict) return 0;
	memset(dict, 0, sizeof(CCODDict));
	return dict;
}


/*static void
CCODDict_setIsBinary(CCODDict *dict, int isbinary)
{
	if (!dict||!dict->next) return;
	dict->isbinary = isbinary?1:0;
}

static int
CCODDict_getIsBinary(CCODDict *dict)
{
	if (!dict) return 0;
	return dict->isbinary;
} */


static void
CCODDict_delete(CCODDict *dict)
{
	CCODDict *ndict;
	while (dict)
	{
		ndict = dict->next;
		if (dict->key) free(dict->key);
		if (dict->val) free(dict->val);
		free(dict);
		dict = ndict;
	}
}

static int 
CCODDict_count(CCODDict *dict)
{
	int count = 0;
	if (!dict) return 0;
	while (dict)
	{
		count++;
		dict = dict->next;
	}
	return count-1;
}

static int
CCODDict_set(CCODDict *dict, const char *key, const char *val)
{
	CCODDict *pdict = 0;
	char *nval = 0;
	CCODDict *ndict = 0;
	if (!dict||!key) return 0;

	while (dict)
	{
		if (dict->key&&!strcmp(key, dict->key))
		{
			if (!val) 
			{
				pdict->next = dict->next;
				if (dict->key) free(dict->key);
				if (dict->val) free(dict->val);
				free(dict);
				return 1;
			}
			else
			{
				nval = (char*)malloc(strlen(val)+1);
				if (!nval) return 0;
				strcpy(nval, val);
				if (dict->val) free(dict->val);
				dict->val = nval;
				return 1;
			}
		}
		else if (!dict->next)
		{
			

			ndict = (CCODDict*)malloc(sizeof(CCODDict));
			if (!ndict) return 0;
			ndict->key = (char*)malloc(strlen(key)+1);
			if (!ndict->key)
			{
				free(ndict);
				return 0;
			}
			strcpy(ndict->key, key);
			ndict->val = (char*)malloc(strlen(val)+1);
			if (!ndict->val) 
			{
				free(ndict->key);
				free(ndict);
				return 0;
			}
			strcpy(ndict->val, val);
			dict->next = ndict;
			ndict->next = 0;
			return 1;
		}
		pdict = dict;
		dict = dict->next;
	}
}

static char *
CCODDict_get(CCODDict *dict, const char *key)
{
	if (!dict||!key) return 0;
	while (dict)
	{
		if (dict->key&&!strcmp(key, dict->key))
		{
			return dict->val;
		}		
		dict = dict->next;
	}
	return 0;
}

static char *
CCODDict_key(CCODDict *dict, int index)
{
	int idx = 0;
	if (!dict) return 0;
	dict = dict->next;
	while (dict)
	{
		if (idx==index) return dict->key;
		idx++;
		dict = dict->next;
	}
	return 0;
}

static char *
CCODDict_val(CCODDict *dict, int index)
{
	int idx = 0;
	if (!dict) return 0;
	dict = dict->next;
	while (dict)
	{
		if (idx==index) return dict->val;
		idx++;
		dict = dict->next;
	}
	return 0;
}


static CCODDict *cookiesdict;
struct _cookie_struct
{
	char *value;
	time_t expires;
	char *domain;
	char *path;
	int secure;
	int response;
};

static void
put_string_into(const char *str, CCODDict *dict)
{
	char *p, *pair;
	p = (char*)str;
	pair = p;
	while (*p)
	{
		
		if (*p=='&')
		{
			*p = 0;
			if (*pair!=0&&*pair!='&')
			{
				char *key, *val, *p2;
				p2 = pair;
				key=p2;
				while (*p2)
				{
					if (*p2=='=')
					{
						*p2=0;
						p2++;
						break;
					}
					p2++;
				}
				val = p2;
				if (*key)
				{
					char *cval, *nval;
					unescape(key);
					unescape(val);
					cval = CCODDict_get(dict, key);
					if (!cval)
					{
						if (!CCODDict_set(dict, key, val)) errFINAL_oom();
					}
					else
					{
						nval = (char*)malloc(strlen(cval)+strlen(val)+2);
						if (!nval) errFINAL_oom();
						sprintf(nval, "%s,%s", cval, val);
						if (!CCODDict_set(dict, key, nval)) errFINAL_oom();						
					}
				}
			}
			p++;
			pair = p;
		}
		p++;
	}
}


static void printerror(const char *str)
{
	if (!resflushed) printf("Content-Type: text/html\n\n");
	printf("<html>\r\n");
	printf("    <head>\r\n");
	printf("        <title>CCOD Runtime Error</title>\r\n");
	printf("    </head>\r\n");
	printf("    <body bgcolor=\"white\">\r\n");
	printf("        <font face=\"arial,sans-serif\" color=\"black\">\r\n");
	printf("        <hr size=\"1\"/>\r\n");
	printf("        <font size=\"3\"><b>CCOD Runtime Error</b><br></font>\r\n");
	printf("        <font size=\"2\">%s</font>\r\n", str);
	printf("        <hr size=\"1\"/>\r\n");
	printf("        <font size=\"2\"><i>%s </i>running on<i> %s</i></font>\r\n", getenv("CCOD_SOFTWARE"), getenv("SERVER_SOFTWARE"));
	printf("        </font>\r\n");
	printf("    </body>\r\n");
	printf("</html>\r\n");
	cspexit();
}



void
errFINAL_oom()
{
	printerror("Out of Memory");
}







static CCODDict *querydict = 0;
static char *querycopy = 0;
static CCODDict *formdict = 0;
static CCODDict *uploaddict = 0;
static char *formcopy = 0;
static CCODDict *setdict = 0;
static char empty_string[] = "";





static char *pexec(const char *cmd, int *retval)
{
	char buf[512], *almem, *cvar = 0;
	size_t cvarlen = 0, read;
	FILE *f; int rv;
	if (!(f = popen(cmd, "r"))) goto l__oom;
	read = fread(buf, 1, sizeof(buf), f);
	while (read)
	{
		almem = (char*)realloc(cvar, cvarlen+read+1);
		if (!almem) goto l__oom;
		cvar = almem;
		memcpy(cvar+cvarlen, buf, read);
		cvar[cvarlen+read] = 0;
		cvarlen+=read;
		read = fread(buf, 1, sizeof(buf), f);
	}
	rv = pclose(f);
	if (retval) *retval = rv;
	if (!cvar)
	{
		cvar = (char*)malloc(1);
		if (!cvar) goto l__oom;
		cvar[0] = 0;
	}
	return cvar;
l__oom:
		if (cvar) free(cvar);
	if (retval) *retval = -1;
	return 0;
}

static void errFINAL_flushed()
{
	printerror("Content already flushed");
}

static void errFINAL_scriptTimeout()
{
	printerror("The script has timed out.");
}


/*
static void 
errFINAL_serverTimeoutCannotStart()
{
	if (!resflushed) printf("Content-Type: text/html\n\n");
	printf("<h2>CCOD Runtime Error</h2>");
	printf("Server Timeout Service did not start.\n");
	cspexit();
}

  */
static void errFINAL_unknown()
{
	printf("<h2>CCOD Runtime Error</h2>");
	printf("Unknown Error Occurred\n");
	cspexit();
}



/*
static char *pexec_res = 0;
static char *exec(const char *cmd, int *retval)
{
	if (pexec_res) free(pexec_res);
	pexec_res = pexec(cmd, retval);
	return pexec_res;
}
*/

void unescape(char *str)
{
	char *p1 = str, *p2 = str;
	while (*p1)
	{
		if (*p1=='%')
		{
			char c3;
			if (!*(p1+1)||!*(p1+2)) break;
			c3 = *(p1+3);
			*(p1+3) = 0;
			*p2 = strtol(p1+1, 0, 16);
			*(p1+3) = c3;
			p1+=3;
		}
		else
		{
			if (*p1=='+') *p2 = ' ';
			else *p2 = *p1;
			p1++;		
		}
		p2++;
	}
	*p2 = 0;
}













static unsigned char cbyte = 0;
static unsigned char lbyte = 0;
static unsigned int readeof = 0;
static unsigned char readbuf[256];
static size_t curidx = 0;
static int retbyte = 0;

static char *cline = 0;
static int clinesz = 0;
static int clineln = 0;
const int clinegro = 15;
static int clineeof = 0;

static int 
readbyte()
{
	if (retbyte)
	{
		retbyte = 0;
		return 1;
	}
	if (totalread>=contentlength) readeof = 1;
	if (readeof) return 0;
	lbyte = cbyte;
	if (!fread(&cbyte,1,1,stdin))
	{
		/* Must exit now! */
		exit(0);
		//printerror("Data was not finished being uploaded by the browser.");
	}
	totalread++;
	if (totalread>=contentlength) readeof = 1;

	scriptstart = clock();

	return 1;
}

static void
returnbyte()
{
	retbyte = 1;
}

size_t
readbytes(unsigned char *buf, size_t num)
{
	size_t i;
	for (i=0;i<num;i++)
	{
		if (!readbyte()) return i;
		buf[i] = cbyte;
	}
	return num;
}

static void
readline_(int strip)
{
	if (clinesz==0)
	{
		clinesz = clinegro;
		cline = (char*)realloc(cline, clinesz+1);
		if (!cline) errFINAL_oom();
	}
	clineln = 0;
	for (;;)
	{
		if (!readbyte())  break;
		if (clineln>=clinesz)
		{
			clinesz += clinegro;
			cline = (char*)realloc(cline, clinesz+1);
			if (!cline) errFINAL_oom();
		}
		cline[clineln] = cbyte;
		clineln++;


		if (cbyte=='\n')
		{
			if (strip)
			{
				clineln--;
				if (clineln&&lbyte=='\r') clineln--;
				/*
				cline[clineln] = 0;
				if (clineln>0&&cline[clineln-1]=='\r')
				{
					cline[clineln-2] = 0;
				} */
			}
			break;
		}

	}
	cline[clineln] = 0;
}
static void
readline()
{
	readline_(1);
}

static void
readfullline()
{
	readline_(0);
}

static int 
strcmpnc(const char *str1, const char *str2)
{
	int res;
	char *p, *str1lc, *str2lc;
	str1lc = strcopy(str1);
	str2lc = strcopy(str2);
	p = str1lc;
	while (*p) {*p = tolower(*p);p++;}
	p = str2lc;
	while (*p) {*p = tolower(*p);p++;}
	res = strcmp(str1lc, str2lc);
	free(str1lc);
	free(str2lc);
	return res;
}





static void
read_multipart(const char *bound)
{

	size_t read;
	char *buf;
	size_t bufsize;
	size_t i;
	char *bfound;
	char *p, *p2, *p3;
	char *formkey = 0;
	FILE *fil;
	char *filname;

	char *boundstart, *boundend, *boundstart_crlf, *boundend_crlf;
	int form_data;

	char *hkey = 0, *hval = 0;
	char *hfkey = 0, *hfval = 0;
	
	NEW_MEMAREA(formval);
	NEW_MEMAREA(formstr);
	

//	printf("Content-Type: text/html\n\nUploaded File");
	//exit(0);

	boundstart = (char*)malloc(strlen(bound)+3);
	if (!boundstart) errFINAL_oom();
	sprintf(boundstart, "--%s", bound);


	boundend = (char*)malloc(strlen(bound)+5);
	if (!boundend) errFINAL_oom();
	sprintf(boundend, "--%s--", bound);


	boundstart_crlf = (char*)malloc(strlen(bound)+7);
	if (!boundstart_crlf) errFINAL_oom();
	sprintf(boundstart_crlf, "\r\n--%s\r\n", bound);


	boundend_crlf = (char*)malloc(strlen(bound)+9);
	if (!boundend_crlf) errFINAL_oom();
	sprintf(boundend_crlf, "\r\n--%s--\r\n", bound);
	
	cline = strcopy("");
	
	//bufsize = strlen(bound)+512;
	//buf = (char*)malloc(bufsize*2);
	//if (!buf) errFINAL_oom();
	
	
	//printf("Content-Type: text/html\n\n<pre>");
	//printf("[%s]\n", getenv("CONTENT_TYPE"));
	//printf("[%s] [%s] [%d]\n", "multipart/form-data", bound, length);
	//printf("[%d]\n", !strcmpnc(getenv("HTTP_CONNECTION"), "keep-alive"));
	//exit(0);

	
	/* Read up to the first boundary start. */
	for (;;)
	{
		if (readeof) goto l__eof;
		readline();
		if (!strcmp(cline, boundend)) goto l__eof;
		if (!strcmp(cline, boundstart)) break;
	}

	//printf("Reading the item headers.\n");
	
l__nextElement:	
		
	MEMAREA_CLEAR(formval);
		
	form_data = 0;
	if (hkey) free(hkey);
	if (hval) free(hval);
	if (hfkey) free(hfkey);
	if (hfval) free(hfval);
	hkey = 0;
	hval = 0;
	hfkey = 0;
	hfval = 0;
	
	/* Read the content-disposition. */
	for (;;)
	{
		if (readeof) goto l__eof;
		readline();
		
		//printf("[%s]\n", cline);
		if (!strcmp(cline, "")) break;
		
		//printf("[name='%s' filename='%s']\n", hval, hfval);
		
		/* Parse the header line. */
		p = cline;
		while (*p)
		{
			if (*p==':')
			{
				*p = 0;
				p++;
				break;
			}
			*p = tolower(*p);
			p++;
		}
		p2 = p;
		p = cline+strlen(cline)-1;
		while (p>cline)
		{
			if (isspace(*p)) *p = 0;
			p--;
		}
		p = p2;
		
		if (!strcmp(cline, "content-disposition"))
		{
			/* read each element */
			while (*p)
			{
				while (*p&&isspace(*p)) p++;
				p2 = p;
				while (*p)
				{
					if (*p==';')
					{
						*p = 0;
						p++;
						break;
					}
					else if (*p=='\"')
					{
						while (*p)
						{
							if (*p=='\"') break;
							p++;
						}
					}
					p++;
				}
				p3 = p2;
				while (*p2)
				{
					if (*p2==';'||*p2=='=') break;
					*p2 = tolower(*p2);
					p2++;
				}
				p2 = p3;
				if (!strcmp(p2, "form-data"))
				{
					form_data = 1;
				}
				else
				{
					p3 = p2;
					while (*p3)
					{
						if (*p3=='=')
						{
							*p3 = 0;
							p3++;
							break;
						}
						p3++;
					}
					while (*p3&&isspace(*p3)) p3++;
					if (*p3=='\"') p3++;
					if (p3[strlen(p3)-1]=='\"') p3[strlen(p3)-1] = 0;
					
					if (!strcmp(p2, "name"))
					{
						if (hkey) free(hkey);
						hkey = strcopy(p2);
						if (hval) free(hval);
						hval = strcopy(p3);
						
					}
					else if (!strcmp(p2, "filename"))
					{
						if (hfkey) free(hfkey);
						hfkey = strcopy(p2);
						if (hfval) free(hfval);
						hfval = strcopy(p3);
					}
				}
			}
		}
	}

	//printf("[%s] [%s]\n", hval, hfval);
	if (hval&&hfval)
	{
		filname = tempnam(0,0);
		fil = fopen(filname, "wb+");
		if (!fil) printerror("Disk write error.\n");
	}
	
	while (readbyte())
	{
		if (cbyte=='\r')
		{
			char *boundcpy;
l__tryAnotherGo:
			i = 0;
			boundcpy = (char*)malloc(strlen(boundend_crlf)+1);
			if (!boundcpy) errFINAL_oom();
			while (cbyte==boundstart_crlf[i]&&i<strlen(boundstart_crlf))
			{
				if (!readbyte()) goto l__eof;
				boundcpy[i] = boundstart_crlf[i];
				i++;
			}
			boundcpy[i] = 0;
			if (i==strlen(boundstart_crlf))
			{
				if (!strcmp(boundstart_crlf, boundcpy))
				{
					char *esckey, *escval;
					esckey = escape(hval);
					MEMAREA_ADDSTR(formstr, esckey);
					MEMAREA_ADDBYTE(formstr, '=');
					if (hval&&!hfval)
					{
						if (formval.mem) escval = (char*)escape((char *)(formval.mem));
						else escval = escape("");
						MEMAREA_ADDSTR(formstr, escval);
					}
					else if (hval&&hfval)
					{
						escval = escape(hfval);
						MEMAREA_ADDSTR(formstr, escval);
						free(escval);
						escval = escape(":");
						MEMAREA_ADDSTR(formstr, escval);
						free(escval);
						escval = escape(filname);
						MEMAREA_ADDSTR(formstr, escval);
						MEMAREA_ADDSTR(uploadedfiles, filname);
						MEMAREA_ADDSTR(uploadedfiles, ":");
						fclose(fil);
					}
					MEMAREA_ADDBYTE(formstr, '&');
					free(esckey);
					free(escval);
					//printf("\nEnd of Element\n", boundcpy);
					returnbyte();
					goto l__nextElement;
				}
			}
			else
			{
				if (!strncmp(boundend_crlf, boundcpy, strlen(boundstart_crlf)-2))
				{
					while (cbyte==boundend_crlf[i]&&i<strlen(boundend_crlf))
					{
						if (!readbyte()) goto l__eofnel;
						boundcpy[i] = boundend_crlf[i];
						i++;
					}
					boundcpy[i] = 0;
					if (i==strlen(boundend_crlf))
					{
						if (!strcmp(boundend_crlf, boundcpy))
						{
							char *esckey, *escval;
l__eofnel:
							esckey = escape(hval);
							MEMAREA_ADDSTR(formstr, esckey);
							MEMAREA_ADDBYTE(formstr, '=');
							if (hval&&!hfval)
							{
								if (formval.mem) escval = (char*)escape((char *)formval.mem);
								else escval = escape("");
								MEMAREA_ADDSTR(formstr, escval);
							}
							else if (hval&&hfval)
							{
								escval = escape(hfval);
								MEMAREA_ADDSTR(formstr, escval);
								free(escval);
								escval = escape(":");
								MEMAREA_ADDSTR(formstr, escval);
								free(escval);
								escval = escape(filname);
								MEMAREA_ADDSTR(formstr, escval);
								MEMAREA_ADDSTR(uploadedfiles, filname);
								MEMAREA_ADDSTR(uploadedfiles, ":");
								fclose(fil);
							}
							MEMAREA_ADDBYTE(formstr, '&');
							free(esckey);
							free(escval);
							//printf("\nEnd of Element\n");
							//printf("End of File\n");
							goto l__eof;
						}
					}
				}
			}
			if (hval&&!hfval)
			{
				MEMAREA_ADDBYTES(formval, boundcpy, i);
				MEMAREA_ADDBYTE(formval, cbyte);
			}
			else if (hval&&hfval)
			{
				if (!fwrite(boundcpy, 1, i, fil)) printerror("Disk write error.\n");
				if (!fwrite(&cbyte,1,1,fil)) printerror("Disk write error.\n");
			}
			if (cbyte=='\r') goto l__tryAnotherGo;
		}
		else
		{
			if (hval&&!hfval)
			{
				MEMAREA_ADDBYTE(formval, cbyte);
			}
			else if (hval&&hfval)
			{
				if (!fwrite(&cbyte,1,1,fil)) printerror("Disk write error.\n");
			}
		}
	}

l__eof:
		
	//printf("All Done!\n");
	
	//exit(0);
	MEMAREA_ADDSTR(formstr, "k=v");
	put_string_into((char*)formstr.mem, formdict);
	
	
	
	
	//printf("[%s]\n", formstr);
		
	if (hkey) free(hkey);
	if (hval) free(hval);
	if (hfkey) free(hfkey);
	if (hfval) free(hfval);
	free(boundend);
	free(boundstart);
	free(boundend_crlf);
	free(boundstart_crlf);
	
	DELETE_MEMAREA(formval);
	DELETE_MEMAREA(formstr);
	//exit(0);
}


/*
 Form
 */
static void initformdata()
{
	formdict = CCODDict_new();
	if (!formdict) errFINAL_oom();

	uploaddict = CCODDict_new();
	if (!uploaddict) errFINAL_oom();

	if (getenv("REQUEST_METHOD"))
	{
		char *request_method;
		char *p;
		request_method = (char*)malloc(strlen(getenv("REQUEST_METHOD"))+1);
		if (!request_method) errFINAL_oom();
		strcpy(request_method, getenv("REQUEST_METHOD"));
		p = request_method;
		while(*p&&isspace(*p)) p++;
		while(*p){*p=tolower(*p);p++;}
		p--; while(p>request_method&&isspace(*p)) *(p--) = 0;
		if (!strcmp(request_method, "post")&&getenv("CONTENT_TYPE"))
		{
			char *content_type;
			char *p;
			char *formstring;

			content_type = (char*)malloc(strlen(getenv("CONTENT_TYPE"))+1);
			if (!content_type) errFINAL_oom();
			strcpy(content_type, getenv("CONTENT_TYPE"));
			p = content_type;
			while(*p&&isspace(*p)) p++;
			while(*p){*p=tolower(*p);p++;}
			p--; while(p>content_type&&isspace(*p)) *(p--) = 0;
			if (!strcmp(content_type, "application/x-www-form-urlencoded"))
			{
				formstring = (char*)malloc(contentlength+10);
				memset(formstring, 0, contentlength+10);
				if (!formstring) errFINAL_oom();

				readbytes((unsigned char*)formstring, contentlength);
				//fgets(formstring, contentlength+1, stdin);
				formcopy = strcopy(formstring);
				strcat(formstring, "&X_=_");
				put_string_into(formstring, formdict);
				free(formstring);
			}
			else
			{
				char *p;
				char *content_line;
				content_line = strcopy(getenv("CONTENT_TYPE"));
				p = content_line;
				while (*p)
				{
					*p = tolower(*p);
					p++;
				}
				if (strstr(content_line, "multipart/form-data ")||strstr(content_line, "multipart/form-data;"))
				{
					char *formstr;
					char *bound, *bound2;
					char *type;
					char *content_length;
					size_t length;
					free(content_line);
					content_line = strcopy(getenv("CONTENT_TYPE"));
					content_length = strcopy(getenv("CONTENT_LENGTH")?getenv("CONTENT_LENGTH"):"0");
					bound = "";
					type = content_line;
					while (*type&&isspace(*type)) type++;
					p = type;
					while (*p&&*p!=';')
					{
						*p = tolower(*p);
						p++;
					}
					if (*p==';')
					{   
						char *p2;
						*p = 0;
						p2 = p-1;
						while (p2>type&&isspace(*p2))
						{
							*p2 = 0;
							p2--;
						}
						p++;
						bound = p;
						while (*bound&&isspace(*bound)) bound++;
						p2 = bound+strlen(bound)-1;
						while (p2>bound&&isspace(*p2))
						{
							*p2 = 0;
							p2--;
						}
					}
					length = atol(content_length);
					p = type;
					while (*p)
					{
						*p = tolower(*p);
						p++;
					}
					p = bound;
					while (*p)
					{
						*p = tolower(*p);
						if (isspace(*p))
						{
							*p = 0;
							p++;
							while (*p&&isspace(*p)) p++;
							if (*p=='=')
							{
l__equ:
								*p = 0;
								p++;
								while (*p&&isspace(*p)) p++;
								bound2 = p;
								if (!strcmp(bound, "boundary")&&strcmp(bound2, ""))
								{
									bound = bound2;
									goto l__oob;	
								}
							}
							bound = "";
						}
						else if (*p=='=') goto l__equ;
						p++;
					}
l__oob:
					if (!strcmp(type, "multipart/form-data")&&strcmp(bound,""))
					{
						if (contentlength==0||maxcontentlength==0||contentlength>maxcontentlength)
						{
							if (!igmaxule)
							{
								printerror("The submitted form data exceded the maximum number of bytes allowed.");
							}
						}
						else
						{
							read_multipart(bound);
						}
					}
					free(content_length);
				}
				free(content_line);
			}
			free(content_type);
		}
		free(request_method);
	}
	
	if (!formcopy)
	{
		formcopy = (char*)malloc(1);
		if (!formcopy) errFINAL_oom();
		*formcopy = 0;
	}
}

static char *
request_form_get(const char *key)
{
	char *val;
	val = CCODDict_get(formdict, key);
	return val?val:empty_string;
}

static char *
request_form_key(int index)
{
	char *key;
	if (index>=Request.Form.Count) return empty_string;
	key = CCODDict_key(formdict, index);
	return key?key:empty_string;
}

static char *
request_form_value(int index)
{
	char *val;
	if (index>=Request.Form.Count) return empty_string;
	val = CCODDict_val(formdict, index);
	return val?val:empty_string;
}

static int 
request_form_exists(const char *key)
{
	return CCODDict_get(formdict, key)?1:0;
}



/*
	QueryString
*/



static void 
initquerystring()
{
	char *querystring;

	querydict = CCODDict_new();
	if (!querydict) errFINAL_oom();

	
	if (getenv("QUERY_STRING"))
	{
		querystring = (char*)malloc(strlen(getenv("QUERY_STRING"))+2);
		if (!querystring) errFINAL_oom();
		sprintf(querystring, "%s&", getenv("QUERY_STRING"));
	}
	else
	{
		querystring = (char*)malloc(1);
		if (!querystring) errFINAL_oom();
		*querystring = 0;
	}
	querycopy = (char*)malloc(strlen(querystring)+1);
	if (!querycopy) errFINAL_oom();
	strcpy(querycopy, querystring);
	put_string_into(querystring, querydict);
	free(querystring);
}

static char *
request_querystring_get(const char *key)
{
	char *val;
	val = CCODDict_get(querydict, key);
	return val?val:empty_string;
}

static char *
request_querystring_key(int index)
{
	char *key;
	if (index>=Request.QueryString.Count) return empty_string;
	key = CCODDict_key(querydict, index);
	return key?key:empty_string;
}

static char *
request_querystring_value(int index)
{
	char *val;
	if (index>=Request.QueryString.Count) return empty_string;
	val = CCODDict_val(querydict, index);
	return val?val:empty_string;
}

static int 
request_querystring_exists(const char *key)
{
	return CCODDict_get(querydict, key)?1:0;
}



/*
	ServerVariables
*/


static void initservervariables()
{
	char *p, *p2, *key, *setlistt, *setlist;
	char defaultlist[] = 
		"APPL_MD_PATH=.\n"
		"APPL_PHYSICAL_PATH=.\n"
		"AUTH_PASSWORD=.\n" 
		"AUTH_TYPE=.\n" 
		"AUTH_USER=.\n" 
		"CCOD_SOFTWARE=.\n"
		"CCOD_PROGPATH=.\n"
		"CCOD_SESSIONS=.\n"
		"CCOD_CSPINUSE=.\n"
		"CCOD_IGMAXULE=.\n"
		"CCOD_MAXULSIZ=.\n"
		"CERT_COOKIE=.\n" 
		"CERT_FLAGS=.\n" 
		"CERT_ISSUER=.\n" 
		"CERT_KEYSIZE=.\n" 
		"CERT_SECRETKEYSIZE=.\n" 
		"CERT_SERIALNUMBER=.\n" 
		"CERT_SERVER_ISSUER=.\n" 
		"CERT_SERVER_SUBJECT=.\n" 
		"CERT_SUBJECT=.\n" 
		"CONTENT_LENGTH=.\n" 
		"CONTENT_TYPE=.\n"
		"DOCUMENT_ROOT=.\n" 
		"GATEWAY_INTERFACE=.\n" 
		"HTTPS=.\n" 
		"HTTPS_KEYSIZE=.\n" 
		"HTTPS_SECRETKEYSIZE=.\n" 
		"HTTPS_SERVER_ISSUER=.\n" 
		"HTTPS_SERVER_SUBJECT=.\n" 
		"HTTP_ACCEPT=.\n" 
		"HTTP_ACCEPT_CHARSET=.\n" 
		"HTTP_ACCEPT_ENCODING=.\n"
		"HTTP_ACCEPT_LANGUAGE=.\n" 
		"HTTP_CONNECTION=.\n"
		"HTTP_COOKIE=.\n" 
		"HTTP_KEEP_ALIVE=.\n"
		"HTTP_FROM=.\n"
		"HTTP_HOST=.\n" 
		"HTTP_REFERER=.\n" 
		"HTTP_USER_AGENT=.\n"
		"INSTANCE_ID=.\n"
		"INSTANCE_META_PATH=.\n"
		"LOCAL_ADDR=.\n"
		"LOGON_USER=.\n" 
		"PATH=.\n"
		"PATH_INFO=.\n"
		"PATH_TRANSLATED=.\n" 
		"QUERY_STRING=.\n" 
		"PWD=.\n"
		"REMOTE_ADDR=.\n"
		"REMOTE_HOST=.\n" 
		"REMOTE_IDENT=.\n" 
		"REMOTE_USER=.\n"
		"REMOTE_PORT=.\n"
		"REQUEST_METHOD=.\n" 
		"REQUEST_URI=.\n"
		"SCRIPT_FILENAME=.\n"
		"SCRIPT_NAME=.\n" 
		"SCRIPT_URI=.\n"
		"SCRIPT_URL=.\n"
		"SERVER_ADDR=.\n"
		"SERVER_ADMIN=.\n"
		"SERVER_NAME=.\n"
		"SERVER_PORT=.\n" 
		"SERVER_PROTOCOL=.\n" 
		"SERVER_PORT_SECURE=.\n"
		"SERVER_PROTOCOL=.\n"
		"SERVER_SIGNATURE=.\n"
		"SERVER_SOFTWARE=.\n"
		"SHLVL=.\n"
		"URL=.\n";

	

	setdict = CCODDict_new();
	if (!setdict) errFINAL_oom();

#if defined(WIN32) || defined(_WIN32)
	setlistt = pexec("set", 0);
	if (!setlistt||!*setlistt)
	{
		setlistt = strcopy(defaultlist);
	}
#else
	setlistt = pexec("env", 0);
	if (!setlistt||!*setlistt) 
	{
		if (setlistt) free(setlistt);
		setlistt = pexec("set",0);
		if (!setlistt||!*setlistt)
		{
			setlistt = strcopy(defaultlist);
		}
	}
#endif


	setlist = (char*)malloc(strlen(setlistt)+strlen(defaultlist)+1);
	if (!setlist) errFINAL_oom();
	sprintf(setlist, "%s\n%s", defaultlist, setlistt);
	free(setlistt);
	
	
	

	p = setlist;
	key = p;
	while (*p)
	{
		if (*p=='\r'||*p=='\n')
		{
			*p = 0;
			p++;
			
			p2 = key;
			while (*p2)
			{
				if (*p2=='=')
				{
					*p2 = 0;
					p2 = key;
					while (*p2)
					{
						if (isspace(*p2)) break;
						p2++;
					}
					if (!isspace(*p2))
					{
						if (getenv(key))
						{
							if (!CCODDict_set(setdict, key, "---")) errFINAL_oom();
						}
					}
					break;
				}
				p2++;
			}
			key = p;
			continue;
		}
		p++;	
	}
	free(setlist);
}










static char *
request_servervariables_value(int index)
{
	char *key, *val;
	key = CCODDict_key(setdict, index);
	if (key&&*key)
	{
		val = getenv(key);
		if (val)
		{
			return val;
		}
	}
	return empty_string;
}


static char *
request_get(const char *key)
{
	if (Request.QueryString.Exists(key)) return Request.QueryString.Get(key);
	if (Request.Form.Exists(key)) return Request.Form.Get(key);
	//if (Request.Cookies.Exists(key)) return Request.Cookies.Get(key);
	//if (Request.ClientCertificate.Exists(key)) return Request.ClientCertificate.Get(key);
	if (Request.ServerVariables.Exists(key)) return Request.ServerVariables.Get(key);
	return empty_string;
}


static char *
request_servervariables_get(const char *key)
{
	char *val;
	val = CCODDict_get(setdict, key);
	if (val)
	{
		val = getenv(key);
		if (val)
		{
			return val;
		}
	}
	return empty_string;
}

static char *
request_servervariables_key(int index)
{
	char *key;
	key = CCODDict_key(setdict, index);
	if (key) return key;
	return empty_string;
}



static int 
request_servervariables_exists(const char *key)
{
	return CCODDict_get(setdict, key)?1:0;
}

static int 
request_exists(const char *key)
{
	return 
		Request.QueryString.Exists(key)||
		Request.Form.Exists(key)||
		Request.Cookies.Exists(key)||
		//Request.ClientCertificate.Exists(key)||
		Request.ServerVariables.Exists(key);
}

static int request_querystring_getcount()
{
	return Request.QueryString.Count;
}
static int request_form_getcount()
{
	return Request.Form.Count;
}
static int request_servervariables_getcount()
{
	return Request.ServerVariables.Count;
}


static int request_binaryread(void *buf, int size)
{
	return readbytes(buf, size);
}

static int request_gettotalbytes()
{
	return contentlength;
}

static void
initcookies()
{
	char *envstr;
	char *p;
	char *key, *val;

	cookiesdict = CCODDict_new();
	if (!cookiesdict) errFINAL_oom();


	envstr = getenv("HTTP_COOKIE");
	if (envstr)
	{
		envstr = strcopy(envstr);

		p = envstr;
		key = p;
l__next:
		while (*p)
		{
			if (*p=='=')
			{
				*p = 0;
				val = p+1;
				p++;
				for (;;)
				{
					if (*p==';'||*p=='\0')
					{
						*p = 0;
						unescape(key);
						response_cookies_setraw(key, val);
						p++;
						if (!*p) goto l__finished;
						p++;
						if (!*p) goto l__finished;
						else if (isspace(*p)) p++;
						key = p;
						goto l__next;
					}
					p++;
				}
			}
			p++;
		}
l__finished:
		free(envstr);
	}
}

static void
checkcontentlength()
{
	contentlength = getenv("CONTENT_LENGTH")?(size_t)atol(getenv("CONTENT_LENGTH")):0;
	maxcontentlength = getenv("CCOD_MAXULSIZ")?(size_t)atol(getenv("CCOD_MAXULSIZ")):0;
	igmaxule = (size_t)(getenv("CCOD_IGMAXULE")&&!strcmp(getenv("CCOD_IGMAXULE"),"yes"))?1:0;
	//printf("Content-Type: text/html\n\n%d:%d\n", contentlength, maxcontentlength);

	if (contentlength>maxcontentlength)
	{
		if (!igmaxule)
		{
			printerror("The submitted data exceded the maximum number of bytes allowed.");
		}
	}
}


static void 
initrequestobject()
{
	
	checkcontentlength();
	



	initquerystring();
	initformdata();
	initservervariables();
	initcookies();

	Request.QueryString.qstring = querycopy;
	Request.QueryString.Count = CCODDict_count(querydict);
	Request.QueryString.GetCount = request_querystring_getcount;
	Request.QueryString.Get = request_querystring_get;
	Request.QueryString.Key = request_querystring_key;
	Request.QueryString.Value = request_querystring_value;
	Request.QueryString.Exists = request_querystring_exists; 
	
	Request.Form.fstring = formcopy;
	Request.Form.Count = CCODDict_count(formdict);
	Request.Form.GetCount = request_form_getcount;
	Request.Form.Get = request_form_get;
	Request.Form.Key = request_form_key;
	Request.Form.Value = request_form_value;
	Request.Form.Exists = request_form_exists; 
	
	Request.ServerVariables.Count = CCODDict_count(setdict);
	Request.ServerVariables.GetCount = request_servervariables_getcount;
	Request.ServerVariables.Get = request_servervariables_get;
	Request.ServerVariables.Key = request_servervariables_key;
	Request.ServerVariables.Value = request_servervariables_value;
	Request.ServerVariables.Exists = request_servervariables_exists; 
	
	Request.Get = request_get; 
	Request.Exists = request_exists; 
	Request.BinaryRead = request_binaryread;
	Request.GetTotalBytes = request_gettotalbytes;

	Request.Cookies.Exists = response_cookies_exists;
	Request.Cookies.Get = response_cookies_get;
	Request.Cookies.GetHasKeys = response_cookies_gethaskeys;	
	Request.Cookies.GetKey = response_cookies_getkey;	
}




/*
	RESPONSE OBJECT
*/
static char *resbuffer = 0;
static CCODDict *headerdict = 0;
static int resbinarymode = 0;
static unsigned char *resbinarybuffer = 0;
static size_t resbinarybuffersize = 0;


static void response_setcontenttype(const char *contype)
{
	Response.AddHeader("Content-Type", contype);
}


static char *response_getcontenttype()
{
	return CCODDict_get(headerdict, "Content-Type");
}


static void response_setstatus(const char *status)
{
	Response.AddHeader("Status", status);
}


static char *response_getstatus()
{
	return CCODDict_get(headerdict, "Status");
}



static void response_clear()
{
	if (resbuffer) *resbuffer = 0;
	resbinarybuffersize = 0;
}

static void response_end()
{
	deinitcsp();
	cspexit();
}

static void response_setbuffer(int val)
{
	Response.p_buffer = val?1:0;
	if (!Response.p_buffer) Response.Flush();
}

static int 
response_getbuffer()
{
	return Response.p_buffer;
}

static void
response_binarywrite(const unsigned char *buf, int bufsize)
{
	unsigned char *nbuf;
	resbinarymode = 1;
	if (buf&&bufsize)
	{
		if (Response.p_buffer)
		{
			resbinarybuffersize+=bufsize;
			nbuf = (unsigned char*)realloc(resbinarybuffer, resbinarybuffersize);
			if (!nbuf) errFINAL_oom();
			resbinarybuffer = nbuf;
			memcpy(resbinarybuffer+resbinarybuffersize-bufsize, buf, bufsize); 
		}
		else
		{
			fwrite(buf, 1, bufsize, stdout);
		}
	}
}

static void
response_write(const char *fmt, ...)
{
	va_list args;
	resbinarymode = 0;
	if (Response.p_buffer)
	{   
		char *ret;
		int suc;
		char *bufalloc;
		va_start(args, fmt);
		suc = dyn_vsnprintf(&ret, fmt, args);
		va_end(args);
		if (suc) errFINAL_oom();
		bufalloc = (char*)realloc(resbuffer, strlen(ret)+strlen(resbuffer)+1);
		if (!bufalloc) errFINAL_oom();
		resbuffer = bufalloc;
		strcat(resbuffer, ret);
		free(ret);
	}
	else
	{
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}


static void 
response_addheader(const char *header, const char *value)
{
	if (resflushed) errFINAL_flushed();
	if (!header||!value) return;
	CCODDict_set(headerdict, header, value);
}

static void
response_redirect(const char *location)
{
	Response.AddHeader("Location", location);
	*resbuffer = 0;
	resbinarymode = 0;
	response_flush();
	printf("Redirecting to <a href=\"%s\">%s</a>\n", location, location);
	cspexit();
}




static struct _cookie_struct
decookiestr(const char *cookiestr)
{
	char *p;
	char *cookiestrcp;
	char *cstr;
	int itemidx;
	struct _cookie_struct cookie;
	memset(&cookie, 0, sizeof(cookie));
	if (!cookiestr) return cookie;
	cookiestrcp = strcopy(cookiestr);
	p = cookiestrcp;
	cstr = p;
	itemidx = 0;
	while (cookiestr)
	{
		if (*p==';')
		{
			*p = 0;
			switch (itemidx)
			{
				case (0):cookie.value = strcopy(cstr);break;
				case (1):cookie.expires = (time_t)(atol(cstr));break;
				case (2):cookie.domain = strcopy(cstr);break;
				case (3):cookie.path = strcopy(cstr);break;
				case (4):cookie.secure = atoi(cstr);break;
				case (5):cookie.response = atoi(cstr);goto l__lastItem;
			}
			itemidx++;
			cstr = p+1;
			*p = ';';
		}
		p++;
	}
l__lastItem:
	free(cookiestrcp);
	return cookie;
}

static char *
encookiestr(struct _cookie_struct *cookie)
{
	char *enstr;
	enstr = (char*)malloc(strlen(cookie->value)+
						  strlen(cookie->domain)+
						  strlen(cookie->path)+250);
	if (!enstr) errFINAL_oom();
	sprintf(enstr, "%s;%lu;%s;%s;%d;%d;", cookie->value, cookie->expires, 
			cookie->domain, cookie->path, cookie->secure, cookie->response);
	return enstr;
}

static int
get_cookie(const char *key, struct _cookie_struct *cookiein)
{
	char *cookiestr;
	struct _cookie_struct cookie;
	if (!key||!cookiein) return 0;
	cookiestr = CCODDict_get(cookiesdict, key);
	if (cookiestr)
	{
		cookie = decookiestr(cookiestr);
	}
	else
	{
		memset(&cookie, 0, sizeof(cookie));
		cookie.value = strcopy("");
		cookie.domain = strcopy("");
		cookie.path = strcopy("");
	}
	//Response.Write("%s<br>", cookiestr);
	memcpy(cookiein, &cookie, sizeof(cookie));
	return 1;
}

static void free_cookie(struct _cookie_struct *cookiein)
{
	free(cookiein->value);
	free(cookiein->domain);
	free(cookiein->path);
}

static char *
response_cookies_get(const char *key)
{
	struct _cookie_struct cookie;
	char *value;
	if (!get_cookie(key, &cookie)) return strcopy(empty_string);
	value = strcopy(cookie.value);
	unescape(value);
	free_cookie(&cookie);
	return value;
}

static int 
response_cookies_exists(const char *key)
{
	return CCODDict_get(cookiesdict, key)?1:0;
}




static int
set_cookie(const char *key, struct _cookie_struct *cookiein)
{
	char *cookiestr;
	if (!key||!cookiein) return 0;
	cookiestr = encookiestr(cookiein);
	if (!cookiestr) errFINAL_oom();
	if (!CCODDict_set(cookiesdict, key, cookiestr)) errFINAL_oom();
	free(cookiestr);
	return 1;
}



static void
response_cookies_set(const char *key, const char *value)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	free(cookie.value);
	cookie.value = escape(value);
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static void
response_cookies_setraw(const char *key, const char *value)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	free(cookie.value);
	cookie.value = strcopy(value);
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}



static void
response_cookies_setpath(const char *key, const char *path)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	free(cookie.path);
	cookie.path = escape(path);
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static char *
response_cookies_getpath(const char *key)
{
	struct _cookie_struct cookie;
	char *path;
	if (!get_cookie(key, &cookie)) return strcopy(empty_string);
	path = strcopy(cookie.path);
	unescape(path);
	//Response.Write("GetPath: %s<br>", encookiestr(&cookie));
	free_cookie(&cookie);
	return path;
}



static void
response_cookies_setdomain(const char *key, const char *domain)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	free(cookie.domain);
	cookie.domain = escape(domain);
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static char *
response_cookies_getdomain(const char *key)
{
	struct _cookie_struct cookie;
	char *domain;
	if (!get_cookie(key, &cookie)) return strcopy(empty_string);
	domain = strcopy(cookie.domain);
	unescape(domain);
	free_cookie(&cookie);
	return domain;
}

static void
response_cookies_setexpires(const char *key, time_t expires)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	cookie.expires = expires;
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static time_t
response_cookies_getexpires(const char *key)
{
	struct _cookie_struct cookie;
	time_t expires;
	if (!get_cookie(key, &cookie)) return 0;
	expires = cookie.expires;
	free_cookie(&cookie);
	return expires;
}



static void
response_cookies_setsecure(const char *key, int secure)
{
	struct _cookie_struct cookie;
	if (!get_cookie(key, &cookie)) return;
	cookie.secure = secure;
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static int
response_cookies_getsecure(const char *key)
{
	struct _cookie_struct cookie;
	int secure;
	if (!get_cookie(key, &cookie)) return 0;
	secure = cookie.secure;
	free_cookie(&cookie);
	return secure;
}









void
response_flush()
{
	if (!resflushed)
	{
		CCODDict *dict = headerdict;
		int cookiecount, i;
		struct _cookie_struct cookie;
		
		dict = dict->next;
		while (dict)
		{
			printf("%s: %s\n", dict->key, dict->val);
			dict = dict->next;
		}
		if (sessionid&&*sessionid&&sessionsenabled)
		{
			session_save();
			response_cookies_set("CCODSessionId", sessionid);
		}
		cookiecount = CCODDict_count(cookiesdict);
		for (i=0;i<cookiecount;i++)
		{
			char *key = CCODDict_key(cookiesdict, i);
			
			if (get_cookie(key, &cookie))
			{
				if (1);//cookie.response)
				{
					char *keyen;
					keyen = escape(key);
					printf("Set-Cookie: ");
					printf("%s=%s", keyen, cookie.value);
					if (cookie.expires)
					{
						char dtstr[60];
						strftime(dtstr, 59, 
							"%a, %d-%b-%Y %H:%M:%S GMT", 
							gmtime(&(cookie.expires)));
						printf("; expires=%s", dtstr);
					}
					if (cookie.domain&&cookie.domain[0])
					{
						printf("; domain=%s", cookie.domain);
					}
					if (cookie.path&&cookie.path[0])
					{
						printf("; path=%s", cookie.path);
					}
					if (cookie.secure)
					{
						printf("; secure");
					}
					printf("\n");
					free(keyen);
				}
				free_cookie(&cookie);
			}
		}
		
		
		//if (resstatus) printf("Status: %s\n", resstatus);	
		//printf("Content-Type: %s\n", rescontenttype);
		printf("\n");
	}
	if (!resbinarymode)
	{
		if (resbuffer && *resbuffer)
		{
			printf("%s", resbuffer);
			*resbuffer = 0;
		}
	}
	else
	{
		if (resbinarybuffer && resbinarybuffersize)
		{
			fwrite(resbinarybuffer, 1, resbinarybuffersize, stdout);
			resbinarybuffersize = 0;
		}
	}
	resflushed = 1;
}

static char *
response_cookies_getkey(const char *key, const char *skey)
{
	char *ekey;
	struct _cookie_struct cookie;
	char *keyidx;
	char *keysearch;
	char *p;
	char *value, *pv;
	if (!key||!skey) return strcopy(empty_string);
	if (!get_cookie(key, &cookie)) return strcopy(empty_string);
	if (!(cookie.value[0]=='&')) return strcopy(empty_string);
	ekey = escape(skey);
	keysearch = (char*)malloc(strlen(skey)+10);
	if (!keysearch) errFINAL_oom();
	sprintf(keysearch, "&%s=", ekey);
	keyidx = strstr(cookie.value, keysearch);
	if (keyidx)
	{
		p = keyidx+strlen(keysearch);
		pv = p;
		for (;;)
		{
			if (*p=='&')
			{
				*p = 0;
				value = strcopy(pv);
				unescape(value);
				*p = '&';
				break;
			}
			else if (!*p)
			{
				value = strcopy(pv);
				unescape(value);
				break;
			}
			p++;
		}
	}
	else
	{
		value = strcopy(empty_string);
	}
	free(keysearch);
	free(ekey);
	free_cookie(&cookie);
	return value;
}



static void
response_cookies_setkey(const char *key, const char *skey, const char *value)
{
	struct _cookie_struct cookie;
	char *ekey;
	char *evalue;
	char *nstr;
	if (!key||!skey||!value) return;
	if (!get_cookie(key, &cookie)) return;
	ekey = escape(skey);
	evalue = escape(value);
	if (cookie.value[0]=='&')
	{
		char *keyidx;
		char *keysearch;
		char *p;
		keysearch = (char*)malloc(strlen(skey)+10);
		if (!keysearch) errFINAL_oom();
		sprintf(keysearch, "&%s=", ekey);
		keyidx = strstr(cookie.value, keysearch);
		if (keyidx)
		{
			char *front, *back = 0;
			front = cookie.value;
			
			*keyidx = 0;
			p = keyidx+1;
			while (*p)
			{
				if (*p=='&')
				{
					*p = 0;
					back = p+1;
					break;
				}
				p++;
			}
			if (back)
			{
				nstr = (char*)malloc(strlen(ekey)+strlen(evalue)+strlen(front)+strlen(back)+50);
				if (!nstr) errFINAL_oom();
				sprintf(nstr, "%s&%s=%s&%s", front, ekey, evalue, back);
			}
			else
			{
				nstr = (char*)malloc(strlen(ekey)+strlen(evalue)+strlen(front)+50);
				if (!nstr) errFINAL_oom();
				sprintf(nstr, "%s&%s=%s", front, ekey, evalue);
			}
		}
		else
		{
			nstr = (char*)malloc(strlen(ekey)+strlen(evalue)+strlen(cookie.value)+50);
			if (!nstr) errFINAL_oom();
			sprintf(nstr, "%s&%s=%s", cookie.value, ekey, evalue);
		}
		free(keysearch);
	}
	else
	{
		nstr = (char*)malloc(strlen(ekey)+strlen(evalue)+50);
		if (!nstr) errFINAL_oom();
		sprintf(nstr, "&%s=%s", ekey, evalue);
	}
	free(ekey);
	free(evalue);
	free(cookie.value);
	cookie.value = nstr;
	cookie.response = 1;
	set_cookie(key, &cookie);
	free_cookie(&cookie);
}

static int
response_cookies_gethaskeys(const char *key)
{
	struct _cookie_struct cookie;
	int haskeys;
	if (!get_cookie(key, &cookie)) return 0;
	haskeys = (cookie.value[0]=='&');
	free_cookie(&cookie);
	return haskeys;
}

static void
initresponseobject()
{
	resbuffer = (char*)malloc(1);
	if (!resbuffer) errFINAL_oom();
	*resbuffer = 0;
	
	
	
	
	headerdict = CCODDict_new();
	if (!headerdict) errFINAL_oom();
	
	Response.p_buffer = 1;
	Response.GetBuffer = response_getbuffer;
	Response.SetBuffer = response_setbuffer;
	Response.SetContentType = response_setcontenttype;
	Response.GetContentType = response_getcontenttype;
	Response.SetStatus = response_setstatus;
	Response.GetStatus = response_getstatus;
	Response.Write = response_write;
	Response.BinaryWrite = response_binarywrite;	
	Response.Flush = response_flush;
	Response.AddHeader = response_addheader;
	Response.Redirect = response_redirect;
	Response.End = response_end;
	Response.Clear = response_clear;
	
	Response.Cookies.Exists = response_cookies_exists;
	Response.Cookies.Set = response_cookies_set;
	Response.Cookies.Get = response_cookies_get;
	Response.Cookies.SetPath = response_cookies_setpath;
	Response.Cookies.GetPath = response_cookies_getpath;
	Response.Cookies.SetDomain = response_cookies_setdomain;
	Response.Cookies.GetDomain = response_cookies_getdomain;
	Response.Cookies.SetExpires = response_cookies_setexpires;
	Response.Cookies.GetExpires = response_cookies_getexpires;
	Response.Cookies.SetSecure = response_cookies_setsecure;
	Response.Cookies.GetSecure = response_cookies_getsecure;

	Response.Cookies.GetHasKeys = response_cookies_gethaskeys;	
	Response.Cookies.SetKey = response_cookies_setkey;	
	Response.Cookies.GetKey = response_cookies_getkey;	
	
	Response.SetContentType("text/html");
	
}






static char *
htmlencode(const char *str)
{
	char *nstr;
	char *p, *p2;
	int nsize = 0;
	p = (char*)str;
	while (*p)
	{
		if (*p=='&'||*p=='<'||*p== '>') nsize+=5;
		else nsize++;
		p++;
	}
	nstr = (char*)malloc(nsize+1);
	if (!nstr) errFINAL_oom();
	p = (char*)str;
	p2 = nstr;
	while (*p)
	{
		if (*p == '&') 
		{
			sprintf(p2, "&amp;");
			p2+=4;
		} 
		else if (*p == '<') 
		{
			sprintf(p2, "&lt;");
			p2+=3;
		} 
		else if (*p == '>') 
		{
			sprintf(p2, "&gt;");
			p2+=3;
		} 
		else 
		{
			*p2 = *p;
		}
		p2++;
		p++;
	}
	*p2 = 0;
	return nstr;
}


char *
escape(const char *str)
{
	char *nstr;
	char *p, *p2;
	int nsize = 0;
	p = (char*)str;
	while (*p)
	{
		if (isalnum(*p)||*p=='-'||*p=='*'||*p=='.'||*p=='_'||*p==' ') nsize++;
		else nsize+=3;
		p++;
	}
	nstr = (char*)malloc(nsize+1);
	if (!nstr) errFINAL_oom();
	p = (char*)str;
	p2 = nstr;
	while (*p)
	{
		if (isalnum(*p)||*p=='-'||*p=='*'||*p=='.'||*p=='_') *p2 = *p;
		else if (*p==' ') *p2 = '+';
		else
		{
			sprintf(p2, "%%%02x", *p);
			p2+=2;
		}
		p2++;
		p++;
	}
	*p2 = 0;
	return nstr;
}


static char *server_urlencode(const char *str)
{
	return escape(str);
}


static char *server_htmlencode(const char *str)
{
	return htmlencode(str);
	
	
}


static char *server_mappath(const char *str)
{
	char *root;
	char *outstr;
	char *p;
	if (*str=='/')
	{
		root = Request.ServerVariables.Get("DOCUMENT_ROOT");
		if (!root||!*root)
		{
			root = Request.ServerVariables.Get("PATH_TRANSLATED");
			outstr = (char*)malloc(strlen(root)+strlen(str)+1);
			if (!outstr) errFINAL_oom();
			strcpy(outstr, root);
			outstr[strlen(outstr)-strlen(Request.ServerVariables.Get("PATH_INFO"))] = 0;
			strcat(outstr, str);
		}
		else
		{
			outstr = (char*)malloc(strlen(root)+strlen(str)+1);
			if (!outstr) errFINAL_oom();
			sprintf(outstr, "%s%s", root, str);
		}
		
	}
	else
	{
		root = Request.ServerVariables.Get("SCRIPT_FILENAME");
		if (!root||!*root)
		{
			root = Request.ServerVariables.Get("PATH_TRANSLATED");
		}
		outstr = (char*)malloc(strlen(root)+strlen(str)+1);
		if (!outstr) errFINAL_oom();
		strcpy(outstr, root);
		p = outstr+strlen(outstr);
		while (p>outstr)
		{
			if (*p=='/'||*p=='\\')
			{
				p++;
				*p = 0;
				break;
			}
			p--;
		}
		strcat(outstr, str);
	}
	p = outstr;
	while (*p)
	{

		if (*p=='\\') *p = '/';
		p++;
	}
	return outstr;
}

static unsigned long 
get_urndnum()
{
	char *uid;
	int rnum;
	unsigned long urndnum;
	static int hassrand = 0;
	char *ua,*ra,*rp;
	if (!hassrand)
	{
		hassrand = 1;
		srand(time(0));
	}
	rnum = rand()%500000+100000;
	ua = Request.ServerVariables.Get("PATH_INFO");
	ra = "";
	rp = Request.ServerVariables.Get("LOCAL_ADDR");
	uid = (char*)malloc(strlen(ua)+strlen(ra)+strlen(rp)+100);
	if (!uid) errFINAL_oom();
	sprintf(uid, "%d-%s-%s-%s", rnum,ua,ra,rp);
	urndnum = cksumbin((unsigned char*)uid, strlen(uid));
	return urndnum;
}


static char *
get_new_sessionid()
{
	char *sessid;
	char *p;
	char *tmpnm = tempnam(0,0);
	char *filepath;
	FILE *f;

l__try_again:
	sessid = (char*)malloc(strlen(tmpnm)+100);
	if (!sessid) errFINAL_oom();
	sprintf(sessid, "%s%lu", tmpnm, get_urndnum());
	p = sessid;
	while (*p)
	{
		if (!isalnum(*p)) *p = '0';
		*p = toupper(*p);
		p++;
	}
	strcat(sessid, ".SID");

	filepath = get_session_filepath(sessid);
	f = fopen(filepath, "rb");
	if (f) 
	{
		free(filepath);
		free(sessid);
		fclose(f);
		goto l__try_again;
	}
	free(filepath);
	return sessid;
}


static int server_getscripttimeout()
{
	return scripttimeout;
}

static void server_setscripttimeout(int timeout)
{
	scripttimeout = timeout;
	scriptstart = clock();
}


static void initserverobject()
{
	Server.URLEncode = server_urlencode;
	Server.HTMLEncode = server_htmlencode;
	Server.MapPath = server_mappath;
	Server.GetScriptTimeout = server_getscripttimeout;
	Server.SetScriptTimeout = server_setscripttimeout;
	scriptstart = clock();
}


static int sessiontimeout = 20; /* minutes */

static CCODDict *sessiondict = 0;

static int session_gettimeout()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	return sessiontimeout;
}

static void session_settimeout(int timeout)
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	sessiontimeout = timeout;
	session_save();
}


static void session_abandon()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	response_cookies_set("CCODSessionId", "");
	CCODDict_delete(sessiondict);
	sessiondict = CCODDict_new();
	if (!sessiondict) errFINAL_oom();
	sessionid = get_new_sessionid();
	session_save();
}


char *get_session_filepath(const char *sessid)
{
	int i;
	char *progpath;
	char *filepath, *p;
	if (!sessionsenabled) printerror("Sessions are disabled.");

	progpath = getenv("CCOD_PROGPATH");
	if (!progpath||!*progpath) return 0;
	filepath = (char*)malloc(strlen(progpath)+100);
	if (!filepath) errFINAL_oom();
	strcpy(filepath, progpath);
	for (i=0;i<4;i++)
	{
		p = filepath+strlen(filepath)-1;
		while (p>filepath)
		{
			if (*p=='/'||*p=='\\')
			{
				*p = 0;
				break;
			}
			p--;
		}
	}
	sprintf(filepath+strlen(filepath), "/sessions");
	umask(0);
	mkdir2(filepath);
	sprintf(filepath+strlen(filepath), "/%s.dat", sessid?sessid:sessionid);
	return filepath;
}

static void session_save()
{
	int i, count;
	char *filepath;
	FILE *f;
	time_t t;

	if (!sessionsenabled) printerror("Sessions are disabled.");
	filepath = get_session_filepath(0);
	f = fopen(filepath, "wb+");
	if (!f) printerror("Could not open the session dat file.");

	t = time(0);
	t += (sessiontimeout*60); /* add the timeout value current time */
	fwrite(&t, 1, sizeof(time_t), f);
	count = CCODDict_count(sessiondict);
	for (i=0;i<count;i++)
	{
		char *key = CCODDict_key(sessiondict, i);
		char *val = CCODDict_val(sessiondict, i);
		fprintf(f, "%s", key);
		fputc(0,f);
		fprintf(f, "%s", val);
		fputc(0,f);
	}
	fclose(f);
	free(filepath);
	response_cookies_set("CCODSessionId", sessionid);
}

static void session_contents_set(const char *key, const char *val)
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	CCODDict_set(sessiondict, key, val);
	session_save();
}

static void session_contents_remove(const char *key)
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	CCODDict_set(sessiondict, key, 0);
	session_save();
}


static int 
session_contents_exists(const char *key)
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	return CCODDict_get(sessiondict, key)?1:0;
}


static void session_contents_removeall()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	CCODDict_delete(sessiondict);
	sessiondict = CCODDict_new();
	if (!sessiondict) errFINAL_oom();
	session_save();
}


static char *session_contents_get(const char *key)
{
	char *res;
	if (!sessionsenabled) printerror("Sessions are disabled.");
	
	res = CCODDict_get(sessiondict, key);
	return res?res:empty_string;
}

static int session_contents_getcount()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	return CCODDict_count(sessiondict);
}

static char *session_contents_key(int index)
{
	char *key;
	int count;
	if (!sessionsenabled) printerror("Sessions are disabled.");

	
	count = session_contents_getcount();
	if (index>=count) return empty_string;
	key = CCODDict_key(sessiondict, index);
	return key?key:empty_string;
}

static char *session_contents_value(int index)
{
	char *val;
	int count;
	if (!sessionsenabled) printerror("Sessions are disabled.");
	
	count = session_contents_getcount();
	if (index>=count) return empty_string;
	val = CCODDict_val(sessiondict, index);
	return val?val:empty_string;
}


static char *get_current_sessionid()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	return Request.Cookies.Get("CCODSessionId");
}

static char *session_contents_getsessionid()
{
	if (!sessionsenabled) printerror("Sessions are disabled.");
	return sessionid;
	
}

static void initsessionobject()
{
	int i;
	char *filepath;
	FILE *f;
	char *buf, *key, *val = 0;
	int kturn = 1;
	size_t siz;
	time_t t, ct;
	char *envstr;
	
	envstr = getenv("CCOD_SESSIONS");
	sessionsenabled = (envstr&&!strcmp(envstr,"yes"))?1:0;

	Session.Abandon = session_abandon;
	
	Session.Contents.GetCount = session_contents_getcount;
	Session.Contents.Set = session_contents_set;
	Session.Contents.Get = session_contents_get;
	Session.Contents.Key = session_contents_key;
	Session.Contents.Value = session_contents_value;
	Session.Contents.Remove = session_contents_remove;
	Session.Contents.RemoveAll = session_contents_removeall;
	Session.Contents.Exists = session_contents_exists;

	Session.GetCount = session_contents_getcount;
	Session.Set = session_contents_set;
	Session.Get = session_contents_get;
	Session.Key = session_contents_key;
	Session.Value = session_contents_value;
	Session.Remove = session_contents_remove;
	Session.RemoveAll = session_contents_removeall;
	Session.Exists = session_contents_exists;
	
	Session.GetSessionID = session_contents_getsessionid;
	Session.GetTimeout = session_gettimeout;
	Session.SetTimeout = session_settimeout;
	
	
	if (!sessionsenabled) return;
	
	sessiondict = CCODDict_new();
	if (!sessiondict) errFINAL_oom();

	sessionid = get_current_sessionid();
	

	if (!sessionid||!*sessionid)
	{
		sessionid = get_new_sessionid();
	}

	
	/* Load Session Data */
	filepath = get_session_filepath(0);

	f = fopen(filepath, "rb");
	if (!f) return;
	
	
	fseek(f, 0, SEEK_END);
	siz = ftell(f);
	rewind(f);
	buf = (char*)malloc(siz);
	if (!buf) errFINAL_oom();
	fread(&t, 1, sizeof(time_t), f);
	ct = time(0);
	if (t-ct <= 0)
	{
		fclose(f);
		remove(filepath);
		free(sessionid);
		sessionid = get_new_sessionid();
		goto l__deadsession;
	}
	siz -= sizeof(t);
	fread(buf, 1, siz, f);
	key = buf;
	for (i=0;i<siz;i++)
	{
		if (buf[i]==0)
		{
			if (kturn==1)
			{
				val = &(buf[i+1]);
				i++;
				kturn = 0;
			}
			else
			{
				Session.Contents.Set(key, val);
				key = &(buf[i+1]);
				i++;
				kturn = 1;
			}
		}
	}
	fclose(f);
l__deadsession:
	free(filepath);	
	
}

#if defined(_WIN32) || defined(WIN32)
static DWORD WINAPI timeoutchecker(LPVOID lpParam) 
{ 
	while (!csp_done)
	{
		Sleep(1);
		if ((int)(((double)(clock()-scriptstart))/CLOCKS_PER_SEC)>=scripttimeout)
		{
			errFINAL_scriptTimeout();
		}
	}
    return 0; 
} 
static void
startservertimeoutthread()
{
	DWORD dwThreadId;
    CreateThread(NULL,0,timeoutchecker,0,0,&dwThreadId);
}
#else
static void *timeoutchecker (void *q)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	while (!csp_done)
	{
		select(0, NULL, NULL, NULL, &tv);
		if ((int)(((double)(clock()-scriptstart))/CLOCKS_PER_SEC)>=scripttimeout)
		{
			errFINAL_scriptTimeout();
		}
	}
}
static void
startservertimeoutthread()
{
	pthread_t timeoutthread_id;
	scriptstart = clock();
	pthread_create (&timeoutthread_id, NULL, timeoutchecker, NULL);
}
#endif


int initcsp()
{
#if defined(_WIN32) || defined(WIN32)
    if (setmode(fileno(stdin), O_BINARY) == -1) errFINAL_unknown();
    if (setmode(fileno(stdout), O_BINARY) == -1) errFINAL_unknown();
	if (setvbuf(stdout, NULL, _IONBF, 0)) errFINAL_unknown();
	if (setvbuf(stdin, NULL, _IONBF, 0)) errFINAL_unknown();
#endif
	scripttimeout = 60; /* 60 seconds allowed between byte reads from stdin. */
	startservertimeoutthread();
	initrequestobject();
	initresponseobject();
	initserverobject();
	initsessionobject();
	scripttimeout = 30; /* standard 30 script timeout. */
	return 0;
}

int deinitcsp()
{	
	Response.Flush();
	cspexit();
	return 0;
}
