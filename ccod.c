/*
	Copyright (C) 2004-2008 Joshua J Baker
	See the file COPYING for copying permission.
*/

#define CCOD_MAJOR_VERSION 1
#define CCOD_MINOR_VERSION 2
#define CCOD_MICRO_VERSION 9

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#if defined(__STRICT_ANSI__)
#undef __STRICT_ANSI__
#endif
#if defined(WIN32) || defined(_WIN32)
	#include <fcntl.h>
    #include <io.h>
	#include <process.h>
#endif
#if defined(WIN32) || defined(_WIN32)
	#include <windows.h>
	#define mkdir2(name) mkdir(name)
	#define setenv(name,value,overwrite) SetEnvironmentVariable((LPCTSTR)name,(LPCTSTR)value)
#else
	#define mkdir2(name) mkdir(name,0755)
#endif

#define mkstr(s) # s

char version[512];
char verstl[100];

unsigned char cgiinc[] = {0};
unsigned char cgiheader[] = {0};
unsigned char cgimain[] = {0};
//#include "cgiinc.bin"
//#include "cgiheader.bin"
//#include "cgimain.bin"

/* Some cksum functions */
static unsigned long cksum(const unsigned char *path);
static unsigned long cksumbin(const unsigned char *bin, size_t n);
static unsigned long cksumpath(const char *path);
//static int process_ssi_file(const char *pathin, const char *pathout);
static int process_ssi_stream(const char *path, FILE *fin, FILE *fout, unsigned long *addcksum);
int systemc(const char *command);




int argc;		/* The original arguments */
char **argv;	/* passed from the command line */

/* String Allocator. */
char *stralloc = 0;


/* Global variables. */
char *cgi_object = 0;	/* String  - the cgi object ready for linking. */
char *cgi_dll = 0;
char *cgi_shared = 0;
char *cgi_main = 0;
char *cgi_object_directory = 0;

char *orgsource = 0;	/* String  - the original source path. */
char *source = 0;		/* String  - the path of the source file. */
char *sourcebase = 0;   /* String  - the base name of thesource path. */
char *sourcefull = 0;   /* String  - the full path of the source file. */
char *orgsourcepath = 0;/* String  - the parent path of sourefull. */
char *orgworkpath = 0;  /* String  - the working directory. */
char *tmp_source = 0;   /* String  - the temporary source file. */
char *username = 0;		/* String  - the user name. */
char *homedir = 0;		/* String  - the home directory of the user. */
char *pardir = 0;		/* String  - the parent directory path. */
char *directory = 0;	/* String  - contains the directory of the program. */
char *tempdir = 0;		/* String  - the temp directory of the user. */
char *options = 0;		/* String  - options passed to the cc command. */
char *use_cc = 0;		/* String  - containing the cc command. */
char *std = 0;			/* String  - c standard to use. */
char *workdir = 0;      /* String  - the working directory of the program. */
char *progpath = 0;		/* String  - the path of the compiled program. */
char *progargs = 0;		/* String  - the args of the executed program. */
char *shellcmd = 0;		/* String  - the shell command that will execute. */
char *cc_cmd= 0;		/* String  - the cc command. */
int sessions = 0;		/* Boolean - is cgi sessions enabled? */
int cgi = 0;			/* Boolean - include cgi functions. */
int gui = 0;			/* Boolean - include gui stuff. */
int inline_errors = 0;  /* Boolean - does the errors appear through stdout. */
int shell_script = 0;   /* Boolean - is this a shell script. */
int script = 0;			/* Boolean - is this a script interface. */
int clean = 0;			/* Boolean - does the program need cleaning? */
int execute = 1;		/* Boolean - is the program going to be executed? */
int argidx = 0;			/* Integer - source file argument index. */
int fretval = 0;		/* Integer - the final return value. */
int ignmaxerr = 0;		/* Integer - ignore the max data error. */
int objc = 0;           /* Boolean - objective-c source. */
unsigned long 
max_upload_size = 0;    /* UInt    - Max bytes allowed to be uploaded. */
unsigned long 
sourcesum = 0;			/* ULong   - the cksum of the source file data. */
unsigned long
pathsum = 0;			/* ULong   - the cksum of the path file data. */
unsigned long
compilesum = 0;			/* ULong   - the compile cksum of the cc options. */


static void print_error(const char *errstr);

/*
	oom();
	Prints an Out of Memory error to the stderr and exits unsuccessfully.
 
 */
#define oom() { oom_(__LINE__); }
static void
oom_(int l)
{
	print_error("error: out of memory.\n");
	exit(-1);
}



/*
	strset(String, Value)
	Sets a string's value.  Auto handles the memory.
*/
#define strset(str,val) ((str?free(str):0),(((str=(char*)malloc(strlen(val)+1))&&strcpy(str,val))?str:0))


#if defined(_WIN32) || defined(WIN32)
int system2file(const char *command, const char *path, int std_output, int std_error)
{
	DWORD retval;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE File = 0;
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL; 
	sa.bInheritHandle = TRUE;
	if (path)
	{
		File = CreateFile(path, 
			GENERIC_WRITE, 
			0,  
			&sa, 
			CREATE_ALWAYS, 
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (File == INVALID_HANDLE_VALUE)
		{
			print_error("error: system2file failed (%s).\n");
			exit(-1);
		}
	}

	//printf("Content-Type: text/html\n\n");

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);							
	si.hStdOutput = std_output?File:GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = std_error?File:GetStdHandle(STD_ERROR_HANDLE);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess( NULL, // No module name (use command line).
		(LPTSTR)command, // Command line.
		NULL,             // Process handle not inheritable.
		NULL,             // Thread handle not inheritable.
		TRUE,             // Set handle inheritance to FALSE.
		0,                // No creation flags.
		NULL,             // Use parent's environment block.
		NULL,             // Use parent's starting directory.
		&si,              // Pointer to STARTUPINFO structure.
		&pi)              // Pointer to PROCESS_INFORMATION structure.
		)
	{
		char errstr[] = "error: could not start process ";
		char *cpath = 0;
		char *p;
		DWORD written;
		TCHAR szBuf[150]; 
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError(); 
		



		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL );

		wsprintf(szBuf, "failed with error %d: %s\n", dw, lpMsgBuf); 



		strset(cpath, command);
		p = cpath;
		if (*p=='\"')
		{
			p++;
			while (*p)
			{
				if (*p=='\"') *p = 0;
				p++;
			}
			p = cpath+1;
		}
		else
		{
			while (*p)
			{
				if (isspace(*p)) *p = 0;
				p++;
			}
			p = cpath;			
		}



		WriteFile(File, errstr, strlen(errstr), &written, 0);
		WriteFile(File, "[", 1, &written, 0);
		WriteFile(File, p, strlen(p), &written, 0);			
		WriteFile(File, "]", 1, &written, 0);
		WriteFile(File, "\n", 1, &written, 0);
		WriteFile(File, szBuf, strlen(szBuf), &written, 0);
		free(cpath);
		LocalFree(lpMsgBuf);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(File);
		return -1;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess (pi.hProcess, &retval);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(File);
	return (int)retval;

}
#else
int 
system2file(const char *command, const char *path, int std_output, int std_error)
{
	char *ncommand;
	int retval;
	ncommand = (char*)malloc(strlen(command)+strlen(path)+50);
	if (!ncommand) oom();
	sprintf(ncommand, "%s &> \"%s\"", command, path);
	retval = system(ncommand);
	free(ncommand);
	return retval;
}
#endif










/* 
	append_cc_options(char *noption)
	Appends the cc command options.
*/
static void
append_cc_options(char *noption)
{
	stralloc = (char*)realloc(options, strlen(options)+strlen(noption)+10);
	if (!stralloc) oom();
	options = stralloc;
	strcat(options, " ");
	strcat(options, noption);
	strcat(options, " ");
}

static void
append_cc_options_quotes(char *noption)
{
	stralloc = (char*)realloc(options, strlen(options)+strlen(noption)+10);
	if (!stralloc) oom();
	options = stralloc;
	strcat(options, " \"");
	strcat(options, noption);
	strcat(options, "\" ");
}


/*
	directory_exists(const char *path)
	Return 1 if the path is a directory or 0 if not.
*/
static int
directory_exists(const char *path)
{
	DIR *d;
	d = opendir(path);
	if (!d) return 0;
	closedir(d);
	return 1;
}



/* 
	store_tempdir()
	Gets the TEMP/TMP environment variable and stores it.
*/
static void
store_tempdir()
{
	char *temp;
	char *p;
	temp = (char*)getenv("TEMP");
	if (!temp) temp = (char*)getenv("TMP");
	if (!temp)
	{
		if (directory_exists("/var/tmp"))
		{
			strset(tempdir, "/var/tmp");
		}
		else if (directory_exists("/usr/tmp"))
		{
			strset(tempdir, "/usr/tmp");
		}
		else if (directory_exists("/tmp"))
		{
			strset(tempdir, "/tmp");
		}
		else
		{
			strset(tempdir, "");
		}
	}
	else
	{
		strset(tempdir, temp);
	}
	p = tempdir;
	while (*p)
	{
		if (*p=='\\') *p = '/';
		p++;
	}
}


