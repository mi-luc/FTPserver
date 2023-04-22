#define CHUNK_SIZE 1000
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>  
#include <sys/sendfile.h>
#include <sys/stat.h>
#include<unistd.h>
#include <errno.h>
char delim[]="\0";
char* getToken(char* message,int* messageSize)
{
    int index=0;
    int isTrue=0;
    //printf("-%s",message);
    if(message[index]=='\0')
    return NULL;
    char* token=malloc((strlen(message)+1)*sizeof(char));
    while(isTrue==0)
    {
        if(message[index]=='\0')
        isTrue=1;
        index++;
    }
    if(index==0)
    return NULL;
    strcpy(token,message);
   
    memcpy(message,message+index,*messageSize);
    *messageSize-=index;
    return token;

}
char* receive_basic2(int s,int* size_received)
{
    char chunk[CHUNK_SIZE];
    char* buffer=NULL;
	
	//loop
    int total_size=0;
    int once=1;
    int size_by_now=0;
	int size=recv(s , chunk , CHUNK_SIZE , 0  );

   
    char* token1=getToken(chunk,&size);
    if(strncmp(token1,"1",1)==0)
    printf("SUCCESS!\n");
    if(strncmp(token1,"0",1)==0)
    {
        printf("ERROR NOT FOUND!\n");
        return NULL;
    }
    char*token2=getToken(chunk,&size);
    size=size-strlen(token1)-strlen(token2)+3;
    

    total_size=atoi(token2);

    if(size_received)
        (*size_received)=total_size;

    //printf("size_received:%d\n",total_size);
    //printf("TOKENS:%s-%s siz:%d\n",token1,token2,size);
    //printf("\nchunk:%s",token3);
    
    buffer=malloc((total_size+100)*sizeof(char));
    bzero(buffer,(total_size+100)*sizeof(char));
    
    size_by_now=size;
    if(size_by_now>0)
        memcpy(buffer,chunk,size_by_now);
    

    //printf("size_by_now:%d\n",size_by_now);
    while(size_by_now<total_size)
    {
        //printf("a");
       size=recv(s , chunk , CHUNK_SIZE , 0  );
       if(size>0)
            memcpy(buffer+size_by_now,chunk,size);
       size_by_now+=size;
       
    }
    //printf("out");
    //printf("size_by_now:%d\n\n",size_by_now);
    //buffer[total_size]=0;
    return buffer;
    


}
char* receive_basic(int s,int* size_received)
{
	char chunk[CHUNK_SIZE];
    char delims[]="-";
    char* buffer=NULL;
	
	//loop
    int total_size=0;
    int once=1;
    int size_by_now=0;
	int size=recv(s , chunk , CHUNK_SIZE , 0  );
    char* token1=strtok(chunk,delims);
    char*token2=strtok(NULL,delims);
    total_size=atoi(token1);

    if(size_received)
        (*size_received)=total_size;

    //printf("size_received:%d\n",total_size);
    

    buffer=malloc((total_size+10)*sizeof(char));
  
    size_by_now=size-1-strlen(token1);
    
     if(size_by_now>0)
        memcpy(buffer,token2,size_by_now);
    

    
    while(size_by_now<total_size)
    {
       size=recv(s , chunk , CHUNK_SIZE , 0  );
       if(size>0)
       memcpy(buffer+size_by_now,chunk,size);
       size_by_now+=size;
       
    }
    
    buffer[total_size]=0;
    return buffer;

	
}

char *itoa(int num, char *str)
{
        if(str == NULL)
        {
                return NULL;
        }
        sprintf(str, "%d", num);
        return str;
}
void sendfileOverTcp(int socket,int descriptor,off_t fileSize)
{
    off_t lastWriteLocation=lseek(descriptor,0,SEEK_CUR);
    off_t total_size=0;
    while(total_size<fileSize)
    {
        int sent=sendfile(socket,descriptor,&lastWriteLocation,fileSize-total_size);
        if(sent==-1)
        {
            printf("%d-%s",errno,strerror(errno));
        }
        total_size+=sent; 
        
        printf("%d-%d\n",total_size,fileSize);
            
    }
}
void get(int socket,int descriptor)
{
    
    int sizeReceived;
    char* fileContent=receive_basic(socket,&sizeReceived);
    int sizeWritten=0;
    while(sizeWritten<sizeReceived)
    {
        int size=write(descriptor,fileContent+sizeWritten,sizeReceived-sizeWritten);
        sizeWritten+=size;
    }
    free(fileContent);
    

}

