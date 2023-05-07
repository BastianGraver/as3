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
	.release = NULL,
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
static FILE *fp;

// Take a full path and return the parent path
char* get_parent_path(const char* path) {
    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return NULL;
    }

    size_t parent_len = last_slash - path;
    char* parent_path = (char*) malloc(parent_len + 2);
    if (parent_path == NULL) {
        return NULL;
    }

    strncpy(parent_path, path, parent_len + 1);
    parent_path[parent_len + 1] = '\0';
    return parent_path;
}



// Find a empty entry in the array entries
int find_empty_entry() {
	for (int i = 0; i < MAX_ENTRIES; i++) {
		if (entries[i] == NULL) {
			return i;
		}
	}
	fprintf("No more space for entries\n");
	return -1;
}

// Take a full path and return the name of the file / folder
char* get_entry_name(char *path) {
	char *name = strrchr(path, '/');
	if (name == NULL) {
		return path;
	}
	name = name + 1;
    return strdup(name);
}


struct entry *get_entry(const char *path) {
	for (int i = 0; i < MAX_ENTRIES; i++)
	{	
		if (entries[i] != NULL) {
			if (strcmp(entries[i]->full_path, path) == 0) {
				return entries[i];
			}
		}
	}
	printf("Entry not found");
	return NULL;
}

int lfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strlen(path) == 1) { // root directory
        stbuf->st_mode = S_IFDIR | 0755; 
        stbuf->st_nlink = 2;
    } else {
        const struct entry *const e = get_entry(path);
        if (e == NULL) {
            printf("lfs_getattr: Entry not found\n");
            return -ENOENT;
        }
        stbuf->st_nlink = e->is_dir ? 2 : 1;
        stbuf->st_mode = e->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0777); 
        stbuf->st_size = e->file_size;
    }
    return 0;
}


int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//If in directory.
	if (path != NULL && strcmp(path, "/") != 0) {
		for (int i = 0; i < MAX_ENTRIES; i++)
		{
			if(entries[i]) {
				char* tmp = strdup(path);
				strcat(tmp, "/");
				if (strcmp(entries[i]->path, tmp) == 0) {
					filler(buf, entries[i]->name, NULL, 0);
				}
			}
		}
		return 0;
	}

	//If in root
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if (entries[i]!= NULL && strcmp(entries[i]->path, path) == 0) {
			filler(buf, entries[i]->name, NULL, 0);
		}
	}
	return 0;
}

//Create a file node
int lfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	int index = find_empty_entry();
	if (index == -1) {
		printf("lfs_mknod: No more space for entries");
		return -1;
	}

	//Create a new file
	struct entry *e = (struct entry*) malloc(sizeof(struct entry));
	if (!e) {
        printf("lfs_mknod: Failed to allocate memory\n");
        return -ENOMEM;
    }
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
	//Find the entry
	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_unlink: Entry not found\n");
		return -ENOENT;
	}

	//Remove the entry
	for (int i = 0; i < MAX_ENTRIES; i++)
	{
		if (entries[i] == e) {
			entries[i] = NULL;
			free(e);
			entries_count--;
			return 0;
		}
	}
	return -ENOENT;
}

int lfs_open( const char *path, struct fuse_file_info *fi ) {
	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}

	fi->fh = (uint64_t) e;
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
	struct entry *e = (struct entry*) fi->fh;
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}
	if (e->data == NULL) {
		printf("lfs_read: No data found\n");
		return -ENOENT;
	}
	memcpy(buf, e->data, size); 
	e->access_time = time(NULL);
	return size;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	struct entry *e = (struct entry*) fi->fh;
	if (e == NULL) {
		printf("lfs_open: Entry not found\n");
		return -ENOENT;
	}

	// Resize the buffer if necessary
	if (offset + size > e->file_size) {
		char *new_data = realloc(e->data, offset + size);
		if (new_data == NULL) {
			return -ENOMEM;
		}
		e->data = new_data;
		e->file_size = offset + size;
	}

	memcpy(e->data + offset, buf, size);
	e->access_time = time(NULL);
	e->modification_time = time(NULL);

	return size;
}