/* 
	store_homedir()
	Gets the HOME environment variable and stores it.
*/
static void
store_homedir()
{
	char *home;
	FILE *fpasswd;
	char *line, *linealloc, *useritm, *p, *homeitm;
	char c;
	int nl, ig, lineln, maxlineln;
	int i;

	home = (char*)getenv("HOME");
	if (!home) home = (char*)getenv("USERPROFILE");
	if (!home)
	{
		home = (char*)getenv("HOMEPATH");
		if (home)
		{
			char *homedrive = (char*)getenv("HOMEDRIVE");
			if (!homedrive)
			{
				strset(homedir, home);
			}
			else
			{
				homedir = (char*)malloc(strlen(home)+strlen(homedrive)+25);
				if (!homedir) oom();
				sprintf(homedir, "%s%s", homedrive, home);
			}
		}
		else
		{
			fpasswd = fopen("/etc/passwd", "rb");
			if (!fpasswd)
			{
				strset(homedir, "");
			}
			else
			{
				line = (char*)malloc(60);
				if (!line)
				{
					fclose(fpasswd);
					strset(homedir, "");
				}
				else
				{
					lineln = 0;
					maxlineln = 50;
					c = getc(fpasswd);	
					nl = 1;
					ig = 0;
					while (!feof(fpasswd))
					{
						if (c=='\r'||c=='\n')
						{
							nl = 1;
							ig = 0;
							c = getc(fpasswd);
							line[lineln+1] = 0;
							p = line;
							for (i=0;i<1;i++)
							{
								useritm = p;
								while (*p)
								{
									if (*p==':')
									{
										*p = 0;
										p++;
										break;
									}
									p++;
								}
							}
							for (i=0;i<5;i++)
							{
								homeitm = p;
								while (*p)
								{
									if (*p==':')
									{
										*p = 0;
										p++;
										break;
									}
									p++;
								}
							}
							if (!strcmp(useritm, username))
							{
								strset(homedir, homeitm);
								free(line);
								fclose(fpasswd);
								return;
							}
							lineln = 0;
							continue;
						}
						if (nl&&c=='#')
						{
							ig = 1;
							nl = 0;
						}
						if (!ig)
						{
							
							if (lineln>=maxlineln)
							{
								maxlineln+=50;
								linealloc = (char*)realloc(line, maxlineln+10);
								if (!linealloc)
								{
									free(line);
									fclose(fpasswd);
									strset(homedir, "");
									return;
								}
								line = linealloc;
							}
							line[lineln] = c;
							lineln++;
						}
						c = getc(fpasswd);
					}
					fclose(fpasswd);
				}
			}

		}
	}
	else
	{
		strset(homedir, home);
	}
	p = homedir;
	while (*p)
	{
		if (*p=='\\') *p = '/';
		p++;
	}
}



static int
remove_directory(const char *path)
{
	FILE *p = 0;
	char *rmcmd;
	int rv = 666;
	if (!directory_exists(path)) return 1;
	rmcmd = (char*)malloc(strlen(path)+25);
	if (!rmcmd) return 0;
#if defined(WIN32) || defined(_WIN32)
	{
		SHFILEOPSTRUCT sfo;
		char *npath;
		char *p;
		npath = (char*)malloc(strlen(path)+10);
		if (!npath) oom();
		memset(npath, 0, strlen(path)+10);
		memcpy(npath, path, strlen(path));
		p = npath;
		while (*p)
		{
			if (*p=='/') *p = '\\';
			p++;
		}
		memset(&sfo, 0, sizeof(SHFILEOPSTRUCT));
		sfo.hwnd = 0;
		sfo.wFunc = FO_DELETE;
		sfo.pFrom = npath;
		sfo.fFlags = FOF_SILENT|FOF_NOCONFIRMATION;
		rv = SHFileOperation(&sfo);
		free(npath);
	}
#else
	sprintf(rmcmd, "rm -rf \"%s\" 2>&1", path);
	p = popen (rmcmd, "r");
	if (p) { rv = pclose(p); }
#endif
	
	//printf("%d %s\n", rv, getenv("PATH"));
	free(rmcmd);
	return !directory_exists(path);
}


static int
create_directory(const char *path)
{
	char *pathm, *p;
	if (directory_exists(path)) return 1;
	umask(0);
	pathm = (char*)malloc(strlen(path)+1);
	if (!pathm) return 0;
	strcpy(pathm, path);
	p = pathm;
	while (*p)
	{
		if (*p=='/'||*p=='\\')
		{
			char c = *p;
			*p = '\0';
			mkdir2(pathm);
			*p = c;
		}
		p++;
	}
	mkdir2(pathm);
	free(pathm);
	return directory_exists(path);
}



