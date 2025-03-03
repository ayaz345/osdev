//
// Created by Aaron Gill-Braun on 2023-05-25.
//

#include <vfs/vfs.h>
#include <vfs/vnode.h>
#include <vfs/ventry.h>

#include <mm.h>
#include <panic.h>
#include <printf.h>

#include "ramfs.h"
#include "ramfs_file.h"

#define ASSERT(x) kassert(x)
#define DPRINTF(fmt, ...) kprintf("ramfs_vnops: %s: " fmt, __func__, ##__VA_ARGS__)
#define TRACE(str, ...) kprintf("ramfs_vnops: " str, ##__VA_ARGS__)

struct vnode_ops ramfs_vnode_ops = {
  .v_read = ramfs_vn_read,
  .v_write = ramfs_vn_write,
  .v_map = ramfs_vn_map,

  .v_load = ramfs_vn_load,
  .v_save = ramfs_vn_save,
  .v_readlink = ramfs_vn_readlink,
  .v_readdir = ramfs_vn_readdir,

  .v_lookup = ramfs_vn_lookup,
  .v_create = ramfs_vn_create,
  .v_mknod = ramfs_vn_mknod,
  .v_symlink = ramfs_vn_symlink,
  .v_hardlink = ramfs_vn_hardlink,
  .v_unlink = ramfs_vn_unlink,
  .v_mkdir = ramfs_vn_mkdir,
  .v_rmdir = ramfs_vn_rmdir,

  .v_cleanup = ramfs_vn_cleanup,
};

struct ventry_ops ramfs_ventry_ops = {
  .v_cleanup = ramfs_ve_cleanup,
};


static inline unsigned char vtype_to_dtype(enum vtype type) {
  switch (type) {
    case V_REG:
      return DT_REG;
    case V_DIR:
      return DT_DIR;
    case V_LNK:
      return DT_LNK;
    case V_CHR:
      return DT_CHR;
    case V_BLK:
      return DT_BLK;
    case V_FIFO:
      return DT_FIFO;
    case V_SOCK:
      return DT_SOCK;
    default:
      return DT_UNKNOWN;
  }
}


ssize_t ramfs_vn_read(vnode_t *vn, off_t off, struct kio *kio) {
  ramfs_node_t *node = vn->data;
  ramfs_file_t *file = node->n_file;
  return ramfs_file_read(file, off, kio);
}

ssize_t ramfs_vn_write(vnode_t *vn, off_t off, struct kio *kio) {
  ramfs_node_t *node = vn->data;
  ramfs_file_t *file = node->n_file;
  return ramfs_file_write(file, off, kio);
}

int ramfs_vn_map(vnode_t *vn, off_t off, struct vm_mapping *mapping) {
  ramfs_node_t *node = vn->data;
  ramfs_file_t *file = node->n_file;
  return ramfs_file_map(file, mapping);
}

//

int ramfs_vn_load(vnode_t *vn) {
  return 0;
}

int ramfs_vn_save(vnode_t *vn) {
  return 0;
}

int ramfs_vn_readlink(vnode_t *vn, struct kio *kio) {
  TRACE("readlink id=%u\n", vn->id);
  ramfs_node_t *node = vn->data;

  kio_t tmp = kio_readonly_from_str(node->n_link);
  size_t res = kio_copy(kio, &tmp);
  if (res != str_len(node->n_link)) {
    return -EIO;
  }
  return 0;
}

