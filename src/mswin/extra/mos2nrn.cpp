// this is a copy of neuron.exe but no space handling and calls
// mos2nrn.sh

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

char* back2forward(const char*);

char* nrnhome;
char* nh;

static void setneuronhome() {
	int i, j;
	char buf[256];
	GetModuleFileName(NULL, buf, 256);
	for (i=strlen(buf); i >= 0 && buf[i] != '\\'; --i) {;}
	buf[i] = '\0'; // /neuron.exe gone
	for (i=strlen(buf); i >= 0 && buf[i] != '\\'; --i) {;}
	buf[i] = '\0'; // /bin gone
	nrnhome = new char[strlen(buf)+1];
	strcpy(nrnhome, buf);
}

char* back2forward(const char* back) {
	char *forward;
	char *cp;
   	forward = new char[strlen(back) + 1];
		strcpy(forward, back);
		for (cp = forward; *cp; ++cp) {
			if (*cp == '\\') {
				*cp = '/';
			}
		}
	return forward;
}

static char* argstr(int argc, char** argv) {
	// put args into single string, each with enclosing ""
	int i, j, cnt;
	char* s;
	char* a;
	cnt = 100;
	for (i=1; i < argc; ++i) {
		cnt += strlen(argv[i])+2;
	}
	s = new char[cnt];
	j = 0;
	for (i=1; i < argc; ++i) {
		s[j++] = '"';
		// convert \ to /
		for (a = argv[i]; *a; ++a) {
			if (*a == '\\') {
				s[j++] = '/';
			}else{
				s[j++] = *a;
			}
		}
		s[j++] = '"';
		if (i < argc-1) {
			s[j++] = ' ';
		}
	}
	s[j] = '\0';
	return s;
}
	
int main(int argc, char** argv) {
	int err;
	char* buf;
	char* args;
	char* msg;

	setneuronhome();
	nh = back2forward(nrnhome);
	args = argstr(argc, argv);
	buf = new char[strlen(args) + 3*strlen(nh) + 200];
	sprintf(buf, "%s\\bin\\sh %s/lib/mos2nrn.sh %s %s", nrnhome, nh, nh, args);
	msg = new char[strlen(buf) + 100];
	err = WinExec(buf, SW_SHOW);
	if (err < 32) {
		sprintf(msg, "Cannot WinExec %s\n", buf);
		MessageBox(0, msg, "NEURON", MB_OK);
	}
	return 0;
}