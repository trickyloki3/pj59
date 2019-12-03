#include "array.h"

int strbuf_create(struct strbuf * strbuf, size_t size) {
    int status = 0;

    if(!size) {
        status = panic("size is zero");
    } else {
        strbuf->buf = calloc(size, sizeof(*strbuf->buf));
        if(!strbuf->buf) {
            status = panic("out of memory");
        } else {
            strbuf->str = strbuf->pos = strbuf->buf;
            strbuf->end = strbuf->buf + size;
        }
    }

    return status;
}

void strbuf_destroy(struct strbuf * strbuf) {
    free(strbuf->buf);
}

void strbuf_clear(struct strbuf * strbuf) {
    memset(strbuf->buf, 0, strbuf->pos - strbuf->buf);
    strbuf->str = strbuf->pos = strbuf->buf;
}

int strbuf_putc(struct strbuf * strbuf, char c) {
    int status = 0;

    if(strbuf->pos == strbuf->end) {
        status = panic("out of memory");
    } else {
        *strbuf->pos++ = c;
    }

    return status;
}


int strbuf_strcpy(struct strbuf * strbuf, char * string, size_t length) {
    int status = 0;

    if(strbuf->end - strbuf->pos < length) {
        status = panic("out of memory");
    } else {
        memcpy(strbuf->pos, string, length);
        strbuf->pos += length;
    }

    return status;
}

struct string * strbuf_string(struct strbuf * strbuf) {
    int status = 0;
    struct string * string = NULL;

    if(strbuf_putc(strbuf, '\0')) {
        status = panic("failed to putc strbuf object");
    } else if(strbuf->end - strbuf->pos < sizeof(*string)) {
        status = panic("out of memory");
    } else {
        string = (void *) strbuf->pos;
        string->string = strbuf->str;
        string->length = strbuf->pos - strbuf->str - 1;
        strbuf->str = strbuf->pos += sizeof(*string);
    }

    return status ? NULL : string;
}
