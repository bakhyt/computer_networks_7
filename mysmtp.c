#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

//Server configurations
#define PORT 2020 
#define MAX_USER 50
#define BUF_SIZE 4096

//User information saving file path
const char data_dir[] = "/home/bakhyt/data/";

const char userinfo[] = "userinfo";

//reply codes
const char reply[][100]={
	{" \r\n"},  //0
	{"200 Mail OK.\r\n"},   //1
	{"211 System status, or system help reply.\r\n"},  //2
	{"214 Help message.\r\n"},   //3
	{"220 Ready\r\n"},  //4
	{"221 Bye\r\n"},  //5
	{"250 OK\r\n"},  //6
	{"251 User not local; will forward to %s<forward-path>.\r\n"},  //7
	{"354 Send from Rising mail proxy\r\n"},  //8
	{"421 service not available, closing transmission channel\r\n"},  //9
	{"450 Requested mail action not taken: mailbox unavailable\r\n"},  //10
	{"451 Requested action aborted: local error in processing\r\n"},   //11
	{"452 Requested action not taken: insufficient system storage\r\n"}, //12
	{"500 Syntax error, command unrecognised\r\n"},  //13
	{"501 Syntax error in parameters or arguments\r\n"},  //14
	{"502 Error: command not implemented\r\n"},  //15
	{"503 Error: bad sequence of commands\r\n"}, //16
	{"504 Error: command parameter not implemented\r\n"},  //17
	{"521 <domain>%s does not accept mail (see rfc1846)\r\n"},  //18
	{"530 Access denied \r\n"},  //19
	{"550 Requested action not taken: mailbox unavailable\r\n"},  //20
	{"551 User not local; please try <forward-path>%s\r\n"},  //21
	{"552 Requested mail action aborted: exceeded storage allocation\r\n"},  //22
};

void smtp(int *param);

int answer(int client_sockfd);

void send_data(int sockfd, const char* data);

void mail_data(int sockfd);

int check_user();

void vrfy(int sockfd);

int level = 0;

int to_num = 0;

char from[50] = "";

char verify[50] = "";

char ask[50]="";

char to[MAX_USER][50] = {""};

time_t mytime;

int main(int argc,char* argv[]) {

	int server_sockfd, client_sockfd;
	
	socklen_t sin_size;

	struct sockaddr_in server_addr, client_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	
	//create a socket
	if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

		perror("S:socket create error！\n");

		exit(1);

	}

	//set the socket's attributes
	server_addr.sin_family = AF_INET;

	server_addr.sin_port = htons(PORT);

	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//create a link (binding)
	if (bind(server_sockfd, (struct sockaddr *) &server_addr,sizeof(struct sockaddr)) == -1) {

		perror("S:bind error！\n");

		exit(1);

	}

	//listening requests from users
	if (listen(server_sockfd, MAX_USER - 1) == -1) {

		perror("S:listen error！\n");

		exit(1);

	}
				
	printf("SMTP server...\n");
	
	sin_size = sizeof(client_addr);
	
	mytime=time(NULL);	

	//accept requests from users, loop and wait.
	while (1) {
		
		if ((client_sockfd = accept(server_sockfd, (struct sockaddr *) &client_addr, &sin_size)) == -1) {
			
			perror("S:accept error!\n");

			sleep(1);

			continue;
		}
				
		printf("S:received a connection from %s at %s\n",inet_ntoa(client_addr.sin_addr),ctime(&mytime));	

		smtp(&client_sockfd);
		

	}

	close(client_sockfd);

	return 0;

}

// process mailing events
void smtp(int* param) {

	int client_sockfd, len;

	char buf[50];

	memset(buf, 0, sizeof(buf));

	client_sockfd = *param;

	send_data(client_sockfd, reply[4]); //send 220

	level = 1;

	while (1) {

		memset(buf, 0, sizeof(buf));

		len = recv(client_sockfd, buf, sizeof(buf), 0);

		if (len > 0) {
			
			printf("Request stream: %s",buf);			
			
			strcpy(ask,buf);

			answer(client_sockfd);
			

		} else {
			
			printf("S: sleeping...\n"); fflush(NULL); 
		
			break;
		} 		

	}
	
	printf("S:[%d] socket closed by client.\n",client_sockfd);
	
	fflush(NULL);

}

