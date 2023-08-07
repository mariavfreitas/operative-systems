#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Given a path, fills pointers with strings for the parent path and child
 * file name
 * Input:
 *  - path: the path to split. ATENTION: the function may alter this parameter
 *  - parent: reference to a char*, to store parent path
 *  - child: reference to a char*, to store child file name
 */
void split_parent_child_from_path(char * path, char ** parent, char ** child) {
	int n_slashes = 0, last_slash_location = 0;
	int len = strlen(path);

	if (path[len-1] == '/') {
		path[len-1] = '\0';
	}

	for (int i=0; i < len; ++i) {
		if (path[i] == '/' && path[i+1] != '\0') {
			last_slash_location = i;
			n_slashes++;
		}
	}

	if (n_slashes == 0) {
		*parent = "";
		*child = path;
		return;
	}

	path[last_slash_location] = '\0';
	*parent = path;
	*child = path + last_slash_location + 1;
}

/*
 * Initializes tecnicofs and creates root node.
 */
void init_fs() {
	inode_table_init();
	
	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	
	if (root != FS_ROOT) {
		printf("failed to create node for tecnicofs root\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
}

/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 */

int is_dir_empty(DirEntry *dirEntries) {
	if (dirEntries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {
			return FAIL;
		}
	}
	return SUCCESS;
}

/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 */
int lookup_sub_node(char *name, DirEntry *entries) {
	if (entries == NULL) {
		return FAIL;
	}

	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            return entries[i].inumber;
        }
    }
	return FAIL;
}

/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 */
int create(char *name, type nodeType){
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;
	char path[MAX_FILE_NAME], *token;
	char reverse_path[MAX_FILE_NAME] = "";

	int parent_inumber, child_inumber;
	int locked_inodes[INODE_TABLE_SIZE];
	int i = 0, current_inumber;

	/* use for copy */
	type pType;
	union Data pdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	strcpy(path, parent_name);

	/*Bloco em que o caminho e protegido*/
	token = strtok_r(path, delim, &saveptr);
	if(token == NULL) {
		wrlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
	} else {
		rdlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
		strcat(reverse_path, token);
	}
	while(token != NULL){
		token = strtok_r(NULL, delim, &saveptr);
		current_inumber = lookup(reverse_path);
		/*se o proximo for nulo é wrlock*/
		if(token == NULL){
			if(current_inumber > 0){
				wrlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		} else { /*c.c. é rdlock*/
			if (current_inumber > 0){
				rdlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
				strcat(reverse_path, "/");
				strcat(reverse_path, token);
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		}
	}

	i--;
	parent_inumber = lookup(parent_name);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n", name, parent_name);

		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n", name, parent_name);

		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n", child_name, parent_name);

		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);
	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n", child_name, parent_name);

		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	wrlock(child_inumber);

	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);

		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		unlock(child_inumber);
		return FAIL;
	}

	for (; i >= 0; i--){
		unlock(locked_inodes[i]);
	}

	unlock(child_inumber);
	return SUCCESS;
}

/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 */
int delete(char *name){
	int parent_inumber, child_inumber;
	int locked_inodes[INODE_TABLE_SIZE];
	int i = 0, current_inumber;	

	char delim[] = "/";
	char *saveptr;
	char path[MAX_FILE_NAME], *token;
	char reverse_path[MAX_FILE_NAME] = "";
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];

	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	strcpy(path, parent_name);

	/*Bloco em que o caminho e protegido*/
	token = strtok_r(path, delim, &saveptr);
	if(token == NULL) {
		wrlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
	} else {	
		rdlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
		strcat(reverse_path, token);
	}
	while(token != NULL){
		token = strtok_r(NULL, delim, &saveptr);
		current_inumber = lookup(reverse_path);
		/*se o proximo for nulo é wrlock*/
		if(token == NULL){
			if(current_inumber > 0){
				wrlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		} else { /*c.c. é rdlock*/
			if (current_inumber > 0){
				rdlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
				strcat(reverse_path, "/");
				strcat(reverse_path, token);
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		}
	}

	i--;
	parent_inumber = lookup(parent_name);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n", child_name, parent_name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n", child_name, parent_name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n", name, parent_name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		return FAIL;
	}

	wrlock(child_inumber);
	inode_get(child_inumber, &cType, &cdata);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n", name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		unlock(child_inumber);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		unlock(child_inumber);
		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		for (; i >= 0; i--){
			unlock(locked_inodes[i]);
		}
		unlock(child_inumber);
		return FAIL;
	}

	for (; i >= 0; i--){
		unlock(locked_inodes[i]);
	}

	unlock(child_inumber);
	return SUCCESS;
}

/*
 * Lookup for a given path without protecting it.
 * Called by delete, create and move functions.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup(char *name) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok_r(full_path, delim, &saveptr);

	/* search for all sub nodes */
	while ((path != NULL) && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_get(current_inumber, &nType, &data);
		path = strtok_r(NULL, delim, &saveptr);
	}
	return current_inumber;
}

