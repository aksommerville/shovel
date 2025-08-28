#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#if USE_mswin
  const char path_separator='\\';
#else
  const char path_separator='/';
#endif

/* Read entire file.
 */
 
int file_read(void *dstpp,const char *path) {
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write entire file.
 */
 
int file_write(const char *path,const void *src,int srcc) {
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* mkdir
 */

#if USE_mswin
  #define mkdir(path,mode) mkdir(path)
#endif

int dir_mkdir(const char *path) {
  if (!path||!path[0]) return -1;
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

int dir_mkdirp(const char *path) {
  if (!path||!path[0]) return -1;
  if (mkdir(path,0775)>=0) return 0;
  if (errno==EEXIST) return 0;
  if (errno!=ENOENT) return -1;
  int sepp=path_split(path,-1);
  if (sepp<=0) return -1;
  char nextpath[1024];
  if (sepp>=sizeof(nextpath)) return -1;
  memcpy(nextpath,path,sepp);
  nextpath[sepp]=0;
  if (dir_mkdirp(nextpath)<0) return -1;
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

int dir_mkdirp_parent(const char *path) {
  int sepp=path_split(path,-1);
  if (sepp<=0) return -1;
  char nextpath[1024];
  if (sepp>=sizeof(nextpath)) return -1;
  memcpy(nextpath,path,sepp);
  nextpath[sepp]=0;
  return dir_mkdirp(nextpath);
}

/* Split path.
 */

int path_split(const char *path,int pathc) {
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int p=pathc;
  while (p&&(path[p-1]==path_separator)) p--; // strip trailing separators
  while (p&&(path[p-1]!=path_separator)) p--; // strip basename
  return p-1; // index of separator, or -1 if there wasn't one, nice and neat.
}