ssize_t ramfs_vn_readdir(vnode_t *vn, off_t off, kio_t *dirbuf) {
  TRACE("readdir id=%u\n", vn->id);
  ramfs_node_t *node = vn->data;

  size_t i = 0;
  ramfs_dirent_t *dent = LIST_FIRST(&node->n_dir);
  while (dent) {
    // get to the right offset
    if (i < off) {
      dent = LIST_NEXT(dent, list);
      i++;
      continue;
    }

    cstr_t name = cstr_from_str(dent->name);
    struct dirent dirent;
    dirent.d_ino = dent->node->id;
    dirent.d_type = vtype_to_dtype(dent->node->type);
    dirent.d_reclen = sizeof(struct dirent) + str_len(dent->name) + 1;
    dirent.d_namlen = str_len(dent->name);

    if (kio_remaining(dirbuf) < dirent.d_reclen)
      break;

    // write the entry
    kio_write(dirbuf, &dirent, sizeof(struct dirent), 0); // write dirent
    kio_write(dirbuf, cstr_ptr(name), dirent.d_namlen+1, 0); // write name

    dent = LIST_NEXT(dent, list);
    i++;
  }

  return (ssize_t) i;
}

//

int ramfs_vn_lookup(vnode_t *dir, cstr_t name, __move ventry_t **result) {
  return -ENOENT;
}

int ramfs_vn_create(vnode_t *dir, cstr_t name, struct vattr *vattr, __move ventry_t **result) {
  TRACE("create id=%u \"{:cstr}\"\n", dir->id, &name);
  vfs_t *vfs = dir->vfs;
  ramfs_node_t *dnode = dir->data;
  ramfs_mount_t *mount = dnode->mount;

  // create the file node and entry
  ramfs_node_t *node = ramfs_node_alloc(mount, vattr->type, vattr->mode);
  ramfs_dirent_t *dent = ramfs_dirent_alloc(node, name);
  node->n_file = ramfs_file_alloc(0);
  ramfs_dir_add(dnode, dent);

  // create the vnode and ventry
  vnode_t *vn = vn_alloc(node->id, vattr);
  vn->data = node;
  ventry_t *ve = ve_alloc_linked(name, vn);
  ve->data = dent;

  *result = ve_moveref(&ve);
  vn_release(&vn);
  return 0;
}

int ramfs_vn_mknod(vnode_t *dir, cstr_t name, struct vattr *vattr, dev_t dev, __move ventry_t **result) {
  TRACE("mknod id=%u \"{:cstr}\"\n", dir->id, &name);
  vfs_t *vfs = dir->vfs;
  ramfs_node_t *dnode = dir->data;
  ramfs_mount_t *mount = dnode->mount;

  mode_t mode = vattr->mode;
  if (!(mode & S_IFCHR) && !(mode & S_IFBLK)) {
    DPRINTF("only character and block devices are supported\n");
    return -EINVAL;
  }

  // create the device node and entry
  ramfs_node_t *node = ramfs_node_alloc(mount, vattr->type, mode);
  ramfs_dirent_t *dent = ramfs_dirent_alloc(node, name);
  node->n_dev = dev;
  ramfs_dir_add(dnode, dent);

  // create the vnode and ventry
  vnode_t *vn = vn_alloc(node->id, vattr);
  vn->data = node;
  ventry_t *ve = ve_alloc_linked(name, vn);
  ve->data = dent;

  *result = ve_moveref(&ve);
  vn_release(&vn);
  return 0;
}

int ramfs_vn_symlink(vnode_t *dir, cstr_t name, struct vattr *vattr, cstr_t target, __move ventry_t **result) {
  TRACE("symlink id=%u \"{:cstr}\" -> \"{:cstr}\"\n", dir->id, &name, &target);
  vfs_t *vfs = dir->vfs;
  ramfs_node_t *dnode = dir->data;
  ramfs_mount_t *mount = dnode->mount;

  // create the symlink node and entry
  ramfs_node_t *node = ramfs_node_alloc(mount, vattr->type, vattr->mode);
  ramfs_dirent_t *dent = ramfs_dirent_alloc(node, name);
  node->n_link = str_copy_cstr(target);
  ramfs_dir_add(dnode, dent);

  // create the vnode and ventry
  vnode_t *vn = vn_alloc(node->id, vattr);
  vn->data = node;
  ventry_t *ve = ve_alloc_linked(name, vn);
  ve->data = dent;

  *result = ve_moveref(&ve);
  vn_release(&vn);
  return 0;
}

