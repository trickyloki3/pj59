#ifndef table_h
#define table_h

#include "parser.h"

struct item_node {
    long id;
    char * name;
    char * bonus;
    char * equip;
    char * unequip;
};

struct item {
    struct store store;
    struct map id;
    struct map name;
    struct item_node * item;
    size_t index;
};

int item_create(struct item *, size_t, struct heap *);
void item_destroy(struct item *);
int item_parse(enum parser_event, int, struct string *, void *);
int item_script_parse(struct item *, char *);

struct constant_node {
    char * identifier;
    long value;
    char * tag;
    struct range_node * range;
};

struct constant {
    struct store store;
    struct map identifier;
    struct constant_node * constant;
    struct range_node * range;
};

int constant_create(struct constant *, size_t, struct heap *);
void constant_destroy(struct constant *);
int constant_parse(enum parser_event, int, struct string *, void *);

struct data_node {
    char * string;
    struct data_node * next;
};

struct argument_node {
    char * identifier;
    char * argument;
    struct data_node * data;
    struct range_node * range;
    long newline;
};

struct argument {
    struct store store;
    struct map identifier;
    struct argument_node * argument;
    struct range_node * range;
};

int argument_create(struct argument *, size_t, struct heap *);
void argument_destroy(struct argument *);
int argument_parse(enum parser_event, int, struct string *, void *);

struct table {
    struct schema schema;
    struct parser parser;
    struct item item;
    struct constant constant;
    struct argument argument;
};

int table_create(struct table *, size_t, struct heap *);
void table_destroy(struct table *);
int table_item_parse(struct table *, char *);
int table_constant_parse(struct table *, char *);
int table_argument_parse(struct table *, char *);

struct item_node * item_start(struct table *);
struct item_node * item_next(struct table *);
struct item_node * item_id(struct table *, long);

struct constant_node * constant_identifier(struct table *, char *);
struct argument_node * argument_identifier(struct table *, char *);

#endif
