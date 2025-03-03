//
// Created by Aaron Gill-Braun on 2020-10-12.
//

#ifndef LIB_RB_TREE_H
#define LIB_RB_TREE_H

#include <base.h>

typedef struct rb_tree rb_tree_t;
typedef struct rb_node rb_node_t;
typedef bool (*rb_pred_t)(rb_tree_t *, rb_node_t *, void *);

typedef enum {
  RED,
  BLACK
} rb_color_t;

typedef enum {
  FORWARD,
  REVERSE
} rb_iter_type_t;

typedef struct rb_node {
  uint64_t key;
  void *data;
  rb_color_t color;
  struct rb_node *left;
  struct rb_node *right;
  struct rb_node *parent;
} rb_node_t;

// event callbacks
typedef void (*rb_evt_pre_rotate_t)(rb_tree_t *tree, rb_node_t *x, rb_node_t *y);
typedef void (*rb_evt_post_rotate_t)(rb_tree_t *tree, rb_node_t *x, rb_node_t *y);
typedef void (*rb_evt_rotate_right_t)(rb_tree_t *tree, rb_node_t *x, rb_node_t *y);
typedef void (*rb_evt_pre_insert_node_t)(rb_tree_t *tree, rb_node_t *z);
typedef void (*rb_evt_post_insert_node_t)(rb_tree_t *tree, rb_node_t *z);
typedef void (*rb_evt_pre_delete_node_t)(rb_tree_t *tree, rb_node_t *z);
typedef void (*rb_evt_post_delete_node_t)(rb_tree_t *tree, rb_node_t *z, rb_node_t *x);
typedef void (*rb_evt_replace_node_t)(rb_tree_t *tree, rb_node_t *u, rb_node_t *v);
typedef void (*rb_evt_dup_node_t)(rb_tree_t *otree, rb_tree_t *ntree, rb_node_t *u, rb_node_t *v);

typedef struct {
  rb_evt_pre_rotate_t pre_rotate;
  rb_evt_post_rotate_t post_rotate;
  rb_evt_pre_insert_node_t pre_insert_node;
  rb_evt_post_insert_node_t post_insert_node;
  rb_evt_pre_delete_node_t pre_delete_node;
  rb_evt_post_delete_node_t post_delete_node;
  rb_evt_replace_node_t replace_node;
  rb_evt_dup_node_t duplicate_node;
} rb_tree_events_t;

typedef struct rb_tree {
  rb_node_t *root;
  rb_node_t *nil;
  rb_node_t *min;
  rb_node_t *max;
  size_t nodes;

  // optional events
  rb_tree_events_t *events;
} rb_tree_t;

typedef struct {
  rb_iter_type_t type;
  rb_tree_t *tree;
  rb_node_t *next;
  bool has_next;
} rb_iter_t;

rb_tree_t *create_rb_tree();
void rb_tree_free(rb_tree_t *tree);
rb_tree_t *copy_rb_tree(rb_tree_t *tree);
rb_tree_t *copy_rb_tree_pred(rb_tree_t *tree, rb_pred_t pred, void *arg);
void *rb_tree_get(rb_tree_t *tree, uint64_t key);
rb_node_t *rb_tree_find(rb_tree_t *tree, uint64_t key);
rb_node_t *rb_tree_find_closest(rb_tree_t *tree, uint64_t key);
void rb_tree_insert(rb_tree_t *tree, uint64_t key, void *data);
void rb_tree_insert_node(rb_tree_t *tree, rb_node_t *node);
void *rb_tree_delete(rb_tree_t *tree, uint64_t key);
void *rb_tree_delete_node(rb_tree_t *tree, rb_node_t *node);

void rb_tree_init_iter(rb_tree_t *tree, rb_node_t *next, rb_iter_type_t type, rb_iter_t *iter);
rb_iter_t *rb_tree_make_iter(rb_tree_t *tree, rb_node_t *next, rb_iter_type_t type);
rb_iter_t *rb_tree_iter(rb_tree_t *tree);
rb_iter_t *rb_tree_iter_reverse(rb_tree_t *tree);
rb_node_t *rb_iter_next(rb_iter_t *iter);

rb_node_t *rb_tree_get_next(rb_tree_t *tree, rb_node_t *node);
rb_node_t *rb_tree_get_last(rb_tree_t *tree, rb_node_t *node);

#endif
