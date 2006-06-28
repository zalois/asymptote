/*****
 * util.cc
 * John Bowman
 *
 * A place for useful utility functions.
 *****/

#include <cassert>
#include <iostream>
#include <cstdio>
#include <cfloat>
#include <sstream>
#include <cerrno>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "util.h"
#include "settings.h"
#include "errormsg.h"
#include "camperror.h"
#include "interact.h"

using namespace settings;

#ifdef __CYGWIN__
#define CERR cout
#else
#define CERR cerr
#endif  

bool False=false;

string stripext(const string& name, const string& ext)
{
  string suffix = "." + ext;
  size_t p=name.rfind(suffix);
  if (p == name.length()-suffix.length())
    return name.substr(0,p);
  else return name;
}

void backslashToSlash(string& s) 
{
  size_t p;
  while ((p=s.find('\\')) < string::npos)
    s[p]='/';
}

void spaceToUnderscore(string& s) 
{
  size_t p;
  while ((p=s.find(' ')) < string::npos)
    s[p]='_';
}

#ifdef __CYGWIN__
string Getenv(const char *name, bool quote)
{
  char *s=getenv(name);
  if(!s) return "";
  string S=string(s);
  backslashToSlash(S);
  if(quote) S="'"+S+"'";
  return S;
}
#else
string Getenv(const char *name, bool)
{
  char *s=getenv(name);
  if(!s) return "";
  return string(s);
}
#endif

string& stripDir(string& name)
{
  size_t p;
#ifdef __CYGWIN__  
  p=name.rfind('\\');
  if(p < string::npos) name.erase(0,p+1);
#endif  
  p=name.rfind('/');
  if(p < string::npos) name.erase(0,p+1);
  return name;
}

string buildname(string name, string suffix, string aux, bool stripdir) 
{
  if(stripdir) stripDir(name);
    
  name = stripext(name,getSetting<mem::string>("outformat"));
  name += aux;
  if(!suffix.empty()) name += "."+suffix;
  return name;
}

string auxname(string filename, string suffix)
{
  return buildname(filename,suffix,"_");
}
  
bool checkFormatString(const string& format)
{
  if(format.find(' ') != string::npos) { // Avoid potential security hole
    ostringstream msg;
    msg << "output format \'" << format << "\' is invalid";
    camp::reportError(msg);
  }
  return true;
}
  
// Return an argv array corresponding to the fields in command delimited
// by spaces not within matching single quotes.
char **args(const char *command)
{
  if(command == NULL) return NULL;
  
  size_t n=0;
  char **argv=NULL;  
  for(int pass=0; pass < 2; ++pass) {
    if(pass) argv=new char*[n+1];
    ostringstream buf;
    const char *p=command;
    bool empty=true;
    bool quote=false;
    n=0;
    char c;
    while((c=*(p++))) {
      if(!quote && c == ' ') {
	if(!empty) {
	  if(pass) {
	    argv[n]=strcpy(new char[buf.str().size()+1],buf.str().c_str());
	    buf.str("");
	  }
	  empty=true;
	  n++;
	}
      } else {
	empty=false;
	if(c == '\'') quote=!quote;
	else if(pass) buf << c;
      }
    }
    if(!empty) {
      if(pass) argv[n]=strcpy(new char[buf.str().size()+1],buf.str().c_str());
      n++;
    }
  }
  
  if(settings::verbose > 1) {
    cerr << argv[0];
    for(size_t m=1; m < n; ++m) cerr << " " << argv[m];
    cerr << endl;
  }
  
  argv[n]=NULL;
  return argv;
}

void execError(const char *command, const char *hint, const char *application)
{
    CERR << "Cannot execute " << command << endl;
    if(application == "") application=hint;
    string s=string(hint);
    transform(s.begin(), s.end(), s.begin(), toupper);        
    if(hint) 
      CERR << "Please put in " << getSetting<mem::string>("config")
	   << ": " << endl << endl
	   << "import settings;" << endl
           << hint << "=\"PATH\";" << endl << endl
	   << "where PATH denotes the correct path to " 
	   << application << "." << endl << endl
	   << "Alternatively, set the environment variable ASYMPTOTE_" << s 
	   << endl << "or use the command line option -" << hint 
	   << "=\"PATH\"" << endl;
    exit(-1);
}
						    
// quiet: 0=none; 1=suppress stdout; 2=suppress stdout+stderr.

int System(const char *command, int quiet, bool wait,
	   const char *hint, const char *application, int *ppid)
{
  int status;

  if(!command) return 1;

  cout.flush(); // Flush stdout to avoid duplicate output.
    
  char **argv=args(command);

  int pid = fork();
  if(pid == -1)
    camp::reportError("Cannot fork process");
  
  if(pid == 0) {
    if(interact::interactive) signal(SIGINT,SIG_IGN);
    if(quiet) {
      static int null=creat("/dev/null",O_WRONLY);
      close(STDOUT_FILENO);
      dup2(null,STDOUT_FILENO);
      if(quiet == 2) {
	close(STDERR_FILENO);
	dup2(null,STDERR_FILENO);
      }
    }
    if(argv) {
      execvp(argv[0],argv);
      execError(argv[0],hint,application);
    }
  }

  if(ppid) *ppid=pid;
  for(;;) {
    if(waitpid(pid, &status, wait ? 0 : WNOHANG) == -1) {
      if(errno == ECHILD) return 0;
      if(errno != EINTR) {
	if(quiet < 2) {
	  ostringstream msg;
	  msg << "Command failed: " << command;
	  camp::reportError(msg);
	}
      }
    } else {
      if(!wait) return 0;
      if(WIFEXITED(status)) {
	if(argv) {
	  char **p=argv;
	  char *s;
	  while((s=*(p++)) != NULL)
	    delete [] s;
	  delete [] argv;
	}
	return WEXITSTATUS(status);
      } else {
	if(quiet < 2) {
	  ostringstream msg;
	  msg << "Command exited abnormally: " << command;
	  camp::reportError(msg);
	}
      }
    }
  }
}

int System(const ostringstream& command, int quiet, bool wait,
	   const char *hint, const char *application, int *pid)
{
  return System(command.str().c_str(),quiet,wait,hint,application,pid);
}

string stripblanklines(string& s)
{
  bool blank=true;
  const char *t=s.c_str();
  size_t len=s.length();
  
  for(size_t i=0; i < len; i++) {
    if(t[i] == '\n') {
      if(blank) s[i]=' ';
      else blank=true;
    } else if(t[i] != '\t' && t[i] != ' ') blank=false;
  }
  return s;
}

static char *startpath=NULL;
char *currentpath=NULL;

char *startPath()
{
  return startpath;
}

void noPath()
{
  camp::reportError("Cannot get current path");
}

char *getPath(char *p)
{
  static int size=MAXPATHLEN;
  if(!p) p=new char[size];
  if(!p) noPath();
  else while(getcwd(p,size) == NULL) {
    if(errno == ERANGE) {
      size *= 2;
      delete [] p;
      p=new char[size];
    } else {noPath(); p=NULL;}
  }
  return p;
}

int setPath(const char *s)
{
  if(s != NULL && *s != 0) {
    if(startpath == NULL) startpath=getPath(startpath);
    return chdir(s);
  } return 0;
}
