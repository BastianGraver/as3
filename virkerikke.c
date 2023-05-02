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
int lfs_rmdir(const char *path);
int lfs_mknod(const char *path, mode_t mode, dev_t dev);
int lfs_unlink(const char *path);
int lfs_utime(const char *path, struct utimbuf *ubuf);
int lfs_truncate(const char *path, off_t size);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = lfs_mknod,
	.mkdir = lfs_mkdir,
	.unlink = lfs_unlink,
	.rmdir = lfs_rmdir,
	.truncate = lfs_truncate,
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
	bool is_dir;
	int file_size;
	char* data;
	time_t access_time;
	time_t modification_time;
};

static struct entry *entries[MAX_ENTRIES];
static int entries_count = 0;

//method that print all entries in the system.
void print_entries() {
	printf("----------------print_entries----------------\n");
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
	printf("----------------get_parent_path----------------\n");
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
	printf("----------------find_empty_entry----------------\n");
	for (int i = 0; i < MAX_ENTRIES; i++) {
		printf("i: %d \n", entries[i]);
		if (entries[i] == NULL) {
			return i;
		}
	}
	printf("No more space for entries");
	return -1;
}

char* get_entry_name(char *path) {
	printf("----------------get_entry_name----------------\n");
	char *name = strrchr(path, '/');
	if (name == NULL) {
		return path;
	}
	name = name + 1;
    printf("name: %s \n", name);
    return strdup(name);
}

struct entry *get_entry(const char *path) {
	printf("----------------*get_entry----------------\n");
	for (int i = 0; i < MAX_ENTRIES; i++)
	{	
		printf("%d\n", i);
		if (entries[i] != NULL) {
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
	printf("----------------lfs_getattr----------------\n");
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
	//print number of entries
	printf("entries_count: %d \n", entries_count);
	return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	printf("----------------lfs_readdir----------------\n");
	// (void) offset;
	// (void) fi;

	printf("readdir: (path=%s)\n", path);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//If in directory.
	if (path != NULL && strcmp(path, "/") != 0) {
		printf("We are Not in root ");
		//Find all files in the directory.
		for (int i = 0; i < MAX_ENTRIES; i++)
		{
			if(entries[i]) {
				printf("lfs_readdir: entries[i]->path: %s \n", entries[i]->path);
				printf("path: %s \n", path);
				//copy the path
				char* tmp = strdup(path);
				//Add a slash to the end of the path
				strcat(tmp, "/");
				if (strcmp(entries[i]->path, tmp) == 0) {
					filler(buf, entries[i]->name, NULL, 0);
				}
			}
		}
		return 0;
	}

	//If in root
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
//Create a file node
int lfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	printf("----------------lfs_mknod----------------\n");
	printf("mknod: (path=%s)\n", path);
	//Find an empty entry
	int index = find_empty_entry();
	if (index == -1) {
		printf("lfs_mknod: No more space for entries");
		return -1;
	}
	//Create a new file
	struct entry *e = (struct entry*) malloc(sizeof(struct entry));
	e->name = get_entry_name(path);
	e->path = get_parent_path(path);
	e->full_path = strdup(path);
	e->is_dir = 0;
	e->file_size = 0;
	e->access_time = time(NULL);
	e->modification_time = time(NULL);
	entries[index] = e;
	entries_count++;
	return 0;
}

int lfs_unlink(const char *path) {
	printf("----------------lfs_unlink----------------\n");
	printf("unlink: (path=%s)\n", path);
	//Find the entry
	struct entry *e = get_entry(path);
	printf("----------------lfs_unlink----------------\n");
	if (e == NULL) {
		printf("lfs_unlink: Entry not found\n");
		return -ENOENT;
	}
	//Remove the entry
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if (entries[i] == e) {
			printf("SIKE!");
			printf("lfs_unlink: file name %s: removed\n", entries[i]->name);
			entries[i] = NULL;
			free(e);
			entries_count--;
			return 0;
		}
	}
	printf("lfs_unlink: Entry not found\n");
	return -ENOENT;
}

int lfs_open( const char *path, struct fuse_file_info *fi ) {
	printf("----------------lfs_open----------------\n");
	printf("open: (path=%s)\n", path);
	//remove entry from entries array

	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}
	fi->fh = (uint64_t) e;
	printf("fi->fh = (uint64_t) e\n");
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
	printf("----------------lfs_read----------------\n");
    printf("read: (path=%s)\n", path);

	struct entry *e = (struct entry*) fi->fh;
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}
	if (e->data == NULL) {
		printf("lfs_read: No data found\n");
		return -ENOENT;
	}
	//Pinrt e->data
	printf("e->data: %s\n", e->data);

	memcpy(buf, e->data, size); 
	e->access_time = time(NULL);

	//print buffer
	printf("buf: %s\n", buf);

	return size;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("----------------lfs_write----------------\n");
	printf("write: (path=%s)\n", path);

	struct entry *e = (struct entry*) fi->fh;
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}

	//If the file is empty
	if(e->data) {
		free(e->data);
	}

	//print buffer
	printf("buf: %s\n", buf);

	e->data = (char*) malloc(size);
	memcpy(e->data, buf, size); 

	//print e->data
	printf("e->data: %s\n", e->data);

	e->access_time = time(NULL);
	e->modification_time = time(NULL);
	e->file_size = size + offset;
	
	return size;
}

int lfs_truncate(const char* path, off_t size) {
	printf("----------------lfs_truncate----------------\n");
	printf("truncate: (path=%s)\n", path);

	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_truncate: Entry not found\n");
		return -ENOENT;
	}

	if (e->data == NULL) {
		printf("lfs_truncate: No data found\n");
		return -ENOENT;
	}

	//New buffer
	char *new_buf = (char*) malloc(size);
	memcpy(new_buf, e->data, size);
	free(e->data);

	e->data = new_buf;
	e->file_size = size;
	e->modification_time = time(NULL);
	e->access_time = time(NULL);	
	return 0;

}

