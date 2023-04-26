#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_ENTRIES 10

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);
int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int lfs_mkdir(const char *path, mode_t mode);

struct inode {
	int id;
	int size;
	int blocks[10];
};

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = lfs_mkdir,
	.unlink = NULL,
	.rmdir = NULL,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lfs_write,
	.rename = NULL,
	.utime = NULL
};

// entry in the system.
struct entry {
	char *name;
	char *path;
	char *full_path;
	struct inode *inode;
	bool is_dir;
	int file_size;
};

static struct entry entires[MAX_ENTRIES];

int find_empty_entry() {
	for (int i = 0; i < MAX_ENTRIES; i++) {
		// if entry is empty.
		if (entires[i].name == NULL) {
			return i;
		}
	}
	printf("No more space for entries");
	return -1;
}

char* get_entry_name(char *path) {
	char *name = strrchr(path, '/');
	if (name == NULL) {
		return path;
	}
	printf("name: %s \n", name + 1);
	return name + 1;
}

struct entry *get_entry(const char *path) {
	for (int i = 0; i < MAX_ENTRIES; i++)
	{	
		if (entires[i].path != NULL && strcmp(entires[i].path, path) == 0) {
			printf("found entry: %s \n", entires[i].name);
			return &entires[i];
		}
	}
	return NULL;
	
}

int lfs_getattr( const char *path, struct stat *stbuf ) {
	printf("Get Attribute");
	printf("getattr: (path=%s)\n", path);
	memset( stbuf, 0, sizeof(struct stat) );

	if(strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		struct entry *e = get_entry(path);
		if (e == NULL) {
			return -ENOENT;
		}
		if(e->is_dir) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		else {
			stbuf->st_mode = S_IFREG | 0777;
			stbuf->st_nlink = 1;
			stbuf->st_size = e->file_size;
		}
	}
	return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);



	//Find all files in the directory.
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if (entires[i].name != NULL && strcmp(entires[i].path, path) == 0) {
			filler(buf, entires[i].name, NULL, 0);
		}
	}

	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
	printf("open: (path=%s)\n", path);
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("write: (path=%s)\n", path);
	return size;
}

int lfs_mkdir(const char *path, mode_t mode) {
	printf("mkdir: (path=%s)\n", path);
	struct entry *e = malloc(sizeof(struct entry));
	e->name = get_entry_name(path);
	e->path = path;
	e->full_path = path;
	e->is_dir = true;
	e->file_size = 0;
	
	// find empty entry and add it.
	int index = find_empty_entry();
	printf("index: %d is empty \n", index);
	entires[index] = *e;




	return 0;
}

int main( int argc, char *argv[] ) {
	fuse_main( argc, argv, &lfs_oper );

	return 0;
}
