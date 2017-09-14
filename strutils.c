#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strutils.h"

// buffer for local sprintf
static char tmpbuf[4096];

void strbuf_alloc(strbuf *sbuf, size_t allocate)
{
   // some sane default allocation
   if (allocate <= 0) {
      allocate = 512;
   }
   sbuf->buf = malloc(allocate);
   sbuf->allocated = allocate;
   sbuf->index = 0;
}

void strbuf_sprintf(strbuf *sbuf, const char *format, ...)
{
   va_list args;

   va_start(args, format);
   int len = vsprintf(tmpbuf, format, args);
   va_end(args);

   while (sbuf->allocated <= sbuf->index + len) {
      sbuf->allocated *= 2;
      sbuf->buf = realloc(sbuf->buf, sbuf->allocated);
   }
   memcpy(&sbuf->buf[sbuf->index], tmpbuf, len + 1);
   sbuf->index += len;
}

void strbuf_free(strbuf *sbuf)
{
   if (sbuf->buf) {
      free(sbuf->buf);
      sbuf->allocated = 0;
   }
}
