#include "snacc.h"
#include "asn-incl.h"
#include "xml.h"

#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? a : b)
#endif

void dump_xml_node(esnacc_xml_node_t *node, int spacing)
{
    esnacc_xml_node_t *it;
    esnacc_xml_list_t *list;
    esnacc_xml_attribute_t *it_attr;
    if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
    if (!spacing)
        printf("====================================================\n");
    else
        printf("{\n");
    if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
    printf("Name: %s\n", node->name ? node->name : "<(NONE)>");
    if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
    printf("Content-len: %lu\n", node->content ? strlen(node->content) : 0);
    if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
    printf("Namespace: %s\n", node->namespace_attribute.value ?
           node->namespace_attribute.value : "");

    if (node->attributes.count) {
        if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
        printf("Attributes: {\n");
        list = &node->attributes;
        FOR_EACH_LIST_ELMT(it_attr, list) {
            if (spacing) { for (int i = 0; i < spacing; ++i){ printf("  "); }}
            printf("  Name:  %s\n", it_attr->attribute);
            if (spacing) { for (int i = 0; i < spacing; ++i){ printf("  "); }}
            printf("  Value: %s\n", it_attr->value);
        }
        if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
        printf("}");
    }

    if (spacing) { for (int i = 0; i < spacing; ++i) { printf("  "); } }
    printf("Children:");
    list = &node->children;
    if (node->children.count) {
        FOR_EACH_LIST_ELMT(it, list) {
            dump_xml_node(it, spacing+1);
        }
    }
    if (spacing) {
        for (int i = 0; i < spacing; ++i) { printf("  "); }
        printf("}\n");
    }
}

static const char *
allocate_substring(const char *start, const char *end)
{
    char *result;
    if (!start || !end || (start > end)) {
        raise(SIGSEGV);
    }

    result = Malloc((end - start) + 2);
    memcpy(result, start, (end-start));
    *(result + (end-start) + 1) = 0;
    return result;
}

size_t new_node_from_text(const char *text, size_t textlen,
                          esnacc_xml_node_t *node)
{
    const char *currentSegmentStart = NULL, *currentSegmentEnd = NULL;
    const char *i = text;

    node->name = NULL;
    node->content = NULL;
    node->namespace_attribute.attribute=NULL;
    node->namespace_attribute.value=NULL;
    AsnListInit(&node->children, sizeof(esnacc_xml_node_t));
    AsnListInit(&node->attributes, sizeof(esnacc_xml_attribute_t));

    /* skip whitespace */
    while (isspace(*i)) i++;

    if (*i++ != '<')
        return 0;
    if (*i == '/')
        return 0;

    while (isspace(*i))i++;

    for (; !isspace(*i) && *i != '>' && i < (text + textlen); ++i) {
        if (currentSegmentStart == NULL) {
            currentSegmentStart = i;
        }
    }
    currentSegmentEnd = i;

    node->name = allocate_substring(currentSegmentStart, currentSegmentEnd);
    if (strchr(node->name, ':')) {
        char *old_name = (char *)node->name;
        node->name = Strdup(strchr(node->name, ':') + 1);
        *(strchr(old_name, ':')) = 0;
        node->namespace_attribute.value = Strdup(old_name);
        Free(old_name);
    }

    currentSegmentStart = i;
    while (i < (text+textlen) && *i != '>') i++;
    currentSegmentEnd = i;

    // parse attributes
    do {
        while (isspace(*currentSegmentStart)) currentSegmentStart++;
        if (currentSegmentStart != currentSegmentEnd) {
            esnacc_xml_attribute_t **ppelmt =
                (esnacc_xml_attribute_t **)AsnListAppend(&node->attributes);
            esnacc_xml_attribute_t *elmt;
            *ppelmt = Malloc(sizeof(esnacc_xml_attribute_t));
            elmt = *ppelmt;
            elmt->attribute = currentSegmentStart;
            elmt->value = NULL;
            while (*currentSegmentStart != '='
                   && currentSegmentStart < currentSegmentEnd
                   && !isspace(*currentSegmentStart)) {
                currentSegmentStart++;
            }
            elmt->attribute = allocate_substring(elmt->attribute,
                                                 currentSegmentStart);
            if (*currentSegmentStart == '='
                && currentSegmentStart < currentSegmentEnd) {
                elmt->value = ++currentSegmentStart;
                while (!isspace(*currentSegmentStart)
                       && currentSegmentStart < currentSegmentEnd)
                    currentSegmentStart++;
                elmt->value = allocate_substring(elmt->value,
                                                 currentSegmentStart);
            }
        }
    } while (currentSegmentStart != currentSegmentEnd);

    if (*(i-1) == '/') {
        // this is an early exit;
        return i - text;
    }
    if (*currentSegmentStart != '>') {
        return 0;
    }
    ++i;

    currentSegmentStart = i;
    while (i < (text + textlen) && isspace(*i)) ++i;

    if ((i + 8 <= (text + textlen)
         && (*i == '<' && *(i+1) == '!' && *(i+2) == '['))
        || *i != '<') {
        char stopchar = '<';
        if (*i == '<') {
            // need to parse until end of <![CDATA[]]>
            i += 8;
            currentSegmentStart = i;
            stopchar = ']';
        }
        while (i < (text + textlen) && *i != stopchar) ++i;
        currentSegmentEnd = i;
        node->content = allocate_substring(currentSegmentStart,
                                           currentSegmentEnd);
        if (stopchar == ']') {
            ++i;
            ++i;
        }
    } else {
        size_t len;
        esnacc_xml_node_t child = {};
        while (i < (text + textlen)
               && (len = new_node_from_text(i, textlen - (i - text), &child))) {
            esnacc_xml_node_t **newChild =
                (esnacc_xml_node_t **)AsnListAppend(&node->children);
            esnacc_xml_node_t *nc;
            *newChild = Malloc(sizeof(esnacc_xml_node_t));
            nc = *newChild;
            *nc = child;

            i += len;
            memset(&child, 0, sizeof(child));
        }
    }

    if (i+1 < (text + textlen) && *i == '<' && *(i+1) == '/') {
        i += 2;
        while (i < (text+textlen) && isspace(*i)) ++i;
        currentSegmentStart = i;
        while (i < (text+textlen) && *i != '>') ++i;
        currentSegmentEnd = i;
        if (*i == '>') ++i;

        if (strncmp(currentSegmentStart, node->name,
                    MIN(currentSegmentEnd - currentSegmentStart,
                        strlen(node->name)))) {
            return 0;
        }
    }

    return i - text;
}