int main()
{
char buffer[10000];
int socketDescriptor;
struct sockaddr_in serverAddr;
socketDescriptor = socket (AF_INET, SOCK_STREAM, 0);

bzero (&serverAddr, sizeof (serverAddr));
serverAddr.sin_family = AF_INET;
serverAddr.sin_addr.s_addr = inet_addr("10.0.2.15"); 
serverAddr.sin_port = htons (3555);

int r=connect (socketDescriptor, (struct sockaddr *) &serverAddr,sizeof(serverAddr));

if(r==-1)
{
    printf("%d-%s",errno, strerror(errno));
   
    return 1;
}

while(1)
{
    printf("\n>");
    char delims[]=" \n";
    int size=10000;
    bzero(buffer,10000);
    fgets(buffer,10000,stdin);
   
   

    buffer[strlen(buffer)-1]=0;

   


    char numberTochar[200];
    itoa(strlen(buffer),numberTochar);
    strcat(numberTochar,"-");
    strcat(numberTochar,buffer);
    send(socketDescriptor,numberTochar,strlen(numberTochar),0);
   
    char* token1=strtok(buffer,delims);
    if(strncmp(token1,"bye",3)==0)
    {
        break;
    }
    if(strncmp(token1,"search",6)==0)
    {
        int size;
        char* code=receive_basic2(socketDescriptor,&size);
        if(code==NULL)
        {
            printf("Cuvantul cautat nu a fost gasit...\n");
            continue;
        }
        char* token=getToken(code,&size);
        int index=1;
        printf("Lista fisiere in care se regaseste cuvantul:\n");
        while(token!=NULL)
        {
            printf("%d.%s\n",index,token);
            index++;
            free(token);
            token=getToken(code,&size);
        }
        free(code);
    }
    if(strncmp(token1,"update",6)==0)
    {
        char buff[10];
       recv(socketDescriptor,buff,1,0);
        if(strncmp(buff,"0",1)==0)
        {
            printf("Operatia nu a reusit!\n");
        }
         if(strncmp(buff,"1",1)==0)
        {
            printf("Operatie incheiata cu succes!\n");
        }

    }
    if(strncmp(token1,"list",4)==0)
    {
        int size;
        
        char* code=receive_basic2(socketDescriptor,&size);
        //printf("%d",size);
        if(code==NULL)
        continue;
        char* token=getToken(code,&size);
        int index=1;
        while(token!=NULL)
        {
            printf("%d.%s\n",index,token);
            index++;
            free(token);
            token=getToken(code,&size);
        }
        free(code);
    }
     if(strncmp(token1,"get",3)==0)
    {

    int descriptor;
    char* token2=strtok(NULL,delims);
    descriptor=open(token2, O_WRONLY | O_APPEND | O_CREAT, 0666);
    get(socketDescriptor,descriptor);

    }
    if(strncmp(token1,"put",3)==0)
    {
        //sleep(1);
        int descriptor;
        char* token2=strtok(NULL,delims);
        struct stat st;
        stat(token2, &st);
        off_t size = st.st_size;
        descriptor=open(token2,O_RDWR);
        printf("%d\n",descriptor);
        char sizeTochar[20];
        itoa(size,sizeTochar);
        send(socketDescriptor,sizeTochar,strlen(sizeTochar),0);
        send(socketDescriptor,"-",1,0);
        sendfileOverTcp(socketDescriptor,descriptor,size);
        char buff[10];
        recv(socketDescriptor,buff,1,0);
        if(strncmp(buff,"1",1)==0)
        {
            printf("Operatie incheiata cu succes!");
        }
    }
    if(strncmp(token1,"update",6)==0)
    {
        char* token2=strtok(NULL,delims);
        char* token3=strtok(NULL,delims);
        char* token4=strtok(NULL,delims);
        if(token4==NULL)
            printf("Eroare la scrierea comenzii update...\n");
        

    }
    if(strncmp(token1,"delete",6)==0)
    {
        char buff[10];
        recv(socketDescriptor,buff,1,0);
        if(strncmp(buff,"1",1)==0)
        {
            printf("Fisier sters cu succes de pe serverul de FTP!\n\n");
        }
         if(strncmp(buff,"0",1)==0)
        {
            printf("Eroare la stergerea fisierului din server...");
            
        }
    }

   
    
   
}

shutdown(socketDescriptor,SHUT_RDWR);
close (socketDescriptor);
}
