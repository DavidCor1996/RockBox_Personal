#ifndef GWATCH_LOCALE_H
#define GWATCH_LOCALE_H

struct lconv
{
    char *decimal_point;
};

struct lconv *localeconv(void);

#endif