//answer to the connected user
int answer(int client_sockfd) {
			
	if (strncmp(ask, "HELO", 4) == 0) {
		
		if (level == 1) {

			send_data(client_sockfd, reply[6]);

			to_num = 0;

			memset(to, 0, sizeof(to));

			level = 2;

		} else {

			send_data(client_sockfd, reply[15]);
		}

	} else if (strncmp(ask, "MAIL FROM", 9) == 0) {

		if (level == 2) {

			char *pa, *pb;

			pa = strchr(ask, '<');

			pb = strchr(ask, '>');

			strncpy(from, pa + 1, pb - pa - 1);

			if (check_user()) {

				send_data(client_sockfd, reply[6]);

				level = 3;

			} else {

				send_data(client_sockfd, reply[19]);

			}


		} else {

			send_data(client_sockfd, reply[16]);

		}

	} else if (strncmp(ask, "RCPT TO", 7) == 0) {

		if ((level == 3 || level == 4) && to_num < MAX_USER) {

			char *pa, *pb;

			pa = strchr(ask, '<');

			pb = strchr(ask, '>');

			strncpy(to[to_num++], pa + 1, pb - pa - 1);

			send_data(client_sockfd, reply[6]);

			level = 4;

		} else {

			send_data(client_sockfd, reply[16]);

		}

	} else if (strncmp(ask, "DATA", 4) == 0) {

		if (level == 4) {

			send_data(client_sockfd, reply[8]);

			mail_data(client_sockfd);

			level = 5;

		} else {

			send_data(client_sockfd, reply[16]);

		}

	} else if (strncmp(ask, "RSET", 4) == 0) {

		level = 1;

		send_data(client_sockfd, reply[6]);

	} else if (strncmp(ask, "NOOP", 4) == 0) {

		send_data(client_sockfd, reply[6]);

	} else if (strncmp(ask, "QUIT", 4) == 0) {

		level = 0;
		
		send_data(client_sockfd, reply[5]); return 0;
	
	} else if (strncmp(ask, "VRFY", 4) == 0) {
			
			vrfy(client_sockfd);
			
	} else {

		send_data(client_sockfd, reply[16]);

	}

}

//send data to user
void send_data(int sockfd, const char* data) {

	if (data != NULL) {

		send(sockfd, data, strlen(data), 0);

		printf("Reply stream: %s",data);

	}

}

//recieve mail contents
void mail_data(int sockfd) {

	sleep(1);

	char buf[BUF_SIZE], mail[256];
	
	int count1=0;

	memset(buf, 0, sizeof(buf));
			
	while(1)
	{
		memset(mail, 0, sizeof(mail));
		
		recv(sockfd, mail, sizeof(mail), 0);
		
		if(mail[0]=='.') { if(strlen(mail)==3) break;}

		strcat(buf,mail);	

	}

	//strcat(buf,mail);

	int i;
	
	mytime=time(NULL);

	char file[80], tp[20];
	
	//mail content store
	for (i = 0; i < to_num; i++) {

		strcpy(file, data_dir);

		strcat(file, to[i]);

		if (access(file,0) == -1) {

			mkdir(file,0777);

		}

		sprintf(tp, "/%s", ctime(&mytime));

		strcat(file, tp);

		FILE* fp = fopen(file, "w+");

		if (fp != NULL) {

			fwrite(buf, 1, strlen(buf), fp);

			fclose(fp);

		} else {

			printf("File open error!\n");

		}

	}

	send_data(sockfd, reply[6]);

}

//check the user from the database
int check_user() {

	FILE* fp;

	char file[80] = "";

	char data[50];

	strcpy(file, data_dir);

	strcat(file, userinfo);

	fp = fopen(file, "r");

	while (fgets(data, sizeof(data), fp) > 0) {

		if (strncmp(from, data, strlen(from)) == 0) // valid user

			return 1;

	}

	return 0;

}

//verify command
void vrfy(int sockfd)
{	
	FILE* fp;
	
	char file[80] = "";

	char data[50], buffer[50];

 	int count1 = 0, count2 = 0, i=0, j, flag, count=0;

	char *pa, *pb;

	pa = (ask+5);

    	pb = strchr(ask, '\0');

    	strncpy(verify, pa, pb - pa);	

	while (verify[count2] != '\0') count2++;
	
	count2=count2-2;	
	
	strcpy(file, data_dir);

	strcat(file, userinfo);	

	fp = fopen(file, "r");
	
	while (fgets(data, sizeof(data), fp) > 0) 
	{					
		memset(buffer, 0, sizeof(buffer));

		pa = data;

    		pb = strchr(data, '@');

    		strncpy(buffer, pa, pb - pa);

		while (buffer[count1] != '\0') count1++;
	        		
		for (i = 0; i <= count1 - count2; i++)
		{

		        for (j = i; j < i + count2; j++)
		        {

		            flag = 1;

		            if (buffer[j] != verify[j - i])
		            {
		                
				flag = 0;
		             
				break;

		            }

		        }

		        if (flag == 1){    

				break;

			}

		}

    		if (flag == 1) {
		
			printf("%s", data);

			fflush(NULL);

			count++;
			
			send(sockfd, data, strlen(data), 0);

			flag = 0;

		}	   

	}
		
	if(count==0) send_data(sockfd, reply[21]);

    	memset(data, 0, sizeof(data));

	memset(verify, 0, sizeof(verify));	

	fclose(fp);

}