int lfs_mkdir(const char *path, mode_t mode) {
	printf("----------------lfs_mkdir----------------\n");
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
	e->access_time = time(NULL);
	e->modification_time = time(NULL);
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
	entries_count++;
	return 0;
}

//Function to delete a entry
void delete_entry(const char *path) {
	printf("----------------delete_entry----------------\n");
	printf("delete_entry: (path=%s)\n", path);

	//remove entry from entries array
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if(entries[i]) {
			if (strcmp(entries[i]->full_path, path) == 0) {
				free(entries[i]->name);
				free(entries[i]->path);
				free(entries[i]->full_path);
				free(entries[i]);
				entries[i] = NULL;
			}
		}
	}
}

//Delete a directory
int lfs_rmdir(const char *path) {
	printf("----------------lfs_rmdir----------------\n");
	printf("rmdir: (path=%s)\n", path);
	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_rmdir: Entry not found\n");
		return -ENOENT;
	}
	if (!e->is_dir) {
		printf("lfs_rmdir: Entry is not a directory\n");
		return -ENOTDIR;
	}
	//Delete entry
	delete_entry(path);
	entries_count--;
	return 0;
}

int lfs_utime(const char *path, struct utimbuf *ubuf) {
	printf("----------------lfs_utime----------------\n");
	printf("utime: (path=%s)\n", path);
	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_utime: Entry not found\n");
		return -ENOENT;
	}
	//Update access and modification time
	e->access_time = ubuf->actime;
	e->modification_time = ubuf->modtime;
	return 0;
}

//Function that read the entries from the file
// void read_entries_from_file (FILE *fp) {
// 	printf("----------------read_entries_from_file----------------\n");
// 	printf("read_entries_from_file: (fp=%s)\n", fp);
// 	//Read the numbers of entries from the file
// 	fread(&entries_count, sizeof(int), 1, fp);

// 	if(entries_count > MAX_ENTRIES) {
// 		printf("Error: Too many entries in the file system\n");
// 		return -1;
// 	}

// 	if(entries_count < 0) {
// 		printf("Error: Invalid number of entries in the file system\n");
// 		return -1;
// 	}

// 	if(entries_count > 0) {
// 		//Read the entries from the file
// 		fread(entries, sizeof(struct entry), entries_count, fp);
// 	}
// }

int main( int argc, char *argv[] ) {

	// Initialize the entries array to NULL pointers
    memset(entries, 0, MAX_ENTRIES * sizeof(struct entry*));

	FILE *fp = fopen(argv[3], "rb");

	
	//Read the numbers of entries from the file
	fread(&entries_count, sizeof(int), 1, fp);

	if(entries_count > MAX_ENTRIES) {
		printf("Error: Too many entries in the file system\n");
		return -1;
	}

	if(entries_count < 0) {
		printf("Error: Invalid number of entries in the file system\n");
		return -1;
	}

	if(entries_count > 0) {
		//Read the entries from the file
		printf("Read entries from file\n");
		for (int i = 0; i < entries_count; i++) {
			fread(entries[i], sizeof(struct entry), 1, fp);
			int size;

			//Read name
			fread(&size, sizeof(int), 1, fp);
			fread(entries[i]->name, size, 1, fp);
			//Read path
			fread(&size, sizeof(int), 1, fp);
			fread(entries[i]->path, size, 1, fp);
			//Read full_path
			fread(&size, sizeof(int), 1, fp);
			fread(entries[i]->full_path, size, 1, fp);
			//Read is_dir
			fread(&entries[i]->is_dir, sizeof(bool), 1, fp);
			//Read access_time
			fread(&entries[i]->access_time, sizeof(time_t), 1, fp);
			//Read modification_time
			fread(&entries[i]->modification_time, sizeof(time_t), 1, fp);

			if (!entries[i]->is_dir) {
				//Read file_size
				fread(&entries[i]->file_size, sizeof(int), 1, fp);
				//Read file_content
				fread(entries[i]->data, entries[i]->file_size, 1, fp);
			}
		}
	}
	
	fclose(fp);

	fuse_main( argc, argv, &lfs_oper );


	//Write the entries to the file
	printf("Write entries to file\n");
	fwrite(&entries_count, sizeof(int), 1, fp);

	for (int i = 0; i < entries_count; i++) {
		int size = strlen(entries[i]->name);
		fwrite(&size, sizeof(int), 1, fp);
		fwrite(entries[i]->name, size, 1, fp);


		size = strlen(entries[i]->path);
		fwrite(&size, sizeof(int), 1, fp);
		fwrite(entries[i]->path, size, 1, fp);

		size = strlen(entries[i]->full_path);
		fwrite(&size, sizeof(int), 1, fp);
		fwrite(entries[i]->full_path, size, 1, fp);

		fwrite(&entries[i]->is_dir, sizeof(bool), 1, fp);
		fwrite(&entries[i]->access_time, sizeof(time_t), 1, fp);
		fwrite(&entries[i]->modification_time, sizeof(time_t), 1, fp);

		if(!entries[i]->is_dir) {
			fwrite(&entries[i]->file_size, sizeof(int), 1, fp);
			fwrite(entries[i]->data, entries[i]->file_size, 1, fp);
		}

		free(entries[i]->name);
		free(entries[i]->path);
		free(entries[i]->full_path);
		free(entries[i]);
	}
	fclose(fp);
	return 0;
}