esnacc_xml_node_t *root_node_from_string(const char *string)
{
    esnacc_xml_node_t *node = Malloc(sizeof(esnacc_xml_node_t));
    if (!new_node_from_text(string, strlen(string), node)) {
        Free(node);
        node = NULL;
    }
    return node;
}

static ssize_t fd_length(int fd) {
    struct stat s;
    if (fstat(fd, &s) == -1) {
        return -1;
    }
    return s.st_size;
}

esnacc_xml_node_t *root_node_from_fd(int fd)
{
    esnacc_xml_node_t *node = NULL;
    ssize_t len = fd_length(fd);

    if (len > 0) {
        char *str = Malloc(len+1);
        read(fd, str, len);
        str[len] = 0;

        node = root_node_from_string(str);
        Free(str);
    }

    return node;
}

esnacc_xml_node_t *delete_first_node_by_name(const char *name,
                                             esnacc_xml_node_t *parent)
{
    esnacc_xml_node_t *child = NULL;
    for_each_child_in_node_pointer(child, parent) {
        if (!strcmp(name, child->name)) {
            struct AsnListNode *remove = parent->children.curr;
            if (parent->children.curr->prev) {
                parent->children.curr->prev->next =
                    parent->children.curr->next;
            }

            if (parent->children.curr->next) {
                parent->children.curr->next->prev =
                    parent->children.curr->prev;
            }
            parent->children.count -= 1;
            parent->children.curr = parent->children.curr->next;
            Free(remove);
            break;
        }
    }
    return child;
}

esnacc_xml_list_t *locate_nodes_by_name(const char *name,
                                        esnacc_xml_node_t *parent)
{
    AsnList *newList = AsnListNew(sizeof(esnacc_xml_node_t *));
    esnacc_xml_node_t *child = NULL;    
    for_each_child_in_node_pointer(child, parent) {
        if (!strcmp(name, child->name)) {
            *(esnacc_xml_node_t **)AsnListAdd(&parent->children) = child;
        }
    }
    return newList;
}
