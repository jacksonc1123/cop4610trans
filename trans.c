#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#define MAXBYTES 4097 /* 4KB of data plus null terminated char */

int checkfiles(FILE *in, FILE *out);
int createpipes(int *p1, int *p2);
char *setshmem(int* pmfd, pid_t *ppid, char *name);
int transfile(char *pmem, int *pcfd, int *cpfd, pid_t *ppid, FILE *in, FILE *out);

int
main(int argc, char *argv[])
{
  /* pid_t pid; */
  pid_t ppid;

  int pmfd; /* file descriptor for shared process memory */
  char *pmem; /* shared process memory */
  FILE *in, *out; /* input and output files */
  int pcfd[2], cpfd[2]; /* [0] is read end, [1] is write end */

  char *name = "/cj12d_cop4610";
  char input = '\0';
  
  pmem = NULL;
  /* check input parameters */
  if(argc < 3) {
    fprintf(stderr, "Not enough arguments\n");
    exit(1);
  } else if(argc > 3) {
    fprintf(stderr, "Too many arguments\n");
    exit(1);
  } else {
    if(access(argv[1], F_OK) == 0) /* file exists */
      in = fopen(argv[1], "rb"); /* open for reading */
    else {
      fprintf(stderr, "%s: ", argv[1]);
      perror("");
    }
    if(access(argv[2], F_OK) == 0) { /* file exists */
      printf("Overwrite file, %s?[y/n]: ", argv[2]);
      input = fgetc(stdin);
      if(input == 'y')
	out = fopen(argv[2], "wb"); /* open for writing */
      else {
	fclose(in);
	exit(0);
      }
    } else {
      out = fopen(argv[2], "wb"); /* open for writing */
    }
    if(checkfiles(in, out)) {
      fprintf(stderr, "Error opening files\n");
      exit(1);
    }
  }

  /* create pipes */
  if(createpipes(pcfd,cpfd)) {
    fprintf(stderr, "Error creating pipe\n");
    exit(1);
  }

  /* open shared memory as a file with read/write permissions */
  pmem = setshmem(&pmfd, &ppid, name);
  if(!pmem) {
    fprintf(stderr, "Error setting shared memory\n");
    exit(1);
  }

  /* read from file */
  if(transfile(pmem, pcfd, cpfd, &ppid, in, out)) {
    fprintf(stderr, "Error transferring information\n");
    exit(1);
  }
  
  /* close files */
  fclose(in);
  fclose(out);
  /* remove shared memory */
  shm_unlink(name);
  munmap(pmem, MAXBYTES);
  return 0;
}

int
checkfiles(FILE *in, FILE *out)
{
  if(!in)
    return 1;
  else if(!out)
    return 1;

  return 0;
}

int
createpipes(int *p1, int *p2)
{
  if(pipe(p1) < 0) { /* error error check for parent pipe */
    fprintf(stderr, "Error creating pipe\n");
    return 1;
  }
  if(pipe(p2) < 0) { /* error check for child pipe */
    fprintf(stderr, "Error creating pipe\n");
    return 1;
  }
  return 0;
}

char
*setshmem(int* pmfd, pid_t *ppid, char *name)
{
  pid_t pid;
  char* pmem;

  *pmfd = shm_open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if(*pmfd == -1) {
    fprintf(stderr, "Error creating shared memory\n");
    return 0;
  }
  /* set size of share memory */
  ftruncate(*pmfd, MAXBYTES);

  /* set parent pid 
   * must set outside of fork control block */
  *ppid = getpid();
  pid = fork();
  if(pid < 0) { /* error */
    fprintf(stderr, "There was an error forking the process\n");
  } else if (pid == 0) { /* child */
    /* allocate shared memory for processes */
    pmem = (char*)mmap(0, MAXBYTES, PROT_READ | PROT_WRITE, MAP_SHARED, *pmfd, 0);
  } else { /* parent */
    pmem = (char*)mmap(0, MAXBYTES, PROT_READ | PROT_WRITE, MAP_SHARED, *pmfd, 0);
  }
  pmem[MAXBYTES-1] = '\0';
  return pmem;
}

int
transfile(char *pmem, int *pcfd, int *cpfd, pid_t *ppid, FILE *in, FILE *out)
{
  struct pipedata {
    int blocknum;
    int blocklen;
  } pinfo, cinfo;

  ssize_t cbytes, pbytes;
  /* initialize struct data to 0 */
  pinfo.blocknum = cinfo.blocknum = -1;
  pinfo.blocklen = cinfo.blocklen = 1;

  while(1) {
    if(*ppid == getpid()) { /* parent */
      /* Check cpfd. [0] is read end, [1] is write end */
      if(pinfo.blocknum != -1) { /* -1 means first read */
  	close(cpfd[1]); /* close write end */
  	pbytes = read(cpfd[0], (void*)(&pinfo), sizeof(pinfo));
  	if(pbytes < 0) {
	  /* close files */
	  fclose(in);
	  fclose(out);
	  fprintf(stderr, "Read in proc[%d]: ", getpid());
  	  perror("");
  	  exit(1);
  	}
      }
      if(pinfo.blocknum == 0) { /* child successfully exited */
  	return 0;
      } else if(pinfo.blocknum == -1) { /* first read */
  	pinfo.blocknum = 0;
      }

      /* write this data to pcfd */
      close(pcfd[0]); /* close read end */
      /* Operate on shared memory */
      pinfo.blocklen = (fread(pmem, sizeof(char), MAXBYTES-1, in));
      if(pinfo.blocklen == 0) { /* file end */
  	pinfo.blocknum = 0;
      } else {
	++(pinfo.blocknum);
      }
      write(pcfd[1], (void*)(&pinfo), sizeof(pinfo));
    } else { /* child */
      /* Check pcfd */
      close(pcfd[1]); /* close write end */
      cbytes = read(pcfd[0], (void*)(&cinfo), sizeof(cinfo));
      if(cbytes < 0) {
  	fprintf(stderr, "Read in proc[%d]: ", getpid());
	perror("");
	/* close files */
	fclose(in);
	fclose(out);
  	exit(1);
      }
      /* write this data to cpfd */
      close(cpfd[0]);
      /* close(cpfd[1]); */
      if(cinfo.blocknum == 0) { /* all blocks transferred */
	/* close files */
	fclose(in);
	fclose(out);
  	exit(EXIT_SUCCESS); /* exit success */
      } else {
  	/*write from shared memory to output file */
  	fwrite(pmem, cinfo.blocklen, sizeof(char), out);
  	/* send blocknum back to parent */
  	write(cpfd[1], (void*)(&cinfo), sizeof(cinfo));
      }
    }
  }

  /* Don't get here */
  return 1;
}

