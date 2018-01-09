/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * dummyfs
 *
 * Copyright 2012, 2016, 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Jacek Popko, Katarzyna Baranowska, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/threads.h>
#include <sys/msg.h>
#include <sys/list.h>
#include <unistd.h>

#include "dummyfs.h"
#include "object.h"


struct {
	handle_t mutex;
} dummyfs_common;

extern int dir_find(dummyfs_object_t *dir, const char *name, oid_t *res);
extern int dir_add(dummyfs_object_t *dir, const char *name, oid_t *oid);
extern int dir_remove(dummyfs_object_t *dir, const char *name);



int dummyfs_lookup(oid_t *dir, const char *name, oid_t *res)
{
	dummyfs_object_t *d;
	int err;

	mutexLock(dummyfs_common.mutex);

	if(dir == NULL)
		d = object_get(0);
	else if ((d = object_get(dir->id)) == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (d->type != otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	err = dir_find(d, name, res);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return err;
}

int dummyfs_setattr(oid_t *oid, int type, int attr)
{
	dummyfs_object_t *o;
	int ret = EOK;

	mutexLock(dummyfs_common.mutex);

	if ((o = object_get(oid->id)) == NULL)
		return -ENOENT;

	switch (type) {

		case (atUid):
			o->uid = attr;
			break;

		case (atGid):
			o->gid = attr;
			break;

		case (atMode):
			o->mode = attr;
			break;

		case (atSize):
			ret = dummyfs_truncate(o, attr);
			break;
	}

	object_put(o);
	mutexUnlock(dummyfs_common.mutex);

	return ret;
}

int dummyfs_getattr(oid_t *oid, int type, int *attr)
{
	dummyfs_object_t *o;

	mutexLock(dummyfs_common.mutex);

	if ((o = object_get(oid->id)) == NULL)
		return -ENOENT;

	switch (type) {

		case (atUid):
			*attr = o->uid;
			break;

		case (atGid):
			*attr = o->gid;
			break;

		case (atMode):
			*attr = o->mode;
			break;

		case (atSize):
			*attr = o->size;
			break;
	}

	object_put(o);
	mutexUnlock(dummyfs_common.mutex);

	return EOK;
}

int dummyfs_link(oid_t *dir, const char *name, oid_t *oid)
{
	dummyfs_object_t *d, *o;

	if (name == NULL)
		return -EINVAL;

	mutexLock(dummyfs_common.mutex);

	if ((d = object_get(dir->id)) == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if ((o = object_get(oid->id)) == NULL) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (d->type != otDir) {
		object_put(o);
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	if (o->type == otDir && o->refs > 1) {
		object_put(o);
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	dir_add(d, name, oid);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return EOK;
}


int dummyfs_unlink(oid_t *dir, const char *name)
{
	int ret;
	oid_t oid;
	dummyfs_object_t *o, *d;

	dummyfs_lookup(dir, name, &oid);

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);
	o = object_get(oid.id);

	if (o == NULL) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (o->type == otDir && o->entries != NULL) {
		object_put(d);
		object_put(o);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	object_put(o);
	if ((ret = object_destroy(o)) == EOK) {
		dummyfs_truncate(o, 0);
		free(o);
	}

	dir_remove(d, name);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return ret;
}

int dummyfs_create(oid_t *oid, int type, int mode)
{
	dummyfs_object_t *o;
	unsigned int id;

	mutexLock(dummyfs_common.mutex);
	o = object_create(NULL, &id);

	if (o == NULL)
		return -EINVAL;

	o->type = type;
	o->mode = mode;
	memcpy(oid, &o->oid, sizeof(oid_t));
	mutexUnlock(dummyfs_common.mutex);

	return EOK;
}

int dummyfs_destroy(oid_t *dir, char *name, oid_t *oid)
{
	dummyfs_object_t *d, *o;
	int ret = EOK;

	if (dir == NULL)
		return -EINVAL;

	if (name == NULL)
		return -EINVAL;

	dummyfs_lookup(dir, name, oid);

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);
	o = object_get(oid->id);

	if (o == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (o->type == otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	object_put(o);
	ret = dummyfs_unlink(dir, name);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);
	return ret;
}

int dummyfs_mkdir(oid_t *dir, const char *name, int mode)
{
	dummyfs_object_t *d, *o;
	oid_t oid;
	unsigned int id;
	int ret = EOK;

	if (dir == NULL)
	   return -EINVAL;

	if (name == NULL)
		return -EINVAL;

	if (dummyfs_lookup(dir, name, &oid) == EOK)
		return -EEXIST;

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);

	o = object_create(NULL, &id);

	o->mode = mode;
	o->type = otDir;

	dummyfs_link(dir, name, &o->oid);

	object_put(d);

	mutexUnlock(dummyfs_common.mutex);

	return ret;
}

int dummyfs_rmdir(oid_t *dir, const char *name)
{
	dummyfs_object_t *d, *o;
	oid_t oid;
	int ret = EOK;

	if (dir == NULL)
		return -EINVAL;

	if (name == NULL)
		return -EINVAL;

	dummyfs_lookup(dir, name, &oid);

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);
	o = object_get(oid.id);

	if (o == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (o->type != otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	if (o->entries != NULL)
		return -EBUSY;

	ret = dummyfs_unlink(dir, name);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);
	return ret;
}

/* temp structure will be moved to libphoenix */
typedef struct _dirent {
	id_t d_ino;
	offs_t d_off;
	unsigned int d_type;
	unsigned short int d_reclen;
	unsigned short int d_namlen;
	char d_name[];
} dirent;

int dummyfs_readdir(oid_t *dir, offs_t offs, dirent *dent, unsigned int size)
{
	dummyfs_object_t *d;
	dummyfs_dirent_t *ei;
	dirent *e;
	offs_t diroffs = 0;

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);

	if (d == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (d->type != otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	if ((ei = d->entries) == NULL) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	do {
		if(diroffs >= offs){
			if ((diroffs - offs + sizeof(dirent) + ei->len) > size)
				goto out;
			e = (dirent*) (((char*)dent) + diroffs);
			e->d_ino = (addr_t)&ei;
			e->d_off = diroffs + sizeof(dirent) + ei->len;
			e->d_reclen = sizeof(dirent) + ei->len;
			memcpy(&(e->d_name[0]), ei->name, ei->len);
			diroffs += sizeof(dirent) + ei->len;
		}
		diroffs += sizeof(dummyfs_dirent_t) + ei->len;
		ei = ei->next;
	} while (ei != d->entries);

out:
	object_put(d);
	mutexUnlock(dummyfs_common.mutex);
	return 	diroffs;
}


int dummyfs_ioctl(oid_t* file, unsigned int cmd, unsigned long arg)
{

	//TODO
	return -ENOENT;
}


int dummyfs_open(oid_t *oid)
{
	return EOK;
}

int main(void)
{
	u32 port;
	oid_t toid;
	oid_t root;
	msg_t msg;
	dummyfs_object_t *o;
	unsigned int rid;

	usleep(500000);
	portCreate(&port);
	printf("dummyfs: Starting dummyfs server at port %d\n", port);

	/* Try to mount fs as root */
	if (portRegister(port, "/", &toid) == EOK)
		printf("dummyfs: Mounted as root %s\n", "");

	object_init();

	mutexCreate(&dummyfs_common.mutex);

	/* Create root directory */
	if (dummyfs_create(&root, otDir, 0) != EOK)
		return -1;

	o = object_get(root.id);
	dir_add(o, "..", &root);

	for (;;) {
		msgRecv(port, &msg, &rid);

		switch (msg.type) {
		case mtOpen:
			break;
		case mtWrite:
			mutexLock(dummyfs_common.mutex);
			o = object_get(msg.i.io.oid.id);
			msg.o.io.err = dummyfs_write(o, msg.i.io.offs, msg.i.data, msg.i.size);
			object_put(o);
			mutexUnlock(dummyfs_common.mutex);
			break;
		case mtRead:
			mutexLock(dummyfs_common.mutex);
			o = object_get(msg.i.io.oid.id);
			msg.o.io.err = dummyfs_read(o, msg.i.io.offs, msg.o.data, msg.o.size);
			object_put(o);
			mutexUnlock(dummyfs_common.mutex);
			break;
		case mtClose:
			break;
		}

		msgRespond(port, rid);
	}

	return EOK;
}
