/* fs.h
 */
 
#ifndef FS_H
#define FS_H

int file_read(void *dstpp,const char *path);
int file_write(const char *path,const void *src,int srcc);

#endif
