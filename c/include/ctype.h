/* ctype.h - character classes, for ASCII. These are functions, not macros,
 * so an argument like *p++ is taken once. Codes 80h and up are in no class. */
#ifndef CTYPE_H
#define CTYPE_H

int isdigit(int c);
int isupper(int c);
int islower(int c);
int isalpha(int c);
int isalnum(int c);
int isxdigit(int c);
int isspace(int c);
int isprint(int c);
int ispunct(int c);
int iscntrl(int c);
int toupper(int c);
int tolower(int c);

#endif
