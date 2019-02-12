#include <stdio.h>
#include <stdlib.h>

// Time function, sockets, htons... file stat
#include <sys/time.h>
#include <time.h>
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

/*Size of the buffer used to send the file
 * in several blocks
 */
#define BUFFERT 496


/* Function Declaration*/
int duration (struct timeval *start,struct timeval *stop, struct timeval *delta);
int create_server_socket (int port);
int RecvWindowPackets(int sfd, int lastseq[], off_t n[]);
void CheckDuplicate(int sfd, int lastseq[], off_t n[]);
void OrderPacket(off_t n[]);


struct sockaddr_in sock_serv,clt;
unsigned int l;
off_t count;

struct Dgram {
	uint32_t seqnum;
	char data[BUFFERT];
};

//For receiving packets
struct Dgram d[6];

int main (int argc, char**argv){
    	//Variables
	int fd, sfd;
    	int seq;
	int k;
	char buf[BUFFERT];
	off_t n[6]; // long int type
	char filename[256];
    	l=sizeof(struct sockaddr_in);
	int terminate=0;
	int lastseq[5];
	count=0;

	int error;
	
	for (int i=0;i<5;i++){
		lastseq[i]=0;
	}
	
	

        //Duration calculation variables
	time_t intps;
	struct tm* tmi;
    
	if (argc != 2){
		printf("Error usage : %s <port_serv>\n",argv[0]);
		return EXIT_FAILURE;
	}
    
	//Create socket
    	sfd = create_server_socket(atoi(argv[1]));

	
    
	intps = time(NULL);
	tmi = localtime(&intps);
	bzero(filename,256);
	sprintf(filename,"File.%d.%d.%d.%d.%d.%d",tmi->tm_mday,tmi->tm_mon+1,1900+tmi->tm_year,tmi->tm_hour,tmi->tm_min,tmi->tm_sec);
	printf("Creating the output file : %s\n",filename);
    
	//open file
	if((fd=open(filename,O_CREAT|O_WRONLY|O_TRUNC,0600))==-1){
		perror("open fail");
		return EXIT_FAILURE;
	}
    
	//preparation to recieve
	for (int i=1;i<6;i++){
		bzero(&d[i].data,BUFFERT);
	
	}	

	//Receive packets
	terminate=RecvWindowPackets(sfd, lastseq, n);
	


	while(terminate==0){
		
		printf("%ld bytes of data received in this window \n",n[1]+n[2]+n[3]+n[4]+n[5]);
		printf("Received sequence numbers: %d, %d, %d, %d, %d \n",d[1].seqnum,d[2].seqnum,d[3].seqnum,d[4].seqnum,d[5].seqnum);

		if(n[1]==-1 || n[2]==-1 || n[3]==-1 || n[4]==-1 || n[5]==-1){
			perror("read fails");
			return EXIT_FAILURE;
		}
		count+=n[1]+n[2]+n[3]+n[4]+n[5];

		//Write data
		for (int i=1;i<6;i++){
			write(fd,d[i].data,n[i]-4);
		}

		//Set socket things to zero
		for (int i=1;i<6;i++){
			bzero(d[i].data,BUFFERT);
		}		
        	
		
		//Receive Packets
		terminate=RecvWindowPackets(sfd, lastseq, n);
	}
    
	
	printf("Number of bytes transferred : %ld \n",count);
	printf("Created the output file : %s\n",filename);    

    close(sfd);
    close(fd);
	return EXIT_SUCCESS;
}

/* Function allowing the calculation of the duration of the sending*/
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

/* Function allowing the creation of a socket and its attachment to the system
 * Returns a file descriptor in the process descriptor table
 * bind allows its definition in the system
 */
int create_server_socket (int port){
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
	sock_serv.sin_addr.s_addr=htonl(INADDR_ANY);
    
	//Assign an identity to the socket
	if(bind(sfd,(struct sockaddr*)&sock_serv,l)==-1){
		perror("bind fail");
		return EXIT_FAILURE;
	}
    
    
    return sfd;
}

int RecvWindowPackets(int sfd, int lastseq[], off_t n[]){
	int k;
	for (int i=0;i<5;i++){    	
		n[0]=recvfrom(sfd,&d[0],BUFFERT+4,0,(struct sockaddr *)&clt,&l);
		if (n[0]){
			//getchar();

			// Handling duplicated recieved packet
			CheckDuplicate(sfd, lastseq,n);	
			if (n[0]){
				//Sending Acknowledgements	
				printf("Sending ack of packet: %d \n",d[0].seqnum);
				k=sendto(sfd, &d[0].seqnum,4,0,(struct sockaddr *)&clt,l);

				//Updating lastseq to maintain record of recieved packets
				lastseq[d[0].seqnum%5]=d[0].seqnum;

				//Copying packets in Dgram Array in order for window
				OrderPacket(n);
			}
			else{
				return 1;
			}
		}
		else if (n[0]==0) {
			return 1;
		}
		else {
			printf("Error at receiving \n");
		}
	}
	return 0;
}

void CheckDuplicate(int sfd, int lastseq[], off_t n[]){
	int error,k;
	while (lastseq[d[0].seqnum%5]>=d[0].seqnum && n[0]>0){
		error=d[0].seqnum;

		k=sendto(sfd, &d[0].seqnum,4,0,(struct sockaddr *)&clt,l);
		printf("Sending ack of duplicate packet: %d \n",d[0].seqnum);

		//Recieving again
		n[0]=recvfrom(sfd,&d[0],BUFFERT+4,0,(struct sockaddr *)&clt,&l);
		count+=n[0];
		if(n[0]){
			printf("Handled duplicated, old recieved %d, new recieved %d \n",error,d[0].seqnum);
		}
		else{
			printf("End of file recieved \n");
		}
	}
}

void OrderPacket(off_t n[]){
	if(d[0].seqnum%5>0){
		memcpy(&d[d[0].seqnum%5],&d[0],sizeof(d[0]));
		n[d[0].seqnum%5]=n[0];
		d[d[0].seqnum%5].seqnum=d[0].seqnum;
	}
	else if (d[0].seqnum%5==0){
		memcpy(&d[5],&d[0],sizeof(d[0]));
		n[5]=n[0];
		d[5].seqnum=d[0].seqnum;			
	}
}