int ramfs_vn_hardlink(vnode_t *dir, cstr_t name, vnode_t *target, __move ventry_t **result) {
  TRACE("hardlink id=%u \"{:cstr}\" -> id=%u\n", dir->id, &name, target->id);
  ramfs_node_t *dnode = dir->data;
  ramfs_node_t *tnode = target->data;
  ramfs_mount_t *mount = dnode->mount;

  // create the new entry
  ramfs_dirent_t *dent = ramfs_dirent_alloc(tnode, name);
  ramfs_dir_add(dnode, dent);
  ventry_t *ve = ve_alloc_linked(name, target);
  ve->data = dent;

  *result = ve_moveref(&ve);
  return 0;
}

int ramfs_vn_unlink(vnode_t *dir, vnode_t *vn, ventry_t *ve) {
  TRACE("unlink id=%u \"{:cstr}\"\n", dir->id, &ve->name);
  ramfs_node_t *dnode = dir->data;
  ramfs_node_t *node = vn->data;
  ramfs_dirent_t *dent = ve->data;
  ramfs_dir_remove(dnode, dent);
  return 0;
}

int ramfs_vn_mkdir(vnode_t *dir, cstr_t name, struct vattr *vattr, __move ventry_t **result) {
  TRACE("mkdir id=%u \"{:cstr}\"\n", dir->id, &name);
  vfs_t *vfs = dir->vfs;
  ramfs_node_t *dnode = dir->data;
  ramfs_mount_t *mount = dnode->mount;

  // create the directory node and entry
  ramfs_node_t *node = ramfs_node_alloc(mount, vattr->type, vattr->mode);
  ramfs_dirent_t *dent = ramfs_dirent_alloc(node, name);
  ramfs_dir_add(dnode, dent);

  // create the vnode and ventry
  vnode_t *vn = vn_alloc(node->id, vattr);
  vn->data = node;
  ventry_t *ve = ve_alloc_linked(name, vn);
  ve->data = dent;
  vfs_add_vnode(vfs, vn);

  // create the dot and dotdot entries
  int res;
  ventry_t *dotve = NULL;
  if ((res = vn_hardlink(ve, vn, cstr_new(".", 1), vn, &dotve)) < 0) {
    panic("failed to create dot entry: {:err}\n", res);
  }
  ventry_t *dotdotve = NULL;
  if ((res = vn_hardlink(ve, vn, cstr_new("..", 2), dir, &dotdotve)) < 0) {
    panic("failed to create dotdot entry: {:err}\n", res);
  }
  ve_release(&dotve);
  ve_release(&dotdotve);

  *result = ve_moveref(&ve);
  vn_release(&vn);
  return 0;
}

int ramfs_vn_rmdir(vnode_t *dir, vnode_t *vn, ventry_t *ve) {
  TRACE("rmdir id=%u \"{:cstr}\"\n", dir->id, &ve->name);
  ramfs_node_t *dnode = dir->data;
  ramfs_node_t *node = vn->data;
  ramfs_dirent_t *dent = ve->data;
  ramfs_dir_remove(dnode, dent);
  return 0;
}

//

void ramfs_vn_cleanup(vnode_t *vn) {
  // DPRINTF("cleanup\n");
  ramfs_node_t *node = vn->data;
  if (!node)
    return;

  if (node->type == V_REG) {
    // release file resources now
    ramfs_file_t *file = node->n_file;
    ramfs_file_free(file);
    node->n_file = NULL;
  } else if (node->type == V_LNK) {
    str_free(&node->n_link);
  }

  ramfs_node_free(node);
}

void ramfs_ve_cleanup(ventry_t *ve) {
  // DPRINTF("cleanup\n");
  ramfs_dirent_t *dent = ve->data;
  if (!dent)
    return;

  str_free(&dent->name);
  ramfs_dirent_free(dent);
}