/* 
	pexec(const char *cmd, int *retval);
	Executes a command and returns the output.
*/
static char *
pexec(const char *cmd, int *retval)
{
	char buf[512], *almem, *cvar = 0;
	size_t cvarlen = 0, read; 
    FILE *f;
    int rv;
	

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


/* 
	store_username()
	Get the USERNAME environment variable and store it.
*/
static void
store_username()
{
	char *usernameenv;
	char *p;
	int retval;

	usernameenv = (char*)getenv("LOGNAME");
	if (!usernameenv) usernameenv = (char*)getenv("USER");
	if (!usernameenv) usernameenv = (char*)getenv("USERNAME");
	if (!usernameenv)
	{
		if (getenv("SERVER_SOFTWARE")&&strstr((char*)getenv("SERVER_SOFTWARE"), "Microsoft-IIS"))
		{
			strset(username, "IUSR");
			return;
		}
	}
	if (!usernameenv)
	{
		usernameenv = pexec("whoami", &retval);
		if (!usernameenv||!*usernameenv||retval)
		{
			if (usernameenv) free(usernameenv);
			usernameenv = pexec("logname", &retval);
			if (!usernameenv||!*usernameenv||retval)
			{
				if (usernameenv) free(usernameenv);
				strset(username, "");
				return;
			}
		}
		p = usernameenv;
		while (*p)
		{
			if (isspace(*p))
			{
				*p = 0;
				break;
			}
			p++;
		}
		strset(username, usernameenv);
		free(usernameenv);
	}
	else
	{
		strset(username, usernameenv);
	}
}



/* 
	reset_globals()
	Resets all the Globals back to the original value.
*/
static void
reset_globals()
{
	/* Reset Integers */
	argidx = 0;
	max_upload_size = 1024*50; /* 50 KB Max */
		/*1024*1024*15;*/ /* 15 MB Max */

	/* Reset Booleans */
	ignmaxerr = 0;
	sessions = 1;
	clean = 0;
	execute = 1;
	script = 1;
	inline_errors = 1;
	cgi = getenv("SERVER_SOFTWARE")?1:0;
	gui = 0;
	
	/* Reset Cksums */
	sourcesum = 0;
	pathsum = 0;
	
	/* Reset Strings */
	strset(use_cc, "gcc");
	strset(options, "-I. -w -s");
	strset(homedir, "");
	strset(tempdir, "");
	strset(username, "");
	strset(source, "");
	strset(sourcefull, "");
	strset(workdir, "");
	strset(directory, "");
	strset(progpath, "");
	strset(progargs, "");
	strset(shellcmd, "");
	strset(pardir, "");
	strset(tmp_source, "");
	strset(orgsourcepath, "");
	strset(orgworkpath, "");
	strset(cc_cmd, "");
	strset(orgsource, "");
	strset(std, "");
	strset(cgi_object, "");
	
	sprintf(verstl, "ccod (CCOD) %d.%d.%d (%s %s)", CCOD_MAJOR_VERSION,CCOD_MINOR_VERSION,CCOD_MICRO_VERSION,__DATE__,__TIME__);
	//sprintf(verstl, "ccod (CCOD) %d.%d.%d (%s)", CCOD_MAJOR_VERSION,CCOD_MINOR_VERSION,CCOD_MICRO_VERSION,__DATE__);
	
	
	sprintf(version, "%s\n"
		//"Written by Joshua J Baker.\n"
		"\n"
		"Copyright (C) 2004-2008 Joshua J Baker\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
		//"\n"
		,verstl);
			
	
	
}




/* 
	read_arguments()
	Read all the command line options
*/
static void
read_arguments()
{
	int i;
	
	for (i=1;i<argc;i++)
	{
		
		if (!strcmp(argv[i], "-v")||!strcmp(argv[i], "--version"))
		{
			printf("%s", version);
			exit(0);
		}
		else if (!strcmp(argv[i], "-c")||!strcmp(argv[i], "--cc"))
		{
			i++;
			if (i<argc)
			{
				if (argv[i][0]=='-')
				{
					print_error("error: missing compiler name\n");
					exit(-1);
				}
				strset(use_cc, argv[i]);
			}
			else
			{
				print_error("error: missing compiler name\n");
				exit(-1);
			}
		}
		else if (!strcmp(argv[i], "-x")||!strcmp(argv[i], "--clean"))
		{
			clean = 1;
		}
		else if (!strcmp(argv[i], "-z")||!strcmp(argv[i], "--clean-only"))
		{
			clean = 1;
			execute = 0;
		}
		else if (!strcmp(argv[i], "-h")||!strcmp(argv[i], "--help"))
		{
			printf(
				   "Usage: ccod [options] -- file [file options]\n"
				   "       ccod file [file options]\n"
				   "\n"
				   "Options:\n"
				   "  -v, --version     Display version information\n"
				   "  -h, --help        Display this information\n"
				   "  -c, --cc          Assigns the compiler (cc is used as the default)\n"
				   "  -x, --clean       Removes the cached program, recompiles, and runs.\n"
				   "  -z, --clean-only  Removes the cached program and does not compile or run.\n"
				   "  -o, --options     Compiler specific options.\n"
				   "\n"
				   "Examples:\n"
				   "  ccod source.c foo bar       # Runs source.c.\n"
				   "  ccod -c g++ -- source.cpp   # Runs source.cpp using the g++ compiler.\n"
				   "  ccod -o -O2 -s -- source.c  # Runs source.c using the compiler options.\n"
				   "\n"
				   "Report bugs to <bakeman@user.sourceforge.net>.\n");
			exit(0);
		}
		else if (!strcmp(argv[i], "--"))
		{
			i++;
			argidx = i;
			return;
		}
#if 0
		else if (!strcmp(argv[i], "-p")||!strcmp(argv[i], "--pack"))
		{
			FILE *packF;
			char *packIn = 0;
			char *packOut = 0;
			char *packBasename;
			char *packPath = 0;
			char *targzcmd = 0;
			
			i++;
			if (argc==i)
			{
				print_error("error: missing path of file/directory to pack.\n");
				exit(-1);
			}
			strset(packIn, argv[i]);
			i++;
			if (argc==i)
			{
				packBasename = basename(packIn);
				packOut = (char*)malloc(strlen(packBasename)+10);
				if (!packOut) oom();
				sprintf(packOut, "%s.pak", packBasename);
			}
			else
			{
				strset(packOut, argv[i]);
			}
			
			packF = fopen(packOut, "wb+");
			if (!packF)
			{
				print_error("error: could not create the pack file 1.\n");
				exit(-1);
			}
			if (fprintf(packF, 
					"#!/bin/sh\n"
					"#if 0\n"
					"eval 'exec ccod $0 ${1+\"$@\"}'\n"
					"exit\n"
					"#endif\n"
					"#pragma CCOD:package\n"
					"//PackSig"
			)<0)
			{
				print_error("error: could not create the pack file 2.\n");
				exit(-1);
			}
			targzcmd = (char*)malloc(strlen(packIn)+(strlen(packOut)*2)+100);
			if (!targzcmd) oom();
			sprintf(targzcmd, "tar cf - \"%s\" | gzip -9 >> \"%s\"; chmod 755 \"%s\"", packIn, packOut, packOut);
			retval = system(targzcmd);
			if (retval) exit(-1);
			//printf("%s", targzcmd);
			
			
			
			//strset(packSource, argv[i]);
			//printf("%s\n", packOut);
			/*packF = fopen(argv[i], "wb+");
			if (!packF)
			{
					print_error("error: cou
			}
			*/
			
			
			
			exit(0);
		}
#endif
		else if (!strcmp(argv[i], "--testcgi"))
		{
			printf("Content-Type: text/html\n\n");
			printf("<html>\n<head>\n<title>Test CGI</title>\n</head>\n");
			printf("<body>\n<h3>Test CGI</h3>\n<hr>\n<pre>\n");

			//i++;
			//for (;i<argc;i++)
			//{
				//printf("%s\n", argv[i]);
			//}
			//printf("</pre>\n");
			/*{
				int rv;
				printf("<pre>\n[%s]</pre>\n", pexec("c:/mingw/bin/gcc.exe --version", &rv));
				//system("c:/mingw/bin/gcc.exe --version");
				printf("<br>%d<br>", rv);
			} */
			//system("gcc --version");
			//system("set");

			//printf("</body>\n<html>\n");
		}
		else if (!strcmp(argv[i], "-o")||!strcmp(argv[i], "--options"))
		{
			i++;
			for (;i<argc;i++)
			{
				if (!strcmp(argv[i], "--"))
				{
					i++;
					argidx = i;
					if (i<argc) return;
				}
				else
				{
					append_cc_options_quotes(argv[i]);
				}
			}
		}
		else if (argv[i][0]=='-')
		{
			print_error("error: invalid option\n");
			exit(-1);
		}
		else 
		{
			
			argidx = i;
			if (i<argc) return;
		}
	}
	print_error("error: missing source file to execute\n");
	exit (-1);
}

/*
 store_source()
 Read the source from arguments and check the file existance.
*/
static void
store_source()
{
	FILE *fsource;
	char *p;
	strset(source, argv[argidx]);

	/* forward-slash path */
	p = source;
	while (*p)
	{
		if (*p=='\\') *p = '/';
		p++;
	}
	if (directory_exists(source))
	{
		print_error("error: source must be a file.\n");
		exit(-1);
	}
	fsource = fopen(source, "r");
	if (!fsource)
	{
		print_error("error: invalid source file or access denied.\n");
		exit(-1);
	}
	fclose(fsource);

	strset(orgsource, source);
}

/*
 full_path(char *path)
 Returns back the full path of the path.
*/
static char *
full_path(const char *path)
{
	char *fpath, *fpath2;
	char *wd, *p, *p2, *d;
	char *cwd;
	int nwd;
	int iswinabs = 0;
	
	
	if (!path) path = "";
	
	iswinabs = ( strlen(path) >= 2 && isalpha(path[0]) && path[1] == ':');
	
	cwd = (char*)getcwd((char*)NULL, 0);
	
	
	if (*path!='/'&&!iswinabs&&cwd)
	{
		wd = (char*)malloc(strlen(cwd)+25);
		if (!wd) return 0;
		strcpy(wd, cwd);
		nwd = 0;
	}
	else
	{
		wd = "";
		nwd = 1;
	}
	
	fpath = (char*)malloc(strlen(path)+strlen(wd)+25);
	if (!fpath) 
	{
		if (!nwd) free(wd);
		if (cwd) free(cwd);
		return 0;
	}
	
	if (*wd&&*path) 
	{
		sprintf(fpath, "%s/%s", wd, path);
	}
	else if (*wd)
	{
		sprintf(fpath, "%s", wd);
	}
	else if (*path)
	{
		sprintf(fpath, "%s", path);
	}
	else
	{
		if (cwd) free(cwd);
		return 0;
	}

	//printf("%s\n", fpath);

	fpath2 = (char*)malloc(strlen(path)+strlen(wd)+25);
	if (!fpath2) 
	{
		if (!nwd) free(wd);
		free(fpath);
		if (cwd) free(cwd);
		return 0;
	}
	

	iswinabs = ( strlen(fpath) >= 2 && isalpha(fpath[0]) && fpath[1] == ':');

	if (iswinabs)
	{
		strcpy(fpath2, "");
	}
	else
	{
		strcpy(fpath2, "/");
	}
	p = fpath;
	d = p;
	while (*p)
	{
		if (*p=='/')
		{
			*p = 0;
			
			if (!strcmp(d, ".")||!strcmp(d, "")){}
			else if (!strcmp(d, ".."))
			{
				p2 = fpath2+strlen(fpath2)-2;
				while (p2>=fpath2)
				{
					if (*p2=='/')
					{
						p2++;
						*p2 = 0;
						break;
					}
					p2--;
				}
			}
			else
			{
				strcat(fpath2, d);
				strcat(fpath2, "/");
			}
			d = p+1;
		}
		p++;
	}
	
	
	if (!strcmp(d, ".")||!strcmp(d, "")){}
	else if (!strcmp(d, ".."))
	{
		p2 = fpath2+strlen(fpath2)-2;
		while (p2>=fpath2)
		{
			if (*p2=='/')
			{
				p2++;
				*p2 = 0;
				break;
			}
			p2--;
		}
	}
	else
	{
		strcat(fpath2, d);
	}

	p = fpath2;
	while (*p)
	{
		if (*p=='\\') *p = '/';
		p++;
	}

	if (cwd) free(cwd);
	return fpath2;
}



/*
 store_sourcefull()
 Get the full path of the source file.
*/
static void
store_sourcefull()
{
	char *sfull;

	sfull = full_path(source);

	

	if (!sfull)
	{
		strset(sourcefull, "");
	}
	else
	{
		strset(sourcefull, sfull);
		free(sfull);
	}
}

/*
 get_sourcesum()
 Get the cksum of the source data
*/
static void
get_sourcesum()
{
	int retval;
	char *cmd = 0;
	char *retstr;
	char *outpath;
	FILE *f;
	int fsiz;
	
	cmd = malloc(strlen(use_cc) + strlen(sourcefull) + 100);
	if (!cmd) oom();
	
	sprintf(cmd, "%s -w -E -nostdinc \"%s\"", use_cc, sourcefull);
	
	outpath = tempnam(0,0);
	
	retval = system2file(cmd, outpath, 1, 1);
	
	f = fopen(outpath, "rb");
	if (!f)
	{
		print_error("error: invalid source file or access denied.\n");
		exit(-1);
	}
	fseek(f, 0, SEEK_END);
	
	fsiz = ftell(f);
	rewind(f);
	retstr = malloc(fsiz+1);
	if (!retstr)
	{
		fclose(f);
		remove(outpath);
		oom();
	}
	fread(retstr, 1, fsiz, f);
	retstr[fsiz] = 0;
	if (retstr)
	{
		sourcesum = cksumbin((unsigned char *)retstr, strlen(retstr)) + cksum((unsigned char *)sourcefull);
	}
	else
	{
		print_error("error: invalid source file or access denied.\n");
		exit(-1);
	}
	if (retstr) free(retstr);
	if (cmd) free(cmd);
	fclose(f);
	remove(outpath);
	
}

/*
 get_pathsum()
 Get the cksum of the source path
*/
static void
get_pathsum()
{
	pathsum = cksumpath(sourcefull);
}



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
cksum(const unsigned char *path)
{
	register unsigned int i, c, s = 0;
	size_t n;
	FILE *f;
	if (directory_exists((char*)path)) return 0;
	f = fopen((char *)path, "rb");
	if (!f)
	{
		n = 0;
	}
	else
	{
		fseek(f, 0, SEEK_END);
		n = ftell(f);
		rewind(f);
	}
	for (i = n; i > 0; --i) 
	{
		c = (unsigned int)getc(f);
		s = (s << 8) ^ crctab[(s >> 24) ^ c];
	}
	/* Extend with the length of the string. */
	while (n != 0) 
	{
		c = n & 0377;
		n >>= 8;
		s = (s << 8) ^ crctab[(s >> 24) ^ c];
	}
	if (f) fclose(f);
	return ~s;
}

static unsigned long
cksumbin(const unsigned char *bin, size_t n)
{
	register unsigned int i, c, s = 0;
	for (i = n; i > 0; --i) 
	{
		c = (unsigned int)*(bin++);
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

unsigned long
cksumpath(const char *path)
{
	char *path1, *path2, *p;
	struct stat sbuf1,sbuf2;
	unsigned long cks;
	
	
	
	path1 = full_path(path);
	if (!path1) return 0;
	path2 = full_path(path);
	if (!path2)
	{
		free(path1);
		return 0;
	}
	if (*(path1+strlen(path1)-1)=='/')
	{
		*(path1+strlen(path1)-1) = 0;
		*(path2+strlen(path2)-1) = 0;
	}
	p = path2;
	while (*p)
	{
		*p = tolower(*p);
		p++;
	}
	
	stat(path1, &sbuf1);
	stat(path2, &sbuf2);
	
	if (sbuf1.st_ino==sbuf2.st_ino)
	{
		cks = cksumbin((unsigned char *)path2, strlen(path2));		
	}
	else
	{
		cks = cksumbin((unsigned char *)path1, strlen(path1));
	}
	free(path1);
	free(path2);
	return cks;
}


static void
detect_objc()
{
	if (objc)
	{
		append_cc_options("-L /System/Library/Frameworks/Foundation.framework/Foundation");
		append_cc_options("-L /System/Library/Frameworks/AppKit.framework/AppKit");
		append_cc_options("-fnested-functions");
	}
}
/*
 read_pragmas()
 This routine will read the pragmas from the source file and do something with 
 them.
*/

static void
read_pragmas()
{
	size_t i, s = 1024;
	int lc = 0;
	int c = 0;
	char *prag, *val, *p, *pline;
	char *line;
	char *allocator;
	FILE *fsource;
	
	fsource = fopen(source, "rb");
	if (!fsource)
	{
		print_error("error: could not open soruce file during pragma parsing.\n");
		exit (-1);
	}

	line = (char*)malloc(s);
	if (!line) oom();
	
l__nextline:
		*line = 0;
	i=0;
l__appendline:
		while (!feof(fsource))
		{
			lc = c;
			c = getc(fsource);
			if (c=='\n'&&lc=='\r') continue;
			if (c=='\r'||c=='\n'||c==EOF)
			{
				if (i>=s)
				{
					s+=1024;	
					allocator = (char*)realloc(line, s);
					if (!allocator) oom();
					line = allocator;
				}
				line[i] = 0;
				
				if (line[i-1]=='\\')
				{
					i--;
					goto l__appendline;
				}
				
				pline = line;
				while (*pline&&isspace(*pline)) pline++;
				
				if (!strncmp(pline, "//@pragma", 9))
				{
					prag = pline+9;
					goto l__readprag;
				}
				else if (!strncmp(pline, "#pragma", 7))
				{
					
					prag = pline+7;
l__readprag:
					while (*prag)
					{
						if (*prag!=' '&&*prag!='\t') break;
						prag++;
					}
					val = prag;
					while (*val)
					{
						if (*val==' '||*val=='\t')
						{
							*val=0;
							val++;
							break;
						}
						val++;
					}
					while (*val)
					{
						if (*val!=' '&&*val!='\t') break;
						val++;
					}
					
					p = val+strlen(val)-1;
					while (p>=val)
					{
						if (!isspace(*p))
						{
							*(p+1) = 0;
							break;
						}
						p--;
					}
					
					if (!strncmp(prag, "CCOD:", 5))
					{
						if (!strcmp(prag+5, "compilerOptions")||!strcmp(prag+5, "options"))
						{
							append_cc_options(val);
						}
						else if (!strcmp(prag+5, "gui"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								gui = 1;
							}
							else if (!strcmp(val, "no"))
							{
								gui = 0;
							}
						}
						else if (!strcmp(prag+5, "cgi")||!strcmp(prag+5, "csp"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								cgi = 1;
							}
							else if (!strcmp(val, "no"))
							{
								cgi = 0;
							}
						}
						else if (!strcmp(prag+5, "ignoreUploadError"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								ignmaxerr = 1;
							}
							else if (!strcmp(val, "no"))
							{
								ignmaxerr = 0;
							}
						}
						else if (!strcmp(prag+5, "maxUploadSize")||!strcmp(prag+5, "upload"))
						{
							max_upload_size = atol(val);
						}
						else if (!strcmp(prag+5, "std"))
						{
							strset(std, val);
						}
						else if (!strcmp(prag+5, "useCompiler")||!strcmp(prag+5, "compiler"))
						{
							strset(use_cc, val);
						}
						else if (!strcmp(prag+5, "useDirectory")||!strcmp(prag+5, "directory"))
						{
							strset(workdir, val);
						}
						else if (!strcmp(prag+5, "scriptInterface")||!strcmp(prag+5, "script"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								script = 1;
							}
							else if (!strcmp(val, "no"))
							{
								script = 0;
							}
						}
						else if (!strcmp(prag+5, "inlineErrors"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								inline_errors = 1;
							}
							else if (!strcmp(val, "no"))
							{
								inline_errors = 0;
							}
						}
						else if (!strcmp(prag+5, "sessions"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								sessions = 1;
							}
							else if (!strcmp(val, "no"))
							{
								sessions = 0;
							}
						}
						else if (!strcmp(prag+5, "recompile"))
						{
							char *p;
							p = val;
							while (*p)
							{
								*p = tolower(*p);
								p++;
							}
							if (!strcmp(val, "yes"))
							{
								clean = 1;
							}
							else if (!strcmp(val, "no"))
							{
								clean = 0;
							}
						}
						/*else if (!strcmp(prag+5, "package"))
						{
							printf("I AM A PACKAGE\n");
							exit(0);
						}*/
						else if (!strcmp(prag+5, "importLibrary")||!strcmp(prag+5, "library"))
						{
							char *tmpstr;
							tmpstr = (char*)malloc(strlen(val)+1);
							if (!tmpstr) oom();
							sprintf(tmpstr, "-l%s", val);
							append_cc_options(tmpstr);
							free(tmpstr);
						}
						else if (!strcmp(prag+5, "addSourceFile")||!strcmp(prag+5, "source"))
						{
							char *tmpstr;
							tmpstr = (char*)malloc(strlen(val)+1);
							if (!tmpstr) oom();
							sourcesum += cksum((unsigned char *)val);
							sprintf(tmpstr, "%s", val);
							append_cc_options(tmpstr);
							free(tmpstr);
						}
						else if (!strcmp(prag+5, "type"))
						{
							if (!strcmp(val, "app"))
							{
								//printf("Application\n");
							}
							else if (!strcmp(val, "lib"))
							{
								
							}
						}
					}
				}
				goto l__nextline;
			}
			else
			{
				if (i>=s)
				{
					s+=1024;	
					allocator = (char*)realloc(line, s);
					if (!allocator) oom();
					line = allocator;
				}
				line[i++] = c;
			}
		} 
	fclose(fsource);
}


/* 
	Produce the compilesum 
*/
static void
get_compilesum()
{
	compilesum = 
		cksumbin((unsigned char *)use_cc, strlen(use_cc))
		+cksumbin((unsigned char *)options, strlen(options))
		+cksumbin((unsigned char *)std, strlen(std))
	
	;
}

/*
	Build the directory name based on different globals
*/
static void
store_dirname()
{
	directory = (char*)malloc(strlen(pardir)+25);
	if (!directory) oom();
	sprintf(directory, "%s/%lu.%lu", pardir, sourcesum, compilesum);
}


static void
store_pardir()
{
	pardir = (char*)malloc(strlen(homedir)+strlen(sourcebase)+strlen(tempdir)+250);
	if (!pardir) oom();
	sprintf(pardir, "%s/.ccod", homedir);
	if (!create_directory(pardir))
	{
		sprintf(pardir, "%s/%s.ccod", tempdir, username);
		if (!create_directory(pardir))
		{
			print_error("error: could not create ccod cache directory.\n");
			exit(-1);
		}
		else
		{
			sprintf(pardir, "%s/%s.ccod/apps/%s.%lu", tempdir, username, sourcebase, pathsum);
		}
	}
	else
	{
		sprintf(pardir, "%s/.ccod/apps/%s.%lu", homedir, sourcebase, pathsum);
	}
}



static char *
basename(const char *path)
{
	char *ptr;
	ptr = (char*)path+strlen((char*)path);
	while (ptr>path)
	{
		if (*ptr=='/'||*ptr=='\\') return ptr+1;
		ptr--;
	}
	return ptr;
}

static void
clean_program()
{
	if (clean)
	{
		remove_directory(directory);
	}
	if (!execute) exit(0);
}


static void
store_sourcebase()
{
	strset(sourcebase, basename(sourcefull));
}


static void
store_progpath()
{
	progpath = (char*)malloc(strlen(directory)+strlen(sourcebase)+50);
	if (!progpath) oom();
	sprintf(progpath, "%s/%s", directory, sourcebase);
}


static void
store_progargs()
{
	char *allocator;
	int i;
	i = argidx+1;
	strset(progargs, "");
    for (;i<argc;i++)
    {
        allocator = (char*)realloc(progargs, strlen(progargs)+strlen(argv[i])+10);
        if (!allocator) oom();
        progargs = allocator;
        strcat(progargs, "\"");
        strcat(progargs, argv[i]);
        strcat(progargs, "\" ");
    }
	if (*progargs) progargs[strlen(progargs)-1] = 0;
}

static void
store_shellcmd()
{
	shellcmd = (char*)malloc(strlen(progpath)+strlen(progargs)+100);
	if (!shellcmd) oom();
	sprintf(shellcmd, "\"%s\" %s", progpath, progargs);
}


static void
create_progdir()
{
	if (!create_directory(directory))
	{
		print_error("error: could not create the program directory.\n");
		exit (-1);
	}
}

static int
copy_file(char *pathin, char *pathout)
{
	FILE *fin;
	FILE *fout;
	
	fin = fopen(pathin, "rb");
	if (!fin) return 0;
	
	fout = fopen(pathout, "wb+");
	if (!fout)
	{
		fclose(fin);
		return 0;
	}
	while (1)
	{
		char c;
		if (!fread(&c,1,1,fin)) break;
		fwrite(&c,1,1,fout);
	}

	fclose(fin);
	fclose(fout);
	return 1;
}

static void
clean_pardir()
{
/*	if (!remove_directory(pardir))
	{
		print_error("error: could not remove old cached version.\n");
		exit(-1);
	}
*/
	/* We dont want to check for errors because otherwise, if a copy of the 
	   program is in use, this compile will fail.
	*/
	remove_directory(pardir);
	
	if (!create_directory(directory))
	{
		print_error("error: could not create program directory.\n");
		exit(-1);
	}
}

static void
copy_source_file()
{
	char *tmp_cmd;
	tmp_source = (char*)malloc(strlen(directory)+strlen(sourcebase)+25);
	if (!tmp_source) oom();
	sprintf(tmp_source, "%s/tmp.%s", directory, sourcebase);
	tmp_cmd = (char*)malloc(strlen(source)+strlen(tmp_source)+25);
	if (!tmp_cmd) oom();
	copy_file(source, tmp_source);
	strset(source, tmp_source);
	free(tmp_cmd);
}



static void
modify_if_shell_script()
{
	FILE *fin;
	FILE *fout;

	char *tmp_source2;
	
	fin = fopen(tmp_source, "rb");
	if (!fin)
	{
		print_error("error: could not open temp source file.\n");
	}
	if (getc(fin)=='#'&&getc(fin)=='!')
	{
		shell_script = 1;
		tmp_source2 = (char*)malloc(strlen(tmp_source)+25);
		if (!tmp_source2) oom();
		sprintf(tmp_source2, "%s.script", tmp_source);
		fout = fopen(tmp_source2, "wb+");
		if (!fout)
		{
			print_error("error: could not create temp source script.\n");
			exit(-1);
		}
		fwrite("\n",1,1,fout);
		while (!feof(fin))
		{
			char c;
			fread(&c, 1, 1, fin);
			if (c=='\n')
			{
				while (!feof(fin))
				{
					char c;
					if (fread(&c, 1, 1, fin)==0) break;
					fwrite(&c, 1, 1, fout);
				}
				break;
			}
		}
		fclose(fin);
		fclose(fout);
		remove(tmp_source);
		fin = fopen(tmp_source2, "rb");
		if (!fin)
		{
			print_error("error: could not open input source for script tag stripping.\n");
			exit(-1);
		}
		fout = fopen(tmp_source, "wb+");
		if (!fin)
		{
			print_error("error: could not open output source for script tag stripping.\n");
			exit(-1);
		}
		while (!feof(fin))
		{
			char c;
			if (fread(&c, 1, 1, fin)==0) break;
			fwrite(&c, 1, 1, fout);
		}
		fclose(fin);	
		fclose(fout);	
		remove(tmp_source2);
	}
	else
	{
		fclose(fin);
	}
}

static void
modify_if_interface_script()
{
	char *cmd;
	char *hpath;
	FILE *fin;
	FILE *fout = 0;
	FILE *fscript;
	FILE *fhead;
	int enterstr = 1;
	int addlines = 0;
	char lcr = 0;
	char entertype = 0;
	int pss = 0;
	int i;
	int header_already = 0;
	int spacesfront = 1;
	char *spacesfront_str = 0, *spacesfront_alloc;
	int spacesfront_grow = 512;
	int spacesfront_size, spacesfront_len;
	
	
#if defined(WIN32) || defined(_WIN32)
	const char hguistr[] = "int WINAPI WinMain (HINSTANCE hInstance,HINSTANCE hPrevInstance,PSTR szCmdLine,int iCmdShow){";
	const char fguistr[] =	";return 0;}\n";
#else
	const char hguistr[] = "int main(int argc, char **argv){";
	const char fguistr[] = ";return 0;}\n";
#endif

	

	const char hcgistr[] = "void cspmain(){";
	const char fcgistr[] =	";return;}\n";

	const char hplainstr[] = "int main(int argc, char **argv){";
	const char fplainstr[] = ";return 0;}\n";

	
	

	if (cgi&&!script)
	{
		/* Add only the cgiinc.h line to the top of the file. */
		char incline[] = "#include <csp.h>\n";
		char *ntmp;
		FILE *fin, *fout;
		
		ntmp = (char*)malloc(strlen(tmp_source)+25);
		if (!ntmp) oom();
		sprintf(ntmp, "%s.2", tmp_source);
		fin = fopen(tmp_source, "rb");
		fout = fopen(ntmp, "wb+");
		if (!fin||!fout)
		{
			print_error("error: could not create no-script cgi temp source.\n");
			exit(-1);
		}
		fwrite(incline, 1, strlen(incline), fout);
		while (1)
		{
			char c;
			if (!fread(&c,1,1,fin)) break;
			fwrite(&c,1,1,fout);
		}
		fclose(fin);
		fclose(fout);
		remove(tmp_source);
		copy_file(ntmp, tmp_source);
		remove(ntmp);
		free(ntmp);
		return;
	}
	if (!script) return;
	
	cmd = (char*)malloc((strlen(tmp_source)*3)+100);
	if (!cmd) oom();

	hpath = (char*)malloc((strlen(tmp_source)*3)+100);
	if (!hpath) oom();

	/*  Modify script tags */
	fin = fopen(tmp_source, "rb");
	if (!fin)
	{
		print_error("error: could not open input source for script tag stripping.\n");
		exit(-1);
	}
	sprintf(cmd, "%s.script", tmp_source);
	sprintf(hpath, "%s.head", tmp_source);

	fscript = fopen(cmd, "wb+");
	fhead = fopen(hpath, "wb+");
	if (!fscript||!fhead)
	{
		print_error("error: could not open output source for script tag stripping.\n");
		exit(-1);
	}

	fout = fscript;

	
	if (cgi)
	{
		fwrite(hcgistr, 1, strlen(hcgistr), fout);
	}
	else if (gui)
	{
		fwrite(hguistr, 1, strlen(hguistr), fout);
	}
	else
	{
		fwrite(hplainstr, 1, strlen(hplainstr), fout);
	}
	if (objc)
	{
		fprintf(fout, "NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];\n");
	}
	
	
	
	
	//if (shell_script) fprintf(fout, "\n");
	//if (shell_script) addlines-=1;
	//if (cgi) addlines-=1;
	spacesfront = 1;
	spacesfront_str = (char*)malloc(spacesfront_grow+1);
	if (!spacesfront_str) oom();
	*spacesfront_str = 0;
	spacesfront_size = spacesfront_grow;
	spacesfront_len = 0;

	while (1)
	{
		char c = 0;
		char bc = 0;
		if (fread(&c, 1, 1, fin)==0) break;

		if (enterstr)
		{
			if (c=='<')
			{
				if (fread(&c, 1, 1, fin)==0)
				{
					c = '<';
					goto l__escapeit;
				}
				if (c=='?'||c=='#')
				{
					if (c == '?')
					{
						if (fread(&c, 1, 1, fin)==0) break;
						if (c != '#')
						{
							fseek(fin, -1, SEEK_CUR); // move back one character
							c = '?';
						}
					}
					
					enterstr = 0;
					entertype = c;
					
					
					if (c=='?')
					{
						if (!spacesfront)
						{
							fprintf(fout, "\");\n");
						}
					}
					else
					{
						if (header_already)
						{
							print_error("error: header data was already processed.\n");
							exit(-1);
						}
						header_already = 1;
					}
					if (!pss&&shell_script)
					{
						//addlines++;
						pss = 1;
					}
					
					fout = (c=='?')?fscript:fhead;
					
					for (i=0;i<addlines;i++)
					{
						fprintf(fout, "\n");
					}
					continue;
				}
				else
				{
					bc = '<';
				}
			}
l__escapeit:
			if (isspace(c)&&spacesfront)
			{
				if (spacesfront_len>=spacesfront_size-2)
				{
					spacesfront_str = (char*)realloc(spacesfront_str, spacesfront_size+spacesfront_grow+1);
					if (!spacesfront_str) oom();
				}
				
				spacesfront_str[spacesfront_len++] = '\\';
				switch (c)
				{
				case (' '): spacesfront_str[spacesfront_len++] = ' '; break;
				case ('\f'): spacesfront_str[spacesfront_len++] = 'f'; break;
				case ('\n'): spacesfront_str[spacesfront_len++] = 'n'; break;
				case ('\r'): spacesfront_str[spacesfront_len++] = 'r'; break;
				case ('\t'): spacesfront_str[spacesfront_len++] = 't'; break;
				case ('\v'): spacesfront_str[spacesfront_len++] = 'v'; break;
				}
				spacesfront_str[spacesfront_len] = 0;
				spacesfront_size += spacesfront_grow;
			}
			else if (!(isspace(c)&&spacesfront))
			{
				if (spacesfront)
				{
					if (cgi)
					{
						if (*spacesfront_str) fprintf(fout, ";\nResponse.Write(\"%s\");\n", spacesfront_str);
						fprintf(fout, ";\nResponse.Write(\"");
					}
					else
					{
						if (*spacesfront_str) fprintf(fout, ";\nprintf(\"%s\");\n", spacesfront_str);
						fprintf(fout, ";\nprintf(\"");
					}
					spacesfront = 0;	
				}
				if (bc)
				{
					fprintf(fout, "%c", bc);
				}
				switch (c)
				{
					case ('\b'): fprintf(fout, "\\b"); break;
					case ('\f'): fprintf(fout, "\\f"); break;
					case ('\n'): 
						if (!lcr) addlines++;
						fprintf(fout, "\\n");
						lcr = 0;
						break;
					case ('\r'): 
						addlines++;
						fprintf(fout, "\\r"); 
						break;
					case ('\t'): fprintf(fout, "\\t"); break;
					case ('\"'): fprintf(fout, "\\\""); break;
					case ('\''): fprintf(fout, "\\'"); break;
					case ('\\'): fprintf(fout, "\\\\"); break;
					case ('\v'): fprintf(fout, "\\v"); break;
					case ('\a'): fprintf(fout, "\\a"); break;
					case ('\?'): fprintf(fout, "\\?"); break;
					case ('%'):  fprintf(fout, "%%%%"); break;
					default: 
						if ( ((c>='0' && c<='~') || c==' ') && c != '%')
						{
							fwrite(&c, 1, 1, fout);	
						}
						else
						{
							fprintf(fout, "\" \"\\x%x\" \"", c);	
						}
				}
			}
			if (c=='\r') lcr = 1;
		}
		else
		{
			if (c=='?'||c=='#')
			{
				char lc = c;
				if (fread(&c, 1, 1, fin)==0)
				{
					fwrite(&c, 1, 1, fout);	
					break;
				}
				if (lc == '#' && c == '?')
				{
					if (fread(&c, 1, 1, fin)==0)
					{
						fwrite(&c, 1, 1, fout);	
						break;
					}
				}
				if (c=='>')
				{
					enterstr = 1;
					addlines = 0;
					entertype = 0;
					fout = fscript;
					if (lc=='?')
					{
						/*
						if (cgi)
						{
							fprintf(fout, ";\nResponse.Write(\"");
						}
						else
						{
							fprintf(fout, ";\nprintf(\"");
						}
						*/
					}
					spacesfront = 1;
					*spacesfront_str = 0;
					spacesfront_len = 0;
					continue;
				}
				else
				{
					fwrite(&lc,1,1,fout);
					
				}
			}
			fwrite(&c, 1, 1, fout);	

		}

	}

	if (!spacesfront)
	{
		fprintf(fout, "\");\n");
	}
	if (spacesfront_str) free(spacesfront_str);

	if (objc)
	{
		fprintf(fout, "[pool release];\n");
	}
	
	
	if (cgi)
	{
		fwrite(fcgistr, 1, strlen(fcgistr), fout);
	}
	else if (gui)
	{
		fwrite(fguistr, 1, strlen(fguistr), fout);
	}
	else
	{
		fwrite(fplainstr, 1, strlen(fplainstr), fout);
	}
	
	
	fclose(fhead);
	fclose(fin);	
	fclose(fscript);
	remove(tmp_source);
	fin = fopen(cmd, "rb");
	if (!fin)
	{
		print_error("error: could not open input source for script tag stripping.\n");
		exit(-1);
	}
	//sprintf(cmd, "%s.script", tmp_source);
	fout = fopen(tmp_source, "wb+");
	if (!fout)
	{
		print_error("error: could not open output source for script tag stripping.\n");
		exit(-1);
	}

	fprintf(fout, "#define csp\n");
	fprintf(fout, "#define ccod\n");

	if (cgi)
	{
		fprintf(fout, "#include <csp.h>\n");
	}
	else
	{
		fprintf(fout, "#include <stdio.h>\n");
		fprintf(fout, "#include <stdlib.h>\n");
	}
	
	if (objc)
	{
		fprintf(fout, "#import <Foundation/Foundation.h>\n");
		fprintf(fout, "#import <AppKit/AppKit.h>\n");
	}
	
	
	fhead = fopen(hpath, "rb");
	if (!fhead)
	{
		print_error("error: could not open input source for script tag stripping.\n");
		exit(-1);
	}
	for (;;)
	{
		char c;
		if (!fread(&c, 1, 1, fhead)) break;
		fwrite(&c, 1, 1, fout);
	}
	fclose(fhead);


	for (;;)
	{
		char c;
		if (!fread(&c, 1, 1, fin)) break;
		fwrite(&c, 1, 1, fout);
	}
	fclose(fin);	
	fclose(fout);
	remove(cmd);
	remove(hpath);
	
	free(cmd);
	free(hpath);
}

static void
modify_if_ssi()
{
	FILE *fin;
	FILE *fout;
	char *fpath;
	int res;
	char *tmp_source2;

	if (!script) return;
	
	tmp_source2 = (char*)malloc(strlen(tmp_source)+25);
	if (!tmp_source2) oom();
	sprintf(tmp_source2, "%s.script", tmp_source);
	fpath = full_path(orgsource);
	if (!fpath) oom();
	fin = fopen(tmp_source, "rb");
	fout = fopen(tmp_source2, "wb+");
	res = process_ssi_stream(fpath, fin, fout, NULL);
	fclose(fin);
	fclose(fout);
	free(fpath);
	//remove(tmp_source);
	copy_file(tmp_source2, tmp_source);
	remove(tmp_source2);
	free(tmp_source2);
	if (!res)
	{
		print_error("error: could not process ssi commands.\n");
		exit(-1);
	}
}

void
print_error(const char *errstr)
{
	if (getenv("SERVER_SOFTWARE"))
	{
		char *use_cc_version;
		char *compiler_version = 0;
		char *p;
		int cv_retval;
		char *outpath;
		
		
		outpath = tempnam(0,0);
		use_cc_version = (char*)malloc(strlen(use_cc)+100);
		if (!use_cc_version) oom();
		sprintf(use_cc_version, "%s --version", use_cc, outpath);
		
		cv_retval = system2file(use_cc_version, outpath, 1, 1);

		if (cv_retval)
		{
			strset(compiler_version, "unknown compiler");
		}
		else
		{
			FILE *f;
			f = fopen(outpath, "rt");
			if (!f)
			{
				strset(compiler_version, "unknown compiler");
			}
			else
			{
				size_t siz;
				fseek(f, 0, SEEK_END);
				siz = ftell(f);
				rewind(f);
				compiler_version = (char*)malloc(siz+1);
				if (!compiler_version) oom();
				memset(compiler_version, 0, siz+1);
				fread(compiler_version, 1, siz+1, f);
				fclose(f);
				remove(outpath);
			}
		}
		p = compiler_version;
		while(*p)
		{
			if (*p=='\r'||*p=='\n')
			{
				*p = 0;
				break;
			}
			p++;
		}


		fprintf(stdout, "Content-Type: text/html\n\n");
		fprintf(stdout, "<html>\r\n");
		fprintf(stdout, "    <head>\r\n");
		fprintf(stdout, "        <title>CCOD Compiler Error</title>\r\n");
		fprintf(stdout, "    </head>\r\n");
		fprintf(stdout, "    <body bgcolor=\"white\">\n");
		fprintf(stdout, "        <font face=\"arial,sans-serif\" color=\"black\">\r\n");
		fprintf(stdout, "        <hr size=\"1\"/>\r\n");
		fprintf(stdout, "        <font size=\"3\"><b>CCOD Compiler Error</b><br></font></h2>\r\n");
		fprintf(stdout, "        <font size=\"2\">CCOD encountered the following errors while compiling a source file.</font>\r\n");
		fprintf(stdout, "        <hr size=\"1\"/>\r\n");
		fprintf(stdout, "        <table cellpadding=15 cellspacing=0 border=0 "
				                 "bgcolor=silver width=\"100%%\"><td><pre>"
				                 "%s"
				        "</pre></td></table>",errstr);
		fprintf(stdout, "        <hr size=1><br>\n");
		if (verstl&&*verstl)
		{
			fprintf(stdout, "\n<i><b><font face=arial,sans-serif>CCOD Software:&nbsp;&nbsp;</font></b></i><br><pre>%s</pre>", verstl);
		}
		if (getenv("SERVER_SOFTWARE"))
		{
			fprintf(stdout, "\n<i><b><font face=arial,sans-serif>Server Software:&nbsp;&nbsp;</font></b></i><br><pre>%s</pre>", getenv("SERVER_SOFTWARE"));
		}
		if (sourcefull&&*sourcefull)
		{
			fprintf(stdout, "\n<i><b><font face=arial,sans-serif>Source Path:&nbsp;&nbsp;</font></b></i><br><pre>%s</pre>", sourcefull);
			if (source&&*source&&strcmp(sourcefull,source))
			{
				fprintf(stdout, "\n<i><b><font face=arial,sans-serif>Cached Path:&nbsp;&nbsp;</font></b></i><br><pre>%s</pre>", source);
			}
		}
		fprintf(stdout, "\n<i><b><font face=arial,sans-serif>Compiler:&nbsp;&nbsp;</font></b></i><br><pre>%s</pre>", compiler_version);
		fprintf(stdout, "\n<hr size=1>\n");
		fprintf(stdout, "</body>\n");
		fprintf(stdout, "</html>\n");
		remove(outpath);
		free(compiler_version);
		exit(0);
	}
	else
	{
		fprintf(stderr, "%s", errstr);
		exit(-1);
	}
}

static void
exec_compile_command()
{
	int retval;
	FILE *p;
	char c;
	char *line = 0, *lalloc;
	int ls, lidx;
	char *stri;
	char *finaloutput = 0;
	char *outpath;
	char *cgiheaderpath = 0;
	char *outdata = 0;
	int minuslines = 0;

	
	FILE *stdoutput;

	if (cc_cmd) free(cc_cmd);
	cc_cmd = (char*)malloc(strlen(orgworkpath)+strlen(orgsourcepath)+strlen(use_cc)+strlen(sourcebase)+strlen(options)+strlen(source)+strlen(progpath)+strlen(cgi_object)+100);
	if (!cc_cmd) oom();

	outpath = (char*)malloc(strlen(directory)+100);
	if (!outpath) oom();

	sprintf(outpath, "%s/errors.txt", directory);
	chdir(orgsourcepath);
	
	if (cgi)
	{
		cgiheaderpath = (char*)malloc(strlen(directory)+100);
		if (!cgiheaderpath) oom();
		sprintf(cgiheaderpath, "%s/cgiinc.h", directory);
	
		p = fopen(cgiheaderpath, "wb+");
		if (!p)
		{
			print_error("error: could not create the cgiinc.h file.\n");
			exit(-1);
		}
		fwrite(cgiheader, 1, sizeof(cgiheader), p);	
		fclose(p);
	}
	
	remove(outpath);
#if defined(WIN32) || defined(_WIN32)
	if (cgi)
	{
		/*
		sprintf(cc_cmd, "%s \"%s\" \"%s\" \"%s\" %s -std=%s -o  \"%s\"", use_cc, source, 
			cgi_object, cgi_dll,
			options, std, progpath);
			*/
		char *dllpath;
		dllpath = (char*)malloc(strlen(argv[0])+50);
		if (!dllpath) oom();
		
		strcpy(dllpath, argv[0]);
		dllpath[strlen(dllpath)-strlen(basename(dllpath))] = 0;
		strcat(dllpath, "libcsp.dll");
		
		
		sprintf(cc_cmd, "%s \"%s\" \"%s\" %s %s%s -o  \"%s\"", use_cc, source, 
			dllpath,
			options, *std?"-std=":"", *std?std:"", progpath);
			
		
		/*
		sprintf(cc_cmd, "%s \"%s\" -lcsp %s -std=%s -o  \"%s\"", use_cc, source, 
			options, std, progpath);
		*/
		free(dllpath);
	}
	else
	{
		sprintf(cc_cmd, "%s  \"%s\" %s %s%s -o  \"%s\"", use_cc, source, options, *std?"-std=":"", *std?std:"", progpath);
	}
	retval = system2file(cc_cmd, outpath, 1, 1);
#else
	if (cgi)
	{
		sprintf(cc_cmd, "%s \"%s\" -lpthread -lcsp %s %s%s -o  \"%s\" 2>&1", use_cc, source, 
			options, *std?"-std=":"", *std?std:"", progpath);
	}
	else
	{
		sprintf(cc_cmd, "%s \"%s\" %s %s%s -o  \"%s\" 2>&1", use_cc, source, options, *std?"-std=":"", *std?std:"", progpath);
	}
	
	outdata = pexec(cc_cmd, &retval);
	if (!outdata) oom();
	p = fopen(outpath, "wb+");
	if (!p)
	{
		print_error("error: could not create the error file (1).\n");
		exit(-1);		
	}
	fprintf(p, "%s", outdata);
	fclose(p);
#endif
	if (cgi)
	{
		remove(cgiheaderpath);
		free(cgiheaderpath);
	}
	
	p = fopen(outpath, "rb");
	if (!p)
	{
		print_error("error: could not open the error file (2).\n");
		exit(-1);
	}
	
	finaloutput = (char*)malloc(1);
	if (!finaloutput) oom();;
	*finaloutput = 0;
	
	if (script&&inline_errors)
	{
		stdoutput = stdout;
	}
	else
	{
		stdoutput = stderr;
	}
	
	
	line = (char*)malloc(250+50);
	if (!line) oom();
	ls = 250;
	
	*line = 0;
	lidx = 0;
	while (!feof(p))
	{
		char *lp, *nstr, *np, nstrac[30];
		size_t ln;
		c = getc(p);
		line[lidx] = c;
		if (line[lidx]=='\r'||line[lidx]=='\n')
		{
			lidx++;
			line[lidx] = 0;

			/* Make sure the line contains the correct line number. */
			lp = line;
			while (*lp)
			{
				if (*lp==':')
				{
					nstr = lp+1;
					lp++;
					while (*lp)
					{
						if (*lp==':')
						{
							*lp = 0;
							np = nstr;
							while (*np)
							{
								if (!isdigit(*np)) 
								{
									*lp = ':';
									goto l__eol;
								}
								np++;
							}
							ln = atol(nstr);
							if (script) ln-=6;
							if (cgi) ln+=1;
							if (cgi&&!script) ln-=2;
							sprintf(nstrac, "%lu", ln);
							*lp = ':';
							{
								char *nline;
								nline = (char*)malloc(strlen(line)+strlen(nstrac)+1);
								if (!nline) oom();
								*(nstr-1) = 0;
								sprintf(nline, "%s:%s:%s", line, nstrac, lp+1);
								strcpy(line, nline);
								free(nline);
							}
							
							//memmove(nstr+strlen(nstrac), lp, strlen(lp));
							//sprintf(nstr, "%s", nstrac);
							//*lp = ':';
							//printf("[%d]\n", ln);
							
							
							goto l__eol;
						}
						lp++;
					}
				}
				lp++;
			}
l__eol:
			
			
			finaloutput = (char*)realloc(finaloutput, 
					strlen(finaloutput)+strlen(orgsource)+
					strlen(line)+strlen(source)+50);
			if (!finaloutput) oom();
			
			
			stri = (char*)strstr((char*)line, (char*)source);										 

			
			
			
			if (stri==line)
			{
				//fprintf(stdoutput, "%s", orgsource);
				//fprintf(stdoutput, "%s", line+strlen(source));
				sprintf(finaloutput+strlen(finaloutput), "%s", orgsource);
				sprintf(finaloutput+strlen(finaloutput), "%s", line+strlen(source));
			}
			else if (stri>line)
			{
				line[stri-line] = 0;
				//fprintf(stdoutput, "%s", line);
				//fprintf(stdoutput, "%s", orgsource);
				//fprintf(stdoutput, "%s", line+strlen(source)+(stri-line));
				sprintf(finaloutput+strlen(finaloutput), "%s", line);
				sprintf(finaloutput+strlen(finaloutput), "%s", orgsource);
				sprintf(finaloutput+strlen(finaloutput), "%s", line+strlen(source)+(stri-line));
			}
			else
			{
				stri = (char*)strstr((char*)line, basename(source));
				if (stri==line)
				{
					//fprintf(stdoutput, "%s", orgsource);
					//fprintf(stdoutput, "%s", line+strlen(source));
					sprintf(finaloutput+strlen(finaloutput), "%s", basename(orgsource));
					sprintf(finaloutput+strlen(finaloutput), "%s", line+strlen(basename(source)));
				}
				else if (stri>line)
				{
					line[stri-line] = 0;
					//fprintf(stdoutput, "%s", line);
					//fprintf(stdoutput, "%s", orgsource);
					//fprintf(stdoutput, "%s", line+strlen(source)+(stri-line));
					sprintf(finaloutput+strlen(finaloutput), "%s", line);
					sprintf(finaloutput+strlen(finaloutput), "%s", basename(orgsource));
					sprintf(finaloutput+strlen(finaloutput), "%s", line+strlen(basename(source))+(stri-line));
				}
				else
				{
					//fprintf(stdoutput, "%s", line);
					sprintf(finaloutput+strlen(finaloutput), "%s", line);
				}
			}
			lidx = 0;
			continue;
		}
		lidx++;
		if (lidx>ls)
		{
			ls+=250;
			lalloc = (char*)malloc(ls+50);
			if (!lalloc) oom();
			line = lalloc;
		}
	}
	if (line) free(line);
	fclose(p);
	remove(outpath);

	if (stdoutput == stderr)
	{
		if (retval) 
		{
			fprintf(stderr, "%s", finaloutput);
			exit(retval);
		}
	}
	else
	{
		if (retval) 
		{
			print_error(finaloutput);
			exit(0);
		}
	}
	
	if (finaloutput) free(finaloutput);
	chdir(orgworkpath);

}

static void
remove_source_copy()
{
	remove(source);
	if (cgi)
	{
		char *cgisource;
		cgisource = (char*)malloc(strlen(directory)+50);
		if (!cgisource) oom();
		sprintf(cgisource, "%s/cgiinc.c", directory);
		remove(cgisource);
		free(cgisource);
	}
}

static void
begin_compile()
{
	FILE *f;
	if (directory_exists(progpath))
	{
		print_error("error: progam file is a directory.\n");
		exit(-1);
	}
	f = fopen(progpath, "rb");
	if (!f)
	{
		
		
		clean_pardir();
		copy_source_file();
		
		modify_if_shell_script();
		modify_if_ssi();
		modify_if_interface_script();
		//modify_if_cgi();
		exec_compile_command();
		remove_source_copy();
	}
	else
	{
		fclose(f);		
	}
}


static void
store_orgworkpath()
{
	char *pwd = (char*)getcwd((char*)NULL,0);
	char *p;
	if (!pwd||!*pwd)
	{
		//print_error("error: could not get the working directory.\n");
		//exit(-1);
		pwd = ".";
	}
	strset(orgworkpath, pwd);
	p = orgworkpath;
	while (*p)
	{
		if (*p=='\\') *p = '/';
		p++;
	}
	free(pwd);
}

static void
change_working_directory()
{
    char *cdcmd;
	if (getenv("PATH_TRANSLATED"))
	{
		strset(cdcmd, (char*)getenv("PATH_TRANSLATED"));
		cdcmd[strlen(cdcmd)-strlen(basename(cdcmd))] = 0;
		chdir(cdcmd);
		free(cdcmd);
	}
		
	
	//char *cdcmd;
    //if (!strcmp(workdir, ".")||!strcmp(workdir, "")) return;
    //cdcmd = (char*)malloc(strlen(workdir)+strlen(shellcmd)+100);
    //if (!cdcmd) oom();
    //sprintf(cdcmd, "cd \"%s\" && cd \"%s\" && %s", orgsourcepath, workdir, shellcmd);
    //strset(shellcmd, cdcmd);
    //free(cdcmd);
}



int systemc(const char *command)
{
#if defined(_WIN32) || defined(WIN32)
	return system2file(command,0,0,0);
#else
	return system(command);
#endif
}


static void
execute_final_program()
{
	int retval;
	char *fshellcmd;
	char maxulsizstr[50]; 
	fshellcmd = (char*)malloc(strlen(shellcmd)+25);
	if (!fshellcmd) oom();
	chdir(workdir);
	
	setenv("CCOD_SOFTWARE", verstl, 1);
	setenv("CCOD_PROGPATH", progpath, 1);
	setenv("CCOD_SESSIONS", sessions?"yes":"no", 1);
	setenv("CCOD_CSPINUSE", cgi?"yes":"no", 1);
	setenv("CCOD_IGMAXULE", ignmaxerr?"yes":"no", 1);
	sprintf(maxulsizstr, "%lu", max_upload_size);
	setenv("CCOD_MAXULSIZ", maxulsizstr, 1);
	
	

	sprintf(fshellcmd, "%s", shellcmd);
	if (fshellcmd[strlen(fshellcmd)-1] == ' ') fshellcmd[strlen(fshellcmd)-1] = 0;
	retval = systemc(fshellcmd);
	chdir(orgsourcepath);
	fretval = retval;
	free(fshellcmd);
}


static void
store_orgsourcepath()
{
	strset(orgsourcepath, sourcefull);
	orgsourcepath[strlen(orgsourcepath)-strlen(basename(sourcefull))-1] = 0;
}

static int
process_ssi_stream(const char *path, FILE *fin, FILE *fout, unsigned long *addcksum)
{

	char ch;
	char ssiotag[6];
	char *ssiline = 0, *ssilinealloc;
	size_t ssilinelen;
	size_t ssilinemax;
	char *command, *p, *attr, *val;
	
	size_t read;
	
	if (!fin) goto l__err;
	//if (!fout) goto l__err;
	
	ssilinemax = 500;
	ssiline = (char*)malloc(ssilinemax+25);
	if (!ssiline) goto l__err;
	*ssiline = 0;
	ssilinelen = 0;
	
	for (;;)
	{
		if (!fread(&ch,1,1,fin)) goto l__eof;
		if (ch=='<')
		{
			memset(ssiotag, 0, sizeof(ssiotag));
			read = fread(ssiotag,1,4,fin);
			if (read!=4)
			{
				if (fout) fprintf(fout, "<%s", ssiotag);
				goto l__eof;
			}
			else if (strcmp(ssiotag, "!--#"))
			{
				if (fout) fprintf(fout, "<%s", ssiotag);
				continue;
			}
			else
			{
				ssilinelen = 0;
				for (;;)
				{
					if (!fread(&ch,1,1,fin)) goto l__eof;
					if (ch=='-')
					{
						if (!fread(&ch,1,1,fin)) goto l__eof;
						if (ch=='-')
						{
							if (!fread(&ch,1,1,fin)) goto l__eof;		
							if (ch=='>')
							{
								break;
							}
							ssiline[ssilinelen++] = '-';
							ssiline[ssilinelen++] = '-';
							ssiline[ssilinelen++] = ch;						
							continue;
						}
						ssiline[ssilinelen++] = '-';
						ssiline[ssilinelen++] = ch;						
						continue;
					}
					if (ssilinelen>=ssilinemax)
					{
						ssilinemax+=500;
						ssilinealloc = (char*)realloc(ssiline, ssilinemax+25);
						if (!ssilinealloc) goto l__err;
						ssiline = ssilinealloc;
					}
					ssiline[ssilinelen++] = ch;
				}
				ssiline[ssilinelen] = 0;
				/* parse the ssi command */
				p = ssiline;
				while (*p&&isspace(*p)) p++;
				command = p;
				while (*p)
				{
					if (isspace(*p))
					{
						*p = 0;
						p++;
						break;
					}
					*p = tolower(*p);
					p++;
				}
				if (!strcmp(command, "include"))
				{
					while (*p&&isspace(*p)) p++;
					attr = p;
					while (*p)
					{
						if (isspace(*p)||*p=='=')
						{
							*p = 0;
							p++;
							break;
						}
						*p = tolower(*p);
						p++;
					}
					while (*p&&isspace(*p)) p++;
					if (*p=='=') p++;
					while (*p&&isspace(*p)) p++;
					if (*p=='\"'||*p=='\'')
					{
						char qp = *p;
						p++;
						val = p;
						while (*p)
						{
							if (*p==qp)
							{
								*p = 0;
								p++;
								break;
							}
							p++;
						}
					}
					else
					{
						val = p;
						while (*p)
						{
							if (isspace(*p))
							{
								*p = 0;
								p++;
								break;
							}
							p++;
						}
					}
					if (!strcmp(attr, "file"))	
					{
						if (!strstr(val, "../")&&*val!='/'&&!strstr(val, "/..")&&((char*)strstr(val, "..")!=(char*)val))
						{
							FILE *fin2;
							char *dir;
							char *fpath;
							char *p3;
							dir = (char*)malloc(strlen(path)+1);
							if (!dir) goto l__err;
							strcpy(dir, path);
							p3 = dir+strlen(dir);
							while (p3>dir)
							{
								if (*p3=='/')
								{
									*p3 = 0;
									break;
								}
								p3--;
							}
							fpath = (char*)malloc(strlen(dir)+strlen(val)+2);
							if (!fpath)
							{
								free(dir);
								goto l__err;
							}
							sprintf(fpath, "%s/%s", dir, val);
							free(dir);
							dir = full_path(fpath);
							if (!dir)
							{
								free(fpath);
								goto l__err;
							}
							free(fpath);
							fpath = dir;
							dir = 0;
							
							fin2 = fopen(fpath, "rb");
							if (!fin2)
							{
								free(fpath);
								goto l__err;
							}
							if (!process_ssi_stream(fpath, fin2, fout, addcksum))
							{
								fclose(fin2);
								free(fpath);
								goto l__err;
							}
							fclose(fin2);
							if (addcksum) *addcksum += cksum((unsigned char *)fpath);
							free(fpath);
						}
					}
					else if (!strcmp(attr, "virtual"))
					{
						if (getenv("DOCUMENT_ROOT")&&*val=='/'&&!strstr(val, "../")&&!strstr(val, "/..")&&((char*)strstr(val, "..")!=(char*)val))
						{
							char *fpath;
							FILE *fin2;
							fpath = (char*)malloc(strlen((char*)getenv("DOCUMENT_ROOT"))+strlen(val)+1);
							if (!fpath) goto l__err;
							sprintf(fpath, "%s%s", getenv("DOCUMENT_ROOT"), val);
							fin2 = fopen(fpath, "rb");
							if (!fin2)
							{
								free(fpath);
								goto l__err;
							}
							if (!process_ssi_stream(fpath, fin2, fout, addcksum))
							{
								fclose(fin2);
								free(fpath);
								goto l__err;
							}
							fclose(fin2);
							if (addcksum) *addcksum += cksum((unsigned char *)fpath);
							free(fpath);
						}
						else
						{
							goto l__err;
						}
					}
				}
				else if (!strcmp(command, "echo"))
				{
					
					
				}
				else if (!strcmp(command, "printenv"))
				{
					//printf("<?system(\"set\");?>\n");
					
				}
				continue;
			}
		}
		if (fout) { if (!fwrite(&ch,1,1,fout)) goto l__err; }
	}
l__eof:
	if (ssiline) free(ssiline);
	return 1;
l__err:
	if (ssiline) free(ssiline);
	return 0;
}


#if 0
static int
process_ssi_file(const char *pathin, const char *pathout)
{
	FILE *fin;
	FILE *fout;
	char *fpath;
	int res;
	fpath = full_path(pathin);
	if (!fpath) return 0;
	fin = fopen(fpath, "rb");
	fout = fopen(pathout, "wb+");
	res = process_ssi_stream(fpath, fin, fout, NULL);
	fclose(fin);
	fclose(fout);
	free(fpath);
	return res;
}
#endif


static void
add_ssiincludesum()
{
	FILE *fin;
	
	fin = fopen(sourcefull, "rb");
	if (!fin)
	{
		print_error("error: error opening source file.\n");
		exit(-1);
	}
	if (!process_ssi_stream(sourcefull, fin, NULL, &sourcesum))
	{
		print_error("error: ssi instruction error.\n");
		exit(-1);
	}
	fclose(fin);
	
}


#define TIMESTART() \
{\
	clock_t start, finish;\
	double  duration;\
	start = clock();

#define TIMEEND() \
	finish = clock();\
	duration = (double)(finish - start) / CLOCKS_PER_SEC;\
	printf( "%7.6f seconds\n", duration );\
}

void
detect_compiler()
{
	char *p, *ext;
	p = source+strlen(source)-1;
	while (p>source)
	{
		if (*p=='.') break;
		p--;
	}
	ext = (char*)malloc(strlen(p)+1);
	if (!ext) oom();
	strcpy(ext, p);
	if (!strcmp(ext, ".c")) 
	{
		strset(use_cc, "gcc");
		strset(std, "c99");

	}
	else
	{
		p = ext;
		while (*p)
		{
			*p = tolower(*p);
			p++;
		}
		if (!strcmp(ext, ".cc")||!strcmp(ext, ".cp")||!strcmp(ext, ".cxx")||
			!strcmp(ext, ".cpp")||!strcmp(ext, ".c++")||!strcmp(ext, ".C")) 
		{
			strset(use_cc, "g++");
			strset(std, "c++98");
		}
		else if (!strcmp(ext, ".m"))
		{
			objc = 1;
			strset(std, "c99");
		}
		
		
	}
	free(ext);
}

#if defined(WIN32) || defined(_WIN32)
int WINAPI WinMain (HINSTANCE hInstance,HINSTANCE hPrevInstance,PSTR szCmdLine,int iCmdShow) 
{
	argc = __argc;
	argv = __argv;
#else
int 
main(int in_argc, char **in_argv)
{
	argc = in_argc;
	argv = in_argv;
	
#endif
#if defined(WIN32) || defined(_WIN32)

	if (setmode(fileno(stdin), O_BINARY) == -1) 
	{
		print_error("error: could not set binary mode for stdin.\n");
		exit(-1);
	}
    if (setmode(fileno(stdout), O_BINARY) == -1)
	{
		print_error("error: could not set binary mode for stdout.\n");
		exit(-1);
	}
	if (getenv("PATH"))
	{
		char envvar[5000];
		char *npath;
		char *ndir = 0;

		npath = (char*)malloc(strlen(getenv("PATH"))+strlen(argv[0])+50);
		if (!npath) oom();
		strset(ndir, argv[0]);
		ndir[strlen(ndir)-strlen(basename(ndir))-1] = 0;
		sprintf(npath, "%s;%s", ndir, getenv("PATH"));
		SetEnvironmentVariable("PATH", npath);
		GetEnvironmentVariable("PATH", envvar, sizeof(envvar));
		free(npath);
		free(ndir);
		
	}
#endif
	
	
	

	


	/* Reset all the Global Variables. */
	reset_globals();

	/* Read Command Line Arguments. */
	read_arguments();

	/* Locate the users temp directory. */
	store_tempdir();

	

	/* Get the user name. */
	store_username();

	/* Locate the users home directory. */
	store_homedir();


	if (!tempdir||!*tempdir)
	{
		print_error("error: could not lookup the temp directory.\n");
		exit(-1);
	}
	else if (!username||!*username)
	{
		print_error("error: could not lookup the user/login name.\n");
		exit(-1);
	}
	else if (!homedir||!*homedir)
	{
		print_error("error: could not lookup the home directory.\n");
		exit(-1);
	}
	


	//printf("\n");
	//printf("User Name: [%s]\nHome Dir:[%s]\nTemp Dir:[%s]\n", username, homedir, tempdir);
	
	/* Read and Check the source file. */
	store_source();

	/* Read and Check source full path. */
	store_sourcefull();

	store_orgworkpath();
	store_orgsourcepath();
	
	/* Read the base name of the source. */
	store_sourcebase();
	
	/* Calculate the source file sum. */
	get_sourcesum();

	/* Add ssi includes to cksum. */
	add_ssiincludesum();
	
	/* Calculate the source path sum. */
	get_pathsum();
	
	if (!source||!*source||!sourcefull||!*sourcefull)
	{
		print_error("error: source file error.\n");
		exit(-1);
	}
	else if (!sourcesum)
	{
		print_error("error: could not calculate the source cksum.\n");
		exit(-1);
	}
	else if (!pathsum)
	{
		print_error("error: could not calculate the path cksum.\n");
		exit(-1);
	}
	
	
	//printf("Source File: [%s]\n", source);
	//printf("Source Full: [%s]\n", sourcefull);
	//printf("Org Source: [%s]\n", orgsourcepath);
	//printf("Org Working: [%s]\n", orgworkpath);
	//printf("Source Sum: [%u]\n", sourcesum);
	//printf("Path Sum: [%u]\n", pathsum);
	
	/* Auto detect compiler */
	detect_compiler();
	
	/* Read the Pragmas from the source file. */
	read_pragmas();
	
	/* Objective-C Support (Mac OS X) */
	detect_objc();
	
	/* Produce a compilesum from the command line cc options. */
	get_compilesum();

	/* Build the parent directory name. */
	store_pardir();
	
	/* Build the directory name. */
	store_dirname();

	if (!directory||!*directory)
	{
		print_error("error: could not build the output directory path.\n");
		exit(-1);
	}
	
	//printf("Compile Sum: [%u]\n", compilesum);
	//printf("Parent Directory: [%s]\n", pardir);
	//printf("Directory: [%s]\n", directory);

	/* Build the program path. */
	store_progpath();
	
	if (!progpath||!*progpath)
	{
		print_error("error: could not build the output program path.\n");
		exit(-1);
	}
	
	//printf("Program Path: [%s]\n", progpath);
	
	/* Store the arguments that will be passed to the program when executed. */
	store_progargs();

	/* Build and store the shell command of the executed program. */
	store_shellcmd();
	
	//printf("Program Arguments: [%s]\n", progargs);
	//printf("Shell Command: [%s]\n", shellcmd);

	/* Clean the program if the option was passed. */
	clean_program();


	/* Create the program directory. */
	create_progdir();

	/* Compile the CGI */
	//compile_cgi();


	/* Begin the compile process. */
	begin_compile();

	/* Make sure the correct working directory is selected. */
	change_working_directory();

	/* Here it is.  This is what we have been waiting for. */
	//TIMESTART();

	execute_final_program();
	//TIMEEND();

	

	return fretval;
}


