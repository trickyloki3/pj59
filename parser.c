#include "parser.h"

struct schema_node * schema_node_create(struct schema *, enum schema_type, int);
void schema_node_destroy(struct schema *, struct schema_node *);
void schema_node_print(struct schema_node *, int, char *);

int schema_reload_loop(struct schema *, struct schema_markup *, struct schema_node *);
int schema_update_loop(struct schema *, struct schema_markup *, struct schema_node *);

struct parser_state {
    struct schema_node * root;
    struct schema_node * data;
    /* parser_state_schema */
    struct strbuf * strbuf;
    char * key;
    struct schema * schema;
    /* parser_state_data */
    parser_cb callback;
    void * context;
};

void parser_state_push(struct parser_state *, struct schema_node *, enum schema_type);
void parser_state_pop(struct parser_state *);
int parser_state_schema(enum event_type, struct string *, void *);
int parser_state_data_next(struct parser_state *, struct schema_node *, enum event_type, struct string *);
int parser_state_data(enum event_type, struct string *, void *);

int parser_schema_path(struct parser *, struct parser_state *, const char *);
int parser_data_path(struct parser *, struct parser_state *, const char *);

struct schema_node * schema_node_create(struct schema * schema, enum schema_type type, int mark) {
    int status = 0;
    struct schema_node * node;

    node = store_calloc(&schema->store, sizeof(*node));
    if(!node) {
        status = panic("failed to calloc store object");
    } else {
        node->type = type;
        node->mark = mark;
        node->map = store_malloc(&schema->store, sizeof(*node->map));
        if(!node->map) {
            status = panic("failed to malloc store object");
        } else if(map_create(node->map, (map_compare_cb) strcmp, &schema->pool)) {
            status = panic("failed to create map object");
        } else {
            node->list = NULL;
        }
    }

    return status ? NULL : node;
}

void schema_node_destroy(struct schema * schema, struct schema_node * node) {
    struct map_kv kv;

    if(node->list)
        schema_node_destroy(schema, node->list);

    kv = map_start(node->map);
    while(kv.key) {
        schema_node_destroy(schema, kv.value);
        kv = map_next(node->map);
    }

    map_destroy(node->map);
}

void schema_node_print(struct schema_node * node, int indent, char * key) {
    int i;
    struct map_kv kv;

    for(i = 0; i < indent; i++)
        fputs("  ", stdout);

    if(key)
        fprintf(stdout, "[%s]", key);

    switch(node->type) {
        case list | map | string:
            fprintf(stdout, "[list | map | string]");
            break;
        case list | map:
            fprintf(stdout, "[list | map]");
            break;
        case list | string:
            fprintf(stdout, "[list | string]");
            break;
        case map | string:
            fprintf(stdout, "[map | string]");
            break;
        case list:
            fprintf(stdout, "[list]");
            break;
        case map:
            fprintf(stdout, "[map]");
            break;
        case string:
            fprintf(stdout, "[string]");
            break;
    }

    fprintf(stdout, "[%d]\n", node->mark);

    if(node->type & map) {
        kv = map_start(node->map);
        while(kv.key) {
            schema_node_print(kv.value, indent + 1, kv.key);
            kv = map_next(node->map);
        }
    }

    if(node->type & list)
        schema_node_print(node->list, indent + 1, NULL);
}

int schema_create(struct schema * schema, size_t size) {
    int status = 0;

    if(pool_create(&schema->pool, sizeof(struct map_node), size / sizeof(struct map_node))) {
        status = panic("failed to create pool object");
    } else {
        if(store_create(&schema->store, size)) {
            status = panic("failed to create store object");
        } else {
            schema->root = NULL;

            if(status)
                store_destroy(&schema->store);
        }
        if(status)
            pool_destroy(&schema->pool);
    }

    return status;
}

void schema_destroy(struct schema * schema) {
    schema_clear(schema);
    store_destroy(&schema->store);
    pool_destroy(&schema->pool);
}

void schema_clear(struct schema * schema) {
    struct schema_node * node;

    if(schema->root) {
        node = schema->root;
        schema->root = NULL;
        schema_node_destroy(schema, node);
    }

    store_clear(&schema->store);
}

void schema_print(struct schema * schema) {
    if(schema->root)
        schema_node_print(schema->root, 0, NULL);
}

