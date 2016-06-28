/*
 * xml.h
 * Routines for encoding / decoding XML data elements.
 *
 * Copyright (C) 2016 Aaron Conole
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef esnacc_asn_xml_h_
#define esnacc_asn_xml_h_

#include "asn-list.h"

typedef AsnList esnacc_xml_list_t;

typedef struct {
    const char *attribute;
    const char *value;
} esnacc_xml_attribute_t;

typedef struct {
    const char *name;
    const char *content;
    esnacc_xml_attribute_t namespace_attribute;
    esnacc_xml_list_t children;
    esnacc_xml_list_t attributes;
} esnacc_xml_node_t;

esnacc_xml_node_t *root_node_from_string(const char *string);
esnacc_xml_node_t *root_node_from_fd(int fd);

esnacc_xml_node_t *delete_first_node_by_name(const char *name,
                                             esnacc_xml_node_t *parent);
esnacc_xml_list_t *locate_nodes_by_name(const char *name,
                                        esnacc_xml_node_t *parent);

size_t new_node_from_text(const char *text, size_t textlen,
                          esnacc_xml_node_t *node);

void dump_xml_node(esnacc_xml_node_t *node, int spacing);

#define for_each_child_in_node(childp, xmlnode)     \
    FOR_EACH_LIST_ELMT(childp, &(xmlnode.children))

#define for_each_child_in_node_pointer(childp, xmlnodep)    \
    FOR_EACH_LIST_ELMT(childp, &(xmlnodep->children))

#define for_each_attribute_in_node(attrp, xmlnode)  \
    FOR_EACH_LIST_ELMT(attrp, xmlnode.attributes)

#define for_each_attribute_in_node_pointer(attrp, xmlnodep)  \
    FOR_EACH_LIST_ELMT(attrp, xmlnodep->attributes)

#endif
