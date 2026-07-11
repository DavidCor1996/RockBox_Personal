#ifndef GWATCH_STDIO_H
#define GWATCH_STDIO_H

#define __need_size_t
#include <stddef.h>

#define __need___va_list
#include <stdarg.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef EOF
#define EOF (-1)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifndef TMP_MAX
#define TMP_MAX 26
#endif

typedef void FILE;

#define stdin ((FILE *)0)
#define stdout ((FILE *)0)
#define stderr ((FILE *)0)

#ifdef __GNUC__
#define GWATCH_VALIST __gnuc_va_list
#else
#define GWATCH_VALIST char *
#endif

int vsnprintf(char *buf, size_t size, const char *fmt, GWATCH_VALIST ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);

FILE *fopen(const char *path, const char *mode);
FILE *tmpfile(void);
int fclose(FILE *stream);
int fflush(FILE *stream);
int ferror(FILE *stream);
int feof(FILE *stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
int ungetc(int c, FILE *stream);
void clearerr(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
char *fgets(char *str, int count, FILE *stream);
int fputs(const char *str, FILE *stream);

#endif