int schema_reload_loop(struct schema * schema, struct schema_markup * array, struct schema_node * root) {
    int status = 0;

    char * key;
    struct schema_node * node;

    root->state = 0;

    while(array->level > 0 && !status) {
        while(root->state >= array->level)
            root = root->next;

        node = schema_node_create(schema, array->type, array->mark);
        if(!node) {
            status = panic("failed to create schema node object");
        } else {
            if(array->key) {
                if(root->type & map) {
                    key = store_printf(&schema->store, "%s", array->key);
                    if(!key) {
                        status = panic("failed to strcpy store object");
                    } else if(map_insert(root->map, key, node)) {
                        status = panic("failed to insert map object");
                    }
                } else {
                    status = panic("expected map");
                }
            } else {
                if(root->type & list) {
                    if(root->list) {
                        status = panic("invalid list");
                    } else {
                        root->list = node;
                    }
                } else {
                    status = panic("expected list");
                }
            }

            if(status) {
                schema_node_destroy(schema, node);
            } else if(node->type & (list | map)) {
                node->state = array->level;
                node->next = root;
                root = node;
            }
        }

        array++;
    }

    return status;
}

int schema_reload(struct schema * schema, struct schema_markup * array) {
    int status = 0;

    schema_clear(schema);

    schema->root = schema_node_create(schema, list, 0);
    if(!schema->root) {
        status = panic("failed to create schema node object");
    } else if(schema_reload_loop(schema, array, schema->root)) {
        status = panic("failed to reload loop schema object");
    }

    return status;
}

int schema_update_loop(struct schema * schema, struct schema_markup * array, struct schema_node * root) {
    int status = 0;

    enum schema_type type;
    struct schema_node * node;

    root->state = 0;

    while(array->level > 0 && !status) {
        while(root->state >= array->level)
            root = root->next;

        if(array->key) {
            if(root->type & map) {
                node = map_search(root->map, array->key);
                if(!node) {
                    status = panic("invalid key - %s", array->key);
                } else {
                    type = node->type & array->type;
                    if(type) {
                        node->mark = array->mark;

                        if(type & (list | map)) {
                            node->state = array->level;
                            node->next = root;
                            root = node;
                        }
                    } else {
                        status = panic("expected %d", array->type);
                    }
                }
            } else {
                status = panic("expected map");
            }
        } else {
            if(root->type & list) {
                node = root->list;
                if(!node) {
                    status = panic("invalid node");
                } else {
                    type = node->type & array->type;
                    if(type) {
                        node->mark = array->mark;

                        if(type & (list | map)) {
                            node->state = array->level;
                            node->next = root;
                            root = node;
                        }
                    } else {
                        status = panic("expected %d", array->type);
                    }
                }
            } else {
                status = panic("expected list");
            }
        }

        array++;
    }

    return status;
}

int schema_update(struct schema * schema, struct schema_markup * array) {
    int status = 0;

    if(schema_update_loop(schema, array, schema->root))
        status = panic("failed to mark loop schema object");

    return status;
}

void parser_state_push(struct parser_state * state, struct schema_node * node, enum schema_type type) {
    node->state = type;
    node->next = state->root;
    state->root = node;
}

void parser_state_pop(struct parser_state * state) {
    state->root = state->root->next;
}

