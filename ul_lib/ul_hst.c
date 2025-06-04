#include "ul_hst.h"
#include "ul_arr.h"
#include "ul_assert.h"
#include "ul_misc.h"

static ul_hst_node_t * create_node(size_t str_size, const char * str, ul_hs_hash_t str_hash)
{
    char * node_str = malloc(sizeof(*node_str) * (str_size + 1));

    ul_assert(node_str != NULL);

    memcpy(node_str, str, sizeof(*str) * str_size);
    node_str[str_size] = 0;

    ul_hst_node_t * node = malloc(sizeof(*node));

    ul_assert(node != NULL);

    *node = (ul_hst_node_t){
        .size = str_size,
        .str = node_str,
        .hash = str_hash,
    };

    return node;
}
static void destroy_node(ul_hst_node_t * node)
{
    if (node == NULL)
    {
        return;
    }

    free(node->str);

    free(node);
}

void ul_hst_init(ul_hst_t * hst)
{
    *hst = (ul_hst_t){ 0 };
}
void ul_hst_cleanup(ul_hst_t * hst)
{
    for (ul_hst_node_t **ent = hst->ents, **ent_end = ent + hst->ents_size; ent != ent_end; ++ent)
    {
        for (ul_hst_node_t * node = *ent; node != NULL;)
        {
            ul_hst_node_t * next = node->next;

            destroy_node(node);

            node = next;
        }
    }

    free(hst->ents);

    memset(hst, 0, sizeof(*hst));
}

static void rehash(ul_hst_t * hst)
{
    size_t new_ents_size = hst->ents_size * 2;
    new_ents_size = ul_max(new_ents_size, 1);

    ul_hst_node_t ** new_ents = malloc(sizeof(*new_ents) * new_ents_size);

    ul_assert(new_ents != NULL);

    memset(new_ents, 0, sizeof(*new_ents) * new_ents_size);

    for (ul_hst_node_t **ent = hst->ents, **ent_end = ent + hst->ents_size; ent != ent_end; ++ent)
    {
        for (ul_hst_node_t * node = *ent; node != NULL;)
        {
            ul_hst_node_t * next = node->next;

            ul_hst_node_t ** new_ent = &new_ents[node->hash % new_ents_size];

            node->next = *new_ent;
            *new_ent = node;

            node = next;
        }
    }

    free(hst->ents);

    hst->ents = new_ents;
    hst->ents_size = new_ents_size;
}
static ul_hst_node_t ** get_node_ins(ul_hst_t * hst, size_t str_size, const char * str, ul_hs_hash_t str_hash)
{
    if (hst->ents_size == 0 || hst->ents_size < hst->strs_count)
    {
        rehash(hst);
    }

    ul_hst_node_t ** node_ins = &hst->ents[str_hash % hst->ents_size];

    while (*node_ins != NULL)
    {
        ul_hst_node_t * node = *node_ins;

        if (node->hash == str_hash
            && node->size == str_size
            && strncmp(node->str, str, str_size) == 0)
        {
            break;
        }

        node_ins = &node->next;
    }

    return node_ins;
}

ul_hst_node_ref_t ul_hst_add(ul_hst_t * hst, size_t str_size, const char * str, ul_hs_hash_t str_hash)
{
    ul_hst_node_t ** node_ins = get_node_ins(hst, str_size, str, str_hash);

    if (*node_ins == NULL)
    {
        *node_ins = create_node(str_size, str, str_hash);

        ++hst->strs_count;
    }

    return (ul_hst_node_ref_t){ .node = *node_ins };
}

void ul_hst_copy_ref(ul_hst_node_ref_t * dst, const ul_hst_node_ref_t * src)
{
    *dst = *src;
}
void ul_hst_free_ref(ul_hst_node_ref_t * ref)
{
    ref->node = NULL;
}

extern inline ul_hst_node_ref_t ul_hst_hashadd(ul_hst_t * hst, size_t str_size, const char * str);
