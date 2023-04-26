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

static struct entry *entries[MAX_ENTRIES];

//method that print all entries in the system.
void print_entries() {
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if (entries[i]) {
			printf("entries[i]->name: %s \n", entries[i]->name);
			//Print the path
			printf("entries[i]->full_path: %s \n", entries[i]->full_path);
		}
	}
}

char* get_parent_path(const char* path) {
    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return NULL;
    }
    int parent_len = last_slash - path;
    char* parent_path = (char*) malloc(parent_len + 2);
    strncpy(parent_path, path, parent_len + 1);
    parent_path[parent_len + 1] = '\0';
	printf("parent_path: %s \n", parent_path);
    return parent_path;
}

int find_empty_entry() {
	for (int i = 0; i < MAX_ENTRIES; i++) {
		// if entry is empty.
		if (!entries[i]) {
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
	name = name + 1;
    printf("name: %s \n", name);
    return strdup(name);
}

struct entry *get_entry(const char *path) {
	for (int i = 0; i < MAX_ENTRIES; i++)
	{	
		printf("%d\n", i);
		if (entries[i]) {
			if (strcmp(entries[i]->full_path, path) == 0) {
				printf("get_entry: found entry: %s \n", entries[i]->name);
				return entries[i];
			}
		}
	}
	printf("Entry not found");
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
			printf("lfs_getattr: Entry not found\n");
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
	// (void) offset;
	// (void) fi;

	printf("readdir: (path=%s)\n", path);


	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//Find all files in the directory.
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if(entries[i]) {
			printf("lfs_readdir: entries[i]->path: %s \n", entries[i]->path);
			printf("path: %s \n", path);
			if (strcmp(entries[i]->path, path) == 0) {
				filler(buf, entries[i]->name, NULL, 0);
			}
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
	int index = find_empty_entry();
	printf("index: %d \n", index);

	entries[index] = calloc(sizeof(struct entry), 1);

	if(!entries[index]){
		return -ENOMEM;
	}

	struct entry *e = entries[index];

	e->name = get_entry_name(path);
	e->path = get_parent_path(path);
	e->full_path = malloc(strlen(path) + 1);
	if (e->full_path == NULL) {
 	   free(e);
 	   return -ENOMEM;
	}
	strcpy(e->full_path, path);
	e->is_dir = true;
	e->file_size = 0;
	
	// find empty entry and add it.
	entries[index] = e;
	printf("LFS_mkdir added entry: %s  on index: %d \n", entries[index]->name, index);

	printf("Print all entries: \n\n\n");
	print_entries();
	printf("\n");

	return 0;
}

int main( int argc, char *argv[] ) {

	// Initialize the entries array to NULL pointers
    memset(entries, 0, MAX_ENTRIES * sizeof(struct entry*));

	fuse_main( argc, argv, &lfs_oper );

	return 0;
}