int parser_state_schema(enum event_type type, struct string * value, void * context) {
    int status = 0;

    struct parser_state * state;
    struct schema_node * node;
    struct schema_node * data;
    char * key;

    state = context;

    node = state->root;
    if(!node) {
        status = panic("invalid node");
    } else {
        if(state->key) {
            if(type == event_list_start) {
                data = map_search(node->map, state->key);
                if(data) {
                    data->type |= list;
                    parser_state_push(state, data, list);
                } else {
                    data = schema_node_create(state->schema, list, 0);
                    if(!data) {
                        status = panic("failed to create schema node object");
                    } else {
                        key = store_printf(&state->schema->store, "%s", state->key);
                        if(!key) {
                            status = panic("failed to strcpy store object");
                        } else if(map_insert(node->map, key, data)) {
                            status = panic("failed to insert map object");
                        } else {
                            parser_state_push(state, data, list);
                        }
                        if(status)
                            schema_node_destroy(state->schema, data);
                    }
                }
            } else if(type == event_map_start) {
                data = map_search(node->map, state->key);
                if(data) {
                    data->type |= map;
                    parser_state_push(state, data, map);
                } else {
                    data = schema_node_create(state->schema, map, 0);
                    if(!data) {
                        status = panic("failed to create schema node object");
                    } else {
                        key = store_printf(&state->schema->store, "%s", state->key);
                        if(!key) {
                            status = panic("failed to strcpy store object");
                        } else if(map_insert(node->map, key, data)) {
                            status = panic("failed to insert map object");
                        } else {
                            parser_state_push(state, data, map);
                        }
                        if(status)
                            schema_node_destroy(state->schema, data);
                    }
                }
            } else if(type == event_scalar) {
                data = map_search(node->map, state->key);
                if(data) {
                    data->type |= string;
                } else {
                    data = schema_node_create(state->schema, string, 0);
                    if(!data) {
                        status = panic("failed to create schema node object");
                    } else {
                        key = store_printf(&state->schema->store, "%s", state->key);
                        if(!key) {
                            status = panic("failed to strcpy store object");
                        } else if(map_insert(node->map, key, data)) {
                            status = panic("failed to insert map object");
                        }
                        if(status)
                            schema_node_destroy(state->schema, data);
                    }
                }
            } else {
                status = panic("invalid type - %d", type);
            }
            state->key = NULL;
            strbuf_clear(state->strbuf);
        } else if(node->state == list) {
            if(type == event_list_end) {
                parser_state_pop(state);
            } else if(type == event_list_start) {
                if(node->list) {
                    node->list->type |= list;
                    parser_state_push(state, node->list, list);
                } else {
                    node->list = schema_node_create(state->schema, list, 0);
                    if(!node->list) {
                        status = panic("failed to create schema node object");
                    } else {
                        parser_state_push(state, node->list, list);
                    }
                }
            } else if(type == event_map_start) {
                if(node->list) {
                    node->list->type |= map;
                    parser_state_push(state, node->list, map);
                } else {
                    node->list = schema_node_create(state->schema, map, 0);
                    if(!node->list) {
                        status = panic("failed to create schema node object");
                    } else {
                        parser_state_push(state, node->list, map);
                    }
                }
            } else if(type == event_scalar) {
                if(node->list) {
                    node->list->type |= string;
                } else {
                    node->list = schema_node_create(state->schema, string, 0);
                    if(!node->list)
                        status = panic("failed to create schema node object");
                }
            } else {
                status = panic("invalid type - %d", type);
            }
        } else if(node->state == map) {
            if(type == event_map_end) {
                parser_state_pop(state);
            } else if(type == event_scalar) {
                if(strbuf_strcpy(state->strbuf, value->string, value->length)) {
                    status = panic("failed to strcpy strbuf object");
                } else {
                    state->key = strbuf_char(state->strbuf);
                    if(!state->key)
                        status = panic("failed to string strbuf object");
                }
            } else {
                status = panic("invalid type - %d", type);
            }
        } else {
            status = panic("invalid node state -- %d", node->state);
        }
    }

    return status;
}

int parser_state_data_next(struct parser_state * state, struct schema_node * node, enum event_type type, struct string * value) {
    int status = 0;

    if(type == event_list_start) {
        if(node->type & list) {
            if(state->callback(start, node->mark, NULL, state->context)) {
                status = panic("failed to process start event");
            } else {
                parser_state_push(state, node, list);
            }
        } else {
            status = panic("unexpected list");
        }
    } else if(type == event_map_start) {
        if(node->type & map) {
            if(state->callback(start, node->mark, NULL, state->context)) {
                status = panic("failed to process start event");
            } else {
                parser_state_push(state, node, map);
            }
        } else {
            status = panic("unexpected map");
        }
    } else if(type == event_scalar) {
        if(node->type & string) {
            if(state->callback(next, node->mark, value, state->context))
                status = panic("failed to process next event");
        } else {
            status = panic("unexpected string");
        }
    } else {
        status = panic("invalid type - %d", type);
    }

    return status;
}

/*
 * parser_state_data grammar
 *
 *  node : string
 *       | start list end
 *       | start map end
 *
 *  list : node
 *       | list node
 *
 *  map  : string node
 *       | map string node
 */