/*
 * Lookup for a given path while protecting it.
 * Called in applyCommands.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup_protect(char *name) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	strcpy(full_path, name);

	char *token;
	char reverse_path[MAX_FILE_NAME] = "";
	int locked_inodes[INODE_TABLE_SIZE];
	int i = 0;

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	/*Bloco em que o caminho e protegido*/
	token = strtok_r(full_path, delim, &saveptr);
	if(token == NULL) {
		wrlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
	} else {
		rdlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
		strcat(reverse_path, token);
	}
	while(token != NULL){
		token = strtok_r(NULL, delim, &saveptr);
		current_inumber = lookup(reverse_path);
		if(token == NULL){
			if(current_inumber > 0){
				wrlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		} else { /*c.c. é rdlock*/
			if (current_inumber > 0){
				rdlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
				strcat(reverse_path, "/");
				strcat(reverse_path, token);
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		}
	}

	i--;
	for (; i >= 0; i--){
		unlock(locked_inodes[i]);
	}

	return current_inumber;
}

/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){

	wrlock(FS_ROOT);
	inode_print_tree(fp, FS_ROOT, "");
	unlock(FS_ROOT);
}

/*
 * Moves a directory/file to a new directory.
 * Input:
 *  - name: path of original node
 *  - new_name: path of new node
 * Returns:
 *  	  0: when successful
 *     	 -1: otherwise
 */
int move(char * name, char * new_name){
	char fullpath_name[MAX_FILE_NAME];
	strcpy(fullpath_name, name);

	char *child_name, *parent_name, *new_child_name, *new_parent_name;
	char *saveptr, *new_saveptr, *token, *new_token;

	char reverse_path[MAX_FILE_NAME] = "";
	char new_reverse_path[MAX_FILE_NAME] = "";
	char path[MAX_FILE_NAME] = ""; 
	char new_path[MAX_FILE_NAME] = "";
	char delim[] = "/";

	int i = 0, j = 0;
	int current_inumber, new_current_inumber;  
	int locked_inodes[INODE_TABLE_SIZE], new_locked_inodes[INODE_TABLE_SIZE];
	int name_inumber, parent_inumber, child_inumber, new_parent_inumber, new_child_inumber;

	type newpType, pType;
	union Data newpdata, pdata;

	split_parent_child_from_path(new_name, &new_parent_name, &new_child_name);
	split_parent_child_from_path(name, &parent_name, &child_name);

	/*Bloco em que o caminho A e protegido*/
	strcpy(path, parent_name);
	token = strtok_r(path, delim, &saveptr);
	if(token == NULL) {
		wrlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
	} else {
		rdlock(FS_ROOT);
		locked_inodes[i] = FS_ROOT;
		i++;
		strcat(reverse_path, token);
	}
	while(token != NULL){
		token = strtok_r(NULL, delim, &saveptr);
		current_inumber = lookup(reverse_path);
		/*se o proximo for nulo é wrlock*/
		if(token == NULL){
			if(current_inumber > 0){
				wrlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		} else { /*c.c. é rdlock*/
			if (current_inumber > 0){
				rdlock(current_inumber);
				locked_inodes[i] = current_inumber;
				i++;
				strcat(reverse_path, "/");
				strcat(reverse_path, token);
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		}
	}

	i--;

	/*Bloco em que o caminho B e protegido*/
	strcpy(new_path, new_parent_name);
	new_token = strtok_r(new_path, delim, &new_saveptr);
	if(new_token == NULL) {
		wrlock(FS_ROOT);
		new_locked_inodes[j] = FS_ROOT;
		j++;
	} else {
		rdlock(FS_ROOT);
		new_locked_inodes[j] = FS_ROOT;
		j++;
		strcat(new_reverse_path, new_token);
	}
	while(new_token != NULL){
		new_token = strtok_r(NULL, delim, &new_saveptr);
		new_current_inumber = lookup(new_reverse_path);
		/*se o proximo for nulo é wrlock*/
		if(new_token == NULL){
			if(new_current_inumber > 0){
				wrlock(new_current_inumber);
				new_locked_inodes[j] = new_current_inumber;
				j++;
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		} else { /*c.c. é rdlock*/
			if (new_current_inumber > 0){
				rdlock(new_current_inumber);
				new_locked_inodes[j] = current_inumber;
				j++;
				strcat(new_reverse_path, "/");
				strcat(new_reverse_path, new_token);
			} else {
				i--;
				for (; i >= 0; i--){
					unlock(locked_inodes[i]);
				}
				return FAIL;
			}
		}
	}

	name_inumber = lookup(fullpath_name);
	if (name_inumber == FAIL) {
		printf("error: original directory does not exist\n");
		return -1;
	}

	new_parent_inumber = lookup(new_parent_name);
	if (new_parent_inumber == FAIL) {
		printf("error: destination directory does not exist\n");
		return -1;
	}

	inode_get(new_parent_inumber, &newpType, &newpdata);
	if(newpType != T_DIRECTORY) {
		printf("error: destination is not a directory\n"); 
		return -1;
	}

	if (strcmp(name, new_parent_name) == 0) {
		printf("Error: can not move directory into subdirectory\n");
		return -1;
	}

	new_child_inumber = lookup_sub_node(new_parent_name, newpdata.dirEntries);
	
	if (new_child_inumber != FAIL) {
		printf("error: destination already exists\n");
		return -1;
	}

	parent_inumber = lookup(parent_name);	
	child_inumber = lookup(fullpath_name);

	wrlock(child_inumber);

	inode_get(parent_inumber, &pType, &pdata);
	dir_add_entry(new_parent_inumber, child_inumber, new_child_name);
	dir_reset_entry(parent_inumber, child_inumber);

	for (; i >= 0; i--){
		unlock(locked_inodes[i]);
	}
	for (; j >= 0; j--){
		unlock(new_locked_inodes[j]);
	}

	unlock(child_inumber);

	return 0;
}
