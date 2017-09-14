#ifndef STRUTILS_H
#define STRUTILS_H

typedef struct
{
   char *buf;
   size_t allocated;
   size_t index;
} strbuf;

void strbuf_alloc(strbuf *sbuf, size_t allocate);

// sprintf and strcat to end of buffer
void strbuf_sprintf(strbuf *sbuf, const char *format, ...);

void strbuf_free(strbuf *sbuf);

#endif /* STRUTILS_H */
