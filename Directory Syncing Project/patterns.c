#include "mysync.h"

// converts a glob string to a regular expression
char *glob2regex(char *glob)
{
    char *re = NULL;

    if(glob != NULL) {
	re	= calloc(strlen(glob)*2 + 4, sizeof(char));
        if(re == NULL) {
	    return NULL;
	}

	char *r	= re;

	*r++	= '^';
	while(*glob != '\0')
	    switch (*glob) {
		case '.' :
		case '\\':
		case '$' : *r++ = '\\'; *r++ = *glob++;	break;
		case '*' : *r++ = '.';  *r++ = *glob++;	break;
		case '?' : *r++ = '.'; glob++;		break;
		case '/' : free(re);
			   re	= NULL;
			   break;
		default  : *r++ = *glob++;
			   break;
	    }
	if(re) {
	    *r++	= '$';
	    *r		= '\0';
	}
    }
    return re;
}

// checks if filename matches any ignore patterns
int matches_ignore(const char *name)
{
    int match_found = 1;  

    if (option->iPattern->flag) {
        for (int i = 0; i < option->iPattern->npatterns; i++) {
            int regex_result = regexec(option->iPattern->patterns[i], name, 0, NULL, 0);
            if (regex_result == 0) {
                match_found = 0;  
                break;           
            }
        }
    }
    return match_found;
}

// checks if filename matches any only patterns
int matches_only(const char *name)
{
    int match_found = 1;  

    if (option->oPattern->flag) {
        for (int i = 0; i < option->oPattern->npatterns; i++) {
            int regex_result = regexec(option->oPattern->patterns[i], name, 0, NULL, 0);
            if (regex_result == 0) {
                match_found = 0;  
                break;           
            }
        }
    }
    return match_found;
}