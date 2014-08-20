#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#define BUFFSZ 4096
#define MSGSZ sizeof(struct chmsg)

// use as massage from child to parent proccess
struct chmsg{
	int count;
  char letter;
};

int procn = 26;    // number of child-workers processes
int caps = 'A'-'a';

int semCreate(key_t key, int semCount){
  //union semun semopts;  //in this case we can send simply integer
 
  struct sembuf semch;
  int semid;
  int i;
 
  semid = semget(key, semCount, IPC_CREAT|0660);
  if (semid==-1){
    printf("Sem err open");  
    exit(1);
  }

  //semopts.val = 0;
  for(i=0; i<semCount; i++) semctl(semid, i, SETVAL, 0);

  return semid;
}

int semSet(int semid, int val){
  semctl(semid, 0, SETVAL, val);	

	return 0;
}

int semDelete(int semid){
	semctl(semid, 0, IPC_RMID, 0);
  return 0;
}

int semOp(int semid, int val){
  int err;
  struct sembuf semch;

	semch.sem_num = 0;
  semch.sem_op = val;
	
  // for case if we unrxpectedly stop and resume our prog
  // waiting of operation may drop
  do {
	    err = semop(semid, &semch, 1);
	} while (err == -1 && errno == EINTR);
	
	return 0;
}

void info(){
	printf("\nUsage: prog filename\n");
}

int main(int argc, char *argv[]){
  
  key_t key;    // unic key for use System V IPC
  
  int semid;   // id`s for using semophore and
  int shmid;   // shared memory block
	char* buff;  // ptr for shm block

  pid_t pid, ppid;
  int i, rc, count, err;

  char letter; 
  struct chmsg msg, *msgp;
  char msgbuff[MSGSZ]; 

  int letCount[procn];   // array of results of counting

  int fd[2];             // for pipe;

  FILE *fin;

  if (argc==1) {
		info();
    return 0;
  }

  letter='a';

  ppid = pid = getpid();

  err = pipe(fd);        

  // preparing for using System V IPC
  key = ftok(".", 'a');
  
  semid = semCreate(key, 1);

  shmid = shmget( key, BUFFSZ, IPC_CREAT | 0660 );
  buff = shmat(shmid, 0, 0); 

  for(i=0; i<procn; i++){
    letCount[i]=0;
  }

  // create child-workers and give each aim as letter
  for(i=0; i<procn; i++){
    if (fork()==0){
      pid = getpid();
      break;
    }
    letter++;   // after each fork this var will be copy with next val
  } 

  // parent and child-workers braches:
  if (pid==ppid){ 
    // parent process branch 
    close(fd[1]);

    fin = fopen(argv[1], "r");

    while(!feof(fin)){
      rc = fread(buff, sizeof(char), BUFFSZ, fin);
      
      if (rc!=BUFFSZ){
      	buff[rc]='\0';  // end of file
			}
      
      semSet(semid, procn);     // open semophore, for procn instances

			for(i=0; i<procn; i++){
		    rc = read(fd[0], msgbuff, MSGSZ);
		    if (rc!=MSGSZ) exit(2);
				msgp = (struct chmsg*) msgbuff;

		    letCount[msgp->letter-'a'] += msgp->count;
			}
    }

    semctl(semid, 0, IPC_RMID, 0);
    shmdt(buff);        // deattach shmemory block
    shmctl(shmid, IPC_RMID, 0);  // delete shm block
	  
    close(fd[0]);  
    fclose(fin);

		for (i=0; i<procn; i++){
      printf("\nletter \"%c\": %d", 'a'+i, letCount[i]);
    }
  } else {     
  // child-worker process bracnch
    close(fd[0]);  

    while(1){
 
			semOp(semid, -1);          // decrement semophore

		  count = 0;

			for(i=0; i<BUFFSZ; i++){
				if (buff[i]==letter || buff[i]==letter+caps){
					count++; 
				} else if(buff[i]=='\0') {
					
					break;
				}
			}

		  msg.count = count;
      msg.letter = letter;

      write(fd[1], &msg, MSGSZ);
        
      if (i!=BUFFSZ) break;
		
			semOp(semid, 0);             // wait semophore set 0
		}

		close(fd[1]);
  }		// end child

  wait(NULL);
  return 0;
}