int lfs_truncate(const char* path, off_t size) {
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
   	if (new_buf == NULL) {
        printf("lfs_truncate: Failed to allocate new buffer\n");
        return -ENOMEM;
    }

	memcpy(new_buf, e->data, size);
	free(e->data);

	e->data = new_buf;
	e->file_size = size;
	e->modification_time = time(NULL);
	e->access_time = time(NULL);	
	return 0;

}

int lfs_mkdir(const char *path, mode_t mode) {
	int index = find_empty_entry();
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
	e->access_time = time(NULL);
	e->modification_time = time(NULL);
	e->is_dir = true;
	e->file_size = 0;
    e->data = NULL;
	
	entries[index] = e;
	entries_count++;
	return 0;
}

//Delete a directory
int lfs_rmdir(const char *path) {
	struct entry *e = get_entry(path);
	if (e == NULL) {
		printf("lfs_rmdir: Entry not found\n");
		return -ENOENT;
	}
	if (!e->is_dir) {
		printf("lfs_rmdir: Entry is not a directory\n");
		return -ENOTDIR;
	}
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
	entries_count--;
	return 0;
}

int lfs_utime(const char *path, struct utimbuf *ubuf) {
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

int read_entries_from_file () {
	size_t bytes_read;
	//Read the numbers of entries from the file
	bytes_read = fread(&entries_count, sizeof(int), 1, fp);

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
		for (int i = 0; i < entries_count; i++) {
			size_t size;
			entries[i] = calloc(sizeof(struct entry), 1);
			if (!entries[i]) {
				printf("Error: Could not allocate memory\n");
			}

			//Read full_path
			bytes_read = fread(&size, sizeof(size_t), 1, fp);
			entries[i]->full_path = calloc(sizeof(char), size);
			if(!entries[i]->full_path){
				return -ENOMEM;
				printf("Error: Could not allocate memory\n");
			}

			bytes_read = fread(entries[i]->full_path, size, 1, fp);

			entries[i]->name = calloc(sizeof(char), size);
			entries[i]->path = calloc(sizeof(char), size);
			entries[i]->name = get_entry_name(entries[i]->full_path);
			entries[i]->path = get_parent_path(entries[i]->full_path);


			bytes_read = fread(&entries[i]->is_dir, sizeof(bool), 1, fp);
			bytes_read = fread(&entries[i]->access_time, sizeof(time_t), 1, fp);
			bytes_read = fread(&entries[i]->modification_time, sizeof(time_t), 1, fp);

			if (!entries[i]->is_dir) {
				bytes_read = fread(&entries[i]->file_size, sizeof(int), 1, fp);
				entries[i]->data = calloc(sizeof(char), entries[i]->file_size);
				bytes_read = fread(entries[i]->data, sizeof(char), entries[i]->file_size, fp);
			}
		}
	}
	fclose(fp);
	return 0;
}

//Method that writes the entries to the file
int write_entries_to_file () {
	fwrite(&entries_count, sizeof(int), 1, fp);

	for (int i = 0; i < entries_count; i++) {

		size_t size = strlen(entries[i]->full_path);
		fwrite(&size, sizeof(size_t), 1, fp);
		fwrite(entries[i]->full_path, sizeof(char), size, fp);

		fwrite(&entries[i]->is_dir, sizeof(bool), 1, fp);
		fwrite(&entries[i]->access_time, sizeof(time_t), 1, fp);
		fwrite(&entries[i]->modification_time, sizeof(time_t), 1, fp);

		if(!entries[i]->is_dir) {
			fwrite(&entries[i]->file_size, sizeof(int), 1, fp);
			fwrite(entries[i]->data, sizeof(char), entries[i]->file_size, fp);
		}

		free(entries[i]->name);
		free(entries[i]->path);
		free(entries[i]->full_path);
		free(entries[i]);
	}
	fclose(fp);
	return 0;
}

int main( int argc, char *argv[] ) {

	// Initialize the entries array to NULL pointers
    memset(entries, 0, MAX_ENTRIES * sizeof(struct entry*));

	fp = fopen(argv[4], "rb");

	if (!fp) {
		printf("Error: File not found\n");
		return -1;
	}

	read_entries_from_file();

	// Initialize the FUSE operations
	fuse_main(4, argv, &lfs_oper);

	// Write the entries to the file
	fp = fopen(argv[4], "wb");
	write_entries_to_file();

	return 0;
}
