#ifndef CTYPE_H
#define CTYPE_H

#define isdigit(c)  ((unsigned)(c) - '0' < 10)
#define isupper(c)  ((unsigned)(c) - 'A' < 26)
#define islower(c)  ((unsigned)(c) - 'a' < 26)
#define isalpha(c)  (isupper(c) || islower(c))
#define isalnum(c)  (isalpha(c) || isdigit(c))
#define isxdigit(c) (isdigit(c) || (unsigned)((c) | 0x20) - 'a' < 6)
#define isspace(c)  ((c) == ' ' || (unsigned)(c) - '\t' < 5)
#define isprint(c)  ((unsigned)(c) - ' ' < 95)
#define ispunct(c)  (isprint(c) && !isalnum(c) && (c) != ' ')
#define iscntrl(c)  ((unsigned)(c) < 32 || (c) == 127)
#define toupper(c)  (islower(c) ? (c) - 32 : (c))
#define tolower(c)  (isupper(c) ? (c) + 32 : (c))

#endif
