/* fs.h
 */
 
#ifndef FS_H
#define FS_H

int file_read(void *dstpp,const char *path);
int file_write(const char *path,const void *src,int srcc);

int dir_mkdir(const char *path);
int dir_mkdirp(const char *path);
int dir_mkdirp_parent(const char *path);

int path_split(const char *path,int pathc);

#endif
