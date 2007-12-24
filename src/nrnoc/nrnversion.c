#include <stdio.h>
#include <string.h>
#include <nrnversion.h>

static char buf[256];

char* nrn_version(int i) {
	char head[50];
	char tail[50];
	char *cp;
	int b;
	buf[0] = '\0';
	if (strncmp(SVN_BRANCH, "Release", 7) == 0) {
		sprintf(head, "%s", SVN_BRANCH);
	}else{
		sprintf(head, "VERSION %s.%s%s%s",
			NRN_MAJOR_VERSION, NRN_MINOR_VERSION,
			((strcmp(SVN_BRANCH, "") == 0) ? "" : "."), SVN_BRANCH);
	}
	sprintf(tail, " %s", SVN_CHANGESET);
	b = 0;
	for (cp = tail; *cp; ++cp) {
		if (*cp == ' ' || *cp == '(' || *cp == ')' ||
		  (*cp >= '0' && *cp <= '9')) {
			;
		}else{
			b = 1;
			break;
		}
	}
	if (b == 0) {
		tail[0] = '\0';
	}
	if (i == 0) {
		sprintf(buf, "%s.%s", NRN_MAJOR_VERSION, NRN_MINOR_VERSION);
	}else if (i == 2) {
		sprintf(buf, "%s.%s", head,SVN_TREE_CHANGE);
	}else if (i == 3) {
		sprintf(buf, "%s", SVN_BASE_CHANGESET);
	}else if (i == 4) {
		sprintf(buf, "%s", SVN_DATE);
	}else if (i == 5) {
		sprintf(buf, "%s", SVN_CHANGESET);
	}else{
		sprintf(buf, "NEURON -- %s.%s (%s) %s%s", head,
			SVN_TREE_CHANGE,
			SVN_BASE_CHANGESET, SVN_DATE,
			tail); 
	}
	return buf;
}