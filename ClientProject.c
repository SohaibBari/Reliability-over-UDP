#include <stdio.h>
#include <stdlib.h>

// Time function, sockets, htons... file stat
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

// File function and bzero
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>


/* Size of the buffer used to send the file
 * in several blocks
 */
#define BUFFERT 496

/* Function Declaration*/
int duration (struct timeval *start,struct timeval *stop, struct timeval *delta);
int create_client_socket (int port, char* ipaddr);
void RecvAcknowledgements(int sfd, int lastseq[], off_t m[], off_t n[]);
void Timeout(int sfd, off_t m[], off_t n[], int ack[]);

struct sockaddr_in sock_serv;
int l;

struct Dgram 
{
	uint32_t seqnum;
	char data[BUFFERT];
};

//Carries packets to send in this window
struct Dgram d[6];
off_t count;


int main (int argc, char**argv){
	struct timeval start, stop, delta;
    	int sfd,fd;
	int seq=1000;
	long int k;
    	char buf[BUFFERT];
    	off_t m[6],sz;//long
	off_t n[6];
    	l=sizeof(struct sockaddr_in);
	struct stat buffer;
	int lastseq[5];

	for (int i=0;i<5;i++){
		lastseq[i]=0;
	}
	
	//For timeout
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
    
	if (argc != 4){
		printf("Error usage : %s <ip_serv> <port_serv> <filename>\n",argv[0]);
		return EXIT_FAILURE;
	}
    
    	sfd=create_client_socket(atoi(argv[2]), argv[1]);

	setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    
	if ((fd = open(argv[3],O_RDONLY))==-1){
		perror("open fail");
		return EXIT_FAILURE;
	}
    
	//file size
	if (stat(argv[3],&buffer)==-1){
		perror("stat fail");
		return EXIT_FAILURE;
	}
	else
		sz=buffer.st_size;
    
	//preparation to send
	for (int i=1;i<6;i++){
		bzero(&d[i].data,BUFFERT);
	
	}
    
	gettimeofday(&start,NULL);

	//Initializing sequence numbers
	for (int i=1;i<6;i++){
		d[i].seqnum=i;
	
	}

	//Reading data for each variable
	for (int i=1;i<6;i++){
		n[i]=read(fd,d[i].data,BUFFERT);
	
	}
	
	while(n[1]){
		if(n[1]==-1 || n[2]==-1 || n[3]==-1 || n[4]==-1 || n[5]==-1){
			perror("read fails");
			return EXIT_FAILURE;
		}
		
		//Sending 5 packets of data
		for (int i=1;i<6;i++){
			m[i]=sendto(sfd,&d[i],n[i]+4,0,(struct sockaddr*)&sock_serv,l);
			printf("Sending data: %d ",d[i].seqnum);
		}

		if(m[1]==-1 || m[2]==-1 || m[3]==-1 || m[4]==-1 || m[5]==-1){
			perror("send error");
			return EXIT_FAILURE;
		}

		//Counting sent bytes
		count+=m[1]+m[2]+m[3]+m[4]+m[5];
		//fprintf(stdout,"----\n%s\n----\n",buf);

		


		//Handling acks
		RecvAcknowledgements(sfd, lastseq, m, n);
		
		//Setting socket stuff to zero
		for (int i=1;i<6;i++){
			bzero(d[i].data,BUFFERT);
		}

		//Reads data again		
		for (int i=1;i<6;i++){
			n[i]=read(fd,d[i].data,BUFFERT);
		}

		//increment seqnum
		for (int i=1;i<6;i++){
			d[i].seqnum+=5;
		}

	
	}
	//read has returned 0: end of file

// to unlock the service
	m[1]=sendto(sfd,buf,0,0,(struct sockaddr*)&sock_serv,l);
	gettimeofday(&stop,NULL);
	duration(&start,&stop,&delta);
    
	printf("Number of bytes transferred : %ld\n",count);
	printf("On a total size of : %ld \n",sz);
	printf("For a total duration of : %ld.%ld \n",delta.tv_sec,delta.tv_usec);
    
    close(sfd);
    close(fd);
	return EXIT_SUCCESS;
}

/* Function allowing the calculation of the duration of the sending */
int duration (struct timeval *start,struct timeval *stop,struct timeval *delta)
{
    suseconds_t microstart, microstop, microdelta;
    
    microstart = (suseconds_t) (100000*(start->tv_sec))+ start->tv_usec;
    microstop = (suseconds_t) (100000*(stop->tv_sec))+ stop->tv_usec;
    microdelta = microstop - microstart;
    
    delta->tv_usec = microdelta%100000;
    delta->tv_sec = (time_t)(microdelta/100000);
    
    if((*delta).tv_sec < 0 || (*delta).tv_usec < 0)
        return -1;
    else
        return 0;
}

/* Function allowing the creation of a socket
 * Returns a file descriptor
 */
int create_client_socket (int port, char* ipaddr){
    int l;
	int sfd;
    
	sfd = socket(AF_INET,SOCK_DGRAM,0);
	if (sfd == -1){
        perror("socket fail");
        return EXIT_FAILURE;
	}
    
    //preparation of the address of the destination socket
	l=sizeof(struct sockaddr_in);
	bzero(&sock_serv,l);
	
	sock_serv.sin_family=AF_INET;
	sock_serv.sin_port=htons(port);
    if (inet_pton(AF_INET,ipaddr,&sock_serv.sin_addr)==0){
		printf("Invalid IP adress\n");
		return EXIT_FAILURE;
	}
    
    return sfd;
}

void RecvAcknowledgements(int sfd, int lastseq[], off_t m[], off_t n[]){
	int dup,seq,k;
	int ack[6];
	for (int i=1;i<6;i++){
		ack[i]=0;
	}
	while (ack[1]!=1 || ack[2]!=1 || ack[3]!=1 || ack[4]!=1 || ack[5]!=1){
		//Receiving acks
		k=recvfrom(sfd, &seq,4, 0, (struct sockaddr*)&sock_serv,&l);
		//Handling duplicated acks
		while (lastseq[seq%5]>=seq && k>0){
			dup=seq;
			k=recvfrom(sfd, &seq,4, 0, (struct sockaddr*)&sock_serv,&l);
			printf("Handled duplicate Ack, Old: %d, New: %d \n",dup,seq);
		}

		lastseq[seq%5]=seq;

		//Updating Acknowledgement array
		if (k>0){
			printf("Acknowledgment recieved: %d\n", seq);
			if (seq%5>0){
				ack[seq%5]=1;
			}
			else if (seq%5==0){
				ack[5]=1;
			}	
		}

		//Timeout handling
		else if (k==-1){
			Timeout(sfd,m,n,ack);			
		}
	}
}

void Timeout(int sfd, off_t m[], off_t n[], int ack[]){
	printf("Error at acknowledgment: Timeout \n");
	for (int i=1;i<6;i++){
		if (ack[i]!=1){
			printf("Timed out packet in window: %d \n",i);
			m[i]=sendto(sfd,&d[i],n[i]+4,0,(struct sockaddr*)&sock_serv,l);
			count+=m[i];
			printf("Resending data: %d \n",d[i].seqnum);
		}
	}
}