int parser_state_data(enum event_type type, struct string * value, void * context) {
    int status = 0;
    struct parser_state * state;
    struct schema_node * node;

    state = context;

    if(state->data) {
        if(parser_state_data_next(state, state->data, type, value)) {
            status = panic("failed to start parser state object");
        } else {
            state->data = NULL;
        }
    } else {
        node = state->root;
        if(!node) {
            status = panic("invalid node");
        } else {
            if(node->state == list) {
                if(type == event_list_end) {
                    if(state->callback(end, node->mark, NULL, state->context)) {
                        status = panic("failed to process end event");
                    } else {
                        parser_state_pop(state);
                    }
                } else if(parser_state_data_next(state, node->list, type, value)) {
                    status = panic("failed to start parser state object");
                }
            } else if(node->state == map) {
                if(type == event_map_end) {
                    if(state->callback(end, node->mark, NULL, state->context)) {
                        status = panic("failed to process end event");
                    } else {
                        parser_state_pop(state);
                    }
                } else if(type == event_scalar) {
                    state->data = map_search(node->map, value->string);
                    if(!state->data)
                        status = panic("invalid key - %s", value->string);
                } else {
                    status = panic("invalid type - %d", type);
                }
            } else {
                status = panic("invalid node state - %d", node->state);
            }
        }
    }

    return status;
}

int parser_create(struct parser * parser, size_t size, struct heap * heap) {
    int status = 0;

    parser->size = size;

    if(csv_create(&parser->csv, parser->size, heap)) {
        status = panic("failed to create csv object");
    } else {
        if(json_create(&parser->json, parser->size)) {
            status = panic("failed to create json object");
        } else {
            if(yaml_create(&parser->yaml, parser->size, heap)) {
                status = panic("failed to create yaml object");
            } else {
                if(strbuf_create(&parser->strbuf, size)) {
                    status = panic("failed to create strbuf object");
                } else {
                    if(status)
                        strbuf_destroy(&parser->strbuf);
                }
                if(status)
                    yaml_destroy(&parser->yaml);
            }
            if(status)
                json_destroy(&parser->json);
        }
        if(status)
            csv_destroy(&parser->csv);
    }

    return status;
}

void parser_destroy(struct parser * parser) {
    strbuf_destroy(&parser->strbuf);
    yaml_destroy(&parser->yaml);
    json_destroy(&parser->json);
    csv_destroy(&parser->csv);
}

int parser_schema(struct parser * parser, struct schema * schema, const char * path) {
    int status = 0;

    struct parser_state state;

    state.root = NULL;
    state.data = NULL;
    state.strbuf = &parser->strbuf;
    state.key = NULL;
    state.schema = schema;

    schema_clear(schema);

    schema->root = schema_node_create(schema, list, 0);
    if(!schema->root) {
        status = panic("failed to create schema node object");
    } else {
        parser_state_push(&state, schema->root, list);

        if(parser_schema_path(parser, &state, path))
            status = panic("failed to schema path parser object");
    }

    return status;
}

int parser_schema_path(struct parser * parser, struct parser_state * state, const char * path) {
    int status = 0;

    char * ext;

    ext = strrchr(path, '.');
    if(!ext) {
        status = panic("failed to get file extension - %s", path);
    } else {
        if(!strcmp(ext, ".yaml") || !strcmp(ext, ".yml")) {
            if(yaml_parse(&parser->yaml, path, parser->size, parser_state_schema, state))
                status = panic("failed to parse yaml object");
        } else {
            status = panic("unsupported extension - %s", ext);
        }
    }

    return status;
}


int parser_data(struct parser * parser, struct schema * schema, parser_cb callback, void * context, const char * path) {
    int status = 0;

    struct parser_state state;

    state.root = NULL;
    state.data = NULL;
    state.callback = callback;
    state.context = context;

    parser_state_push(&state, schema->root, list);

    if(parser_data_path(parser, &state, path))
        status = panic("failed to parse path parser object");

    return status;
}

int parser_data_path(struct parser * parser, struct parser_state * state, const char * path) {
    int status = 0;

    char * ext;

    ext = strrchr(path, '.');
    if(!ext) {
        status = panic("failed to get file extension - %s", path);
    } else {
        if(!strcmp(ext, ".txt")) {
            if(csv_parse(&parser->csv, path, parser->size, parser_state_data, state))
                status = panic("failed to parse csv object");
        } else if(!strcmp(ext, ".json")) {
            if(json_parse(&parser->json, path, parser->size, parser_state_data, state))
                status = panic("failed to parse json object");
        } else if(!strcmp(ext, ".yaml") || !strcmp(ext, ".yml")) {
            if(yaml_parse(&parser->yaml, path, parser->size, parser_state_data, state))
                status = panic("failed to parse yaml object");
        } else {
            status = panic("unsupported extension - %s", ext);
        }
    }

    return status;
}
