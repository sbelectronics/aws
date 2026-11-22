/* ctype.c - character classes */
#include <ctype.h>

int isdigit(int c)
{
    return (unsigned)c - '0' < 10;
}

int isupper(int c)
{
    return (unsigned)c - 'A' < 26;
}

int islower(int c)
{
    return (unsigned)c - 'a' < 26;
}

int isalpha(int c)
{
    return isupper(c) || islower(c);
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isxdigit(int c)
{
    return isdigit(c) || (unsigned)(c | 0x20) - 'a' < 6;
}

int isspace(int c)
{
    return c == ' ' || (unsigned)c - '\t' < 5;
}

int isprint(int c)
{
    return (unsigned)c - ' ' < 95;
}

int ispunct(int c)
{
    return isprint(c) && !isalnum(c) && c != ' ';
}

int iscntrl(int c)
{
    return (unsigned)c < 32 || c == 127;
}

int toupper(int c)
{
    return islower(c) ? c - 32 : c;
}

int tolower(int c)
{
    return isupper(c) ? c + 32 : c;
}
