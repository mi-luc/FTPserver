#define _GNU_SOURCE
#define PORT 3555
#define NR_CONNECTIONS 5
#define IP_ADDRESS "10.0.2.15"
#define PATH "./DIR"
#define CHUNK_SIZE 1000
#define _OPEN_THREADS
#define TRUE 1
#define FALSE 0
#define SEM_MAX 10
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>  
#include <sys/sendfile.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>


//MESAJE
char stergeFisier[300]="S-a sters fisierul:";
char adaugareFisier[300]="S-a adaugat un fisier!";
char cautare[300]="S-a cautat cuvantul:";

struct parameters
{
    int socketId;
    int myIndex;
};
struct frequentWords
{
    char wordsList[10][100];
    int frequency[10];
    int number;
};
struct file
{
    char* filename;
    int reading;
    pthread_cond_t cond ;
    pthread_mutex_t writing;
    struct frequentWords cuvinteFrecvente;
};

struct lsFiles
{
    struct file listOfFiles[100];
    int size;
};

struct tidsAndMutex
{
    pthread_t tid;
    pthread_mutex_t mutexThread;
};
typedef struct parameters parameters;
typedef struct lsFiles lsFiles;


lsFiles globalFilesList;
pthread_mutex_t fileListinUseMutex;
char delim[]="\0";
char filenameOperation[10][100];
pthread_mutex_t indexLock;
pthread_cond_t  indexCond;
unsigned count=0;

//LOG THREAD
char logBuffer[1000]="\0";
int hasToWrite=0;
pthread_mutex_t writing;
pthread_mutex_t readingMutex;
pthread_cond_t conditionToWrite = PTHREAD_COND_INITIALIZER;

//TLS
__thread int socketDescriptor;
__thread char* filenameThread;

//lista tiduri
struct tidsAndMutex tidsList[20];
int tidsSize=0;

struct file* getFileEntry(char* filename)
{
int index=-1;
    for(int i=0;i<globalFilesList.size;i++)
    {
        if(strcmp(filename,globalFilesList.listOfFiles[i].filename)==0)
        {
            index=i;
            break;
        }
    }
     if(index==-1)
    return NULL;
    return &(globalFilesList.listOfFiles[index]);

}
void addFileEntry(char* filename,lsFiles* list,int global)
{
    if(global==TRUE)
        pthread_mutex_lock(&fileListinUseMutex);
   
    struct file* lastEntry=&list->listOfFiles[list->size];
    lastEntry->filename=malloc((strlen(filename)+1)*sizeof(char));
    strcpy(lastEntry->filename,filename);
   
    lastEntry->reading=0;
    pthread_mutex_unlock(&lastEntry->writing);
    pthread_cond_init(&lastEntry->cond,NULL);
    (*list).size++;
    if(global==TRUE)
        pthread_mutex_unlock(&fileListinUseMutex);
}

void deleteFileEntry(char* filename,lsFiles* list,int global)
{
    if(global==TRUE)
        pthread_mutex_lock(&fileListinUseMutex);
    int index=-1;
    for(int i=0;i<(*list).size;i++)
    {
        if(strcmp(filename,(*list).listOfFiles[i].filename)==0)
        {
            index=i;
            break;
        }
    }
   
    if(index==-1)
    return;
    //asteapta terminarea operatiilor de update/write
    pthread_mutex_lock(&list->listOfFiles[index].writing);
   for (int i = index; i < list->size; i++)
    {
        list->listOfFiles[i] = list->listOfFiles[i + 1];
    }
    (*list).size--;
    if(global==TRUE)
        pthread_mutex_unlock(&fileListinUseMutex);
}

void printFiles()
{
    for(int i=0;i<globalFilesList.size;i++)
    {
        printf("%d.%s\n",i,globalFilesList.listOfFiles[i].filename);
    }
}

void gracefullTermination(int signum)
{
   
   for(int i=0;i<tidsSize;i++)
   {
    pthread_mutex_lock(&tidsList[i].mutexThread);
    pthread_cancel(tidsList[i].tid);
    pthread_mutex_unlock(&tidsList[i].mutexThread);
   }
   printf("Gracefull termination succeded!");
   exit(0);
}

int getTotalSize(struct lsFiles files)
{
    int sum=0;
    for(int i=0;i<files.size;i++)
    {
        
        sum+=strlen(files.listOfFiles[i].filename);
    }
    return sum;
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

char* addTokenToMessage(char* message, char* token, int* messageSize)
{
    int index=0;
    int isTrue=0;
    while(isTrue==0)
    {
        if(message[index]=='\0' && message[index+1]=='\0')
        isTrue=1;
        if(index>100000)
        return NULL;
        index++;
    }   
    index--;
    *messageSize=index+strlen(token)+1;
    if(index==0)
    memcpy(message+index,token,strlen(token));
    else
    memcpy(message+index+1,token,strlen(token));

    return message;

}

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

char* makeOutMessage(int status,char* message,int* Msize,struct lsFiles* filesList)
{
     char * outMessage;
   if(message!=NULL)
   {
     outMessage= malloc(100*sizeof(char)+strlen(message)*sizeof(char));
    memset(outMessage, 0, 100 * sizeof(char) + strlen(message) * sizeof(char));
   }
    if(filesList!=NULL)
    {
    int totalSize=getTotalSize(*filesList);
    outMessage= malloc((totalSize+100)*sizeof(char));
    memset(outMessage, 0, (totalSize+100)*sizeof(char));

    }

    if(status==TRUE)
    {
       
        addTokenToMessage(outMessage,"1",Msize);
        
        if(message!=NULL && filesList==NULL)
        {
        char size[50];
        itoa(strlen(message),size);
        
        addTokenToMessage(outMessage,size,Msize);
        addTokenToMessage(outMessage,message,Msize);
        
        }
        if(filesList!=NULL)
        {
        
        int totalSize=getTotalSize(*filesList);
       
        char size[50];
        itoa(totalSize,size);
        addTokenToMessage(outMessage,size,Msize);
        for(int i=0;i<(*filesList).size;i++)
        {
           
            addTokenToMessage(outMessage,(*filesList).listOfFiles[i].filename,Msize);
        }
        }
        return outMessage;
    }
    if(status==FALSE)
    {
       
        addTokenToMessage(outMessage,"0",Msize);
        if(message!=NULL && filesList==NULL)
        {
        char size[50];
        itoa(strlen(message),size);

        addTokenToMessage(outMessage,size,Msize);
        addTokenToMessage(outMessage,message,Msize);
        }
       
    
    if(filesList!=NULL)
        {
            int totalSize=getTotalSize(*filesList);
             char size[50];
            itoa(totalSize,size);
            addTokenToMessage(outMessage,size,Msize);
            for(int i=0;i<(*filesList).size;i++)
                addTokenToMessage(outMessage,(*filesList).listOfFiles[i].filename,Msize);
    
        }
    return outMessage;
    }
    return NULL;

}

lsFiles _ls(const char *dir)
{
	struct dirent *d;
    int index=0;
    lsFiles listOfFileNames;
	DIR *dh = opendir(dir);
	if (!dh)
	{
		if (errno = ENOENT)
		{
			
			perror("Directory doesn't exist");
		}
		else
		{
			
			perror("Unable to read directory");
		}
		exit(0);
	}
    while ((d = readdir(dh)) != NULL)
	{
        index++;
    }
    index-=2;
   
    if(index<0)
    listOfFileNames.size=0;
    else
    listOfFileNames.size=index;
   
    index=0;
    closedir(dh);
    dh = opendir(dir);
    
	while ((d = readdir(dh)) != NULL)
	{
        
        char* filename=d->d_name;
        
        if(strcmp(filename,"..")==0)
        continue;
        if(strcmp(filename,".")==0)
        continue;
       
        int nameLen=strlen(filename)+1;
        listOfFileNames.listOfFiles[index].filename=malloc(nameLen*sizeof(char));
        memcpy(listOfFileNames.listOfFiles[index].filename,filename,nameLen);
        listOfFileNames.listOfFiles[index].reading=FALSE;
        pthread_mutex_unlock(&listOfFileNames.listOfFiles[index].writing);
		
        index++;
	}
    closedir(dh);
    
	
	return listOfFileNames;
}

char* readFile(char* filename)
{

    
    char * buffer = 0;
    long length;

    char beggining[250]="./DIR/";
    strcat(beggining,filename);

    FILE * f = fopen (beggining, "rb");
    if(f==NULL)
{
    printf("Eroare la deshiderea fisierului: %s",beggining);
    return NULL;
}

if (f)
{
  fseek (f, 0, SEEK_END);
  length = ftell (f);
  fseek (f, 0, SEEK_SET);
  buffer = malloc (length);

  if (buffer)
  {
    fread (buffer, 1, length, f);
  }
  fclose (f);
}
return buffer;
}

char* receive_basic(int* size_received)
{
	char chunk[CHUNK_SIZE];
    char delims[]="-";
    char* buffer=NULL;
	char* save,*save2;
	//loop
    int total_size=0;
    int once=1;
    int size_by_now=0;
	int size=recv(socketDescriptor , chunk , CHUNK_SIZE , 0  );
    char* token1=strtok_r(chunk,delims,&save);
    if(token1==NULL)
    {
        printf("HUGE MISTAKE");
    }
    char*token2=strtok_r(save,delims,&save2);
    total_size=atoi(token1);

    if(size_received)
        (*size_received)=total_size;

    //printf("size_received:%d\n",total_size);
   
    buffer=malloc((total_size+10)*sizeof(char));
  
    size_by_now=size-1-strlen(token1);
    
     if(size_by_now>0)
        memcpy(buffer,token2,size_by_now);
    

    //printf("size_by_now:%d\n",size_by_now);
    while(size_by_now<total_size)
    {
       size=recv(socketDescriptor , chunk , CHUNK_SIZE , 0  );
       memcpy(buffer+size_by_now,chunk,size);
       size_by_now+=size;
       
    }
    //printf("size_by_now:%d\n\n",size_by_now);
    buffer[total_size]=0;
    return buffer;

	
}
void list()
{
    
    writeMessage("Clientul cere list!",NULL);
    int size;
    
    pthread_mutex_lock(&fileListinUseMutex);
    int status=TRUE;
    if(globalFilesList.size==0)
    status=FALSE;
    char* buff=makeOutMessage(status,NULL,&size,&globalFilesList);
    pthread_mutex_unlock(&fileListinUseMutex);

    send(socketDescriptor,buff,size,0);
    free(buff);

}

void put(int descriptor)
{
    int sizeReceived;
    char* fileContent=receive_basic(&sizeReceived);
    int sizeWritten=0;
    while(sizeWritten<sizeReceived)
    {
        int size=write(descriptor,fileContent+sizeWritten,sizeReceived-sizeWritten);
        sizeWritten+=size;
    }
    send(socketDescriptor,"1",1,0);
    //writeMessage("S-a adaugat un fisier cu succes!");
   
    pthread_mutex_lock(&indexLock);
    
    strcpy(filenameOperation[count],filenameThread);
   
    count++;
    pthread_cond_signal(&indexCond);
    pthread_mutex_unlock(&indexLock);
    free(fileContent);

    writeMessage(adaugareFisier,filenameThread);

}
void delete()
{
    char beggining[250]="./DIR/";
    strcat(beggining,filenameThread);
    
    int success=remove(beggining);
    if(success==-1)
    {
         send(socketDescriptor,"0",1,0);
    }
    else
    {
         writeMessage("S-a sters un fisier cu succes!",filenameThread);
         send(socketDescriptor,"1",1,0);
    }
   
    return;
}
void update( off_t offset, char* string)
{

    char beggining[250]="./DIR/";
    strcat(beggining,filenameThread);
    int fd=open(beggining,O_WRONLY);
    if(fd==-1)
    {
        writeMessage("Deschidere fisier a esuat",filenameThread);
        send(socketDescriptor,"0",1,0);
        return;
    }
    int total_size=strlen(string);
    int written=0;
    lseek(fd,offset,SEEK_CUR);
    while(written<total_size)
    {
        int size=write(fd,string+written,total_size-written);
        written+=size;
    }
    //writeMessage("S-a updatat un fisier cu succes!");
    send(socketDescriptor,"1",1,0);
    close(fd);

    pthread_mutex_lock(&indexLock);
    strcpy(filenameOperation[count],filenameThread);
   
    count++;
    pthread_cond_signal(&indexCond);
    pthread_mutex_unlock(&indexLock);

}

void sendfileOverTcp(int descriptor,off_t fileSize)
{
    off_t lastWriteLocation=lseek(descriptor,0,SEEK_CUR);
    off_t total_size=0;
    while(total_size<fileSize)
    {
        int sent=sendfile(socketDescriptor,descriptor,&lastWriteLocation,fileSize-total_size);
        if(sent==-1)
        {
            printf("%d-%s",errno,strerror(errno));
        }
        total_size+=sent; 
        
            
    }
}
void get()
{
            int descriptor;
            char beggining[250]="./DIR/";

            strcat(beggining,filenameThread);
            struct stat st;
            stat(beggining, &st);
            off_t size = st.st_size;
            descriptor=open(beggining,O_RDWR);
           
            char sizeTochar[20];
            itoa(size,sizeTochar);
            strcat(sizeTochar,"-");
            send(socketDescriptor,sizeTochar,strlen(sizeTochar),0);
            sendfileOverTcp(descriptor,size);
}
void search(char* word)
{
    lsFiles list;
    list.size=0;
    int size;
    int status=FALSE;
    for(int i=0;i<globalFilesList.size;i++)
    {
        struct file* currentFile=&globalFilesList.listOfFiles[i];
        for(int j=0;j<currentFile->cuvinteFrecvente.number;j++)
        {
            if(strcmp(currentFile->cuvinteFrecvente.wordsList[j],word)==0)
            {
              
                status=TRUE;
                addFileEntry(currentFile->filename,&list,FALSE);
            }
        }
    }

    char* buff=makeOutMessage(status,NULL,&size,&list);
    
    send(socketDescriptor,buff,size,0);
    free(buff);
    if(status==TRUE)
    writeMessage("S-a cautat un cuvant! SUCCES",word);
    else
    writeMessage("S-a cautat un cuvant! EROARE",word);
    
}

void* communicate_function(void* params)
{
   
   
    char* buffer;
    char delims[]=" \n";
    struct parameters* func_param=(struct parameters*)params;
    socketDescriptor=func_param->socketId;
    
    while(1)
    {
        buffer=receive_basic(NULL);
        printf("%s\n",buffer);
        char * save,*save2,*save3,*save4;
        char* token1=strtok_r(buffer,delims,&save);
        pthread_mutex_lock(&tidsList[func_param->myIndex].mutexThread);
        if(strncmp(token1,"bye",3)==0)
        {
            
            close(socketDescriptor);
            break;
        }
        if(strncmp(token1,"search",6)==0)
        {
            char* token2=strtok_r(save,delims,&save2);
            search(token2);
            
        }

        if(strncmp(token1,"list",4)==0)
        {   
            list();
            
        }
        if(strcmp(token1,"get")==0)
        {
            filenameThread=strtok_r(save,delims,&save2);
            get();
        }
        if(strcmp(token1,"put")==0)
        {

            int descriptor;
            filenameThread=strtok_r(save,delims,&save2);
            char beggining[250]="./DIR/";
            strcat(beggining,filenameThread);
            descriptor=open(beggining, O_WRONLY | O_APPEND | O_CREAT, 0666);
            addFileEntry(filenameThread,&globalFilesList,TRUE);
            put(descriptor);
            
            
        }
        if(strcmp(token1,"update")==0)
        {
            //update filename 100 alabalaportocala
            //t1      t2       t3     t4
            //RECEIVE PLACE AND BUFFER
            filenameThread=strtok_r(save,delims,&save2);
            char* byteoffset=strtok_r(save2,delims,&save3);
            char* string=save3+strlen(byteoffset)-1;
            
            int offset=atoi(byteoffset);
            if(offset<0 || string== NULL)
            {
                send(socketDescriptor,"0",1,0);
                
            }
            else
            update(offset,string);
            
            
        }
         if(strcmp(token1,"delete")==0)
        {
            filenameThread=strtok_r(save,delims,&save2);
              
            delete();
            deleteFileEntry(filenameThread,&globalFilesList,TRUE);
           
        }
        free(buffer);
        pthread_mutex_unlock(&tidsList[func_param->myIndex].mutexThread);
    }
   return NULL;
}

void writeMessage(char* msg,char*filename)
{
   
    pthread_mutex_lock(&readingMutex);
     
    time_t t=time(NULL);
    struct tm tm = *localtime(&t);
    char* year,hour,min;
    do
    {
        if(logBuffer[0]=='\0')
        break;
    } while (TRUE);
    if(filename!=NULL)
    sprintf(logBuffer,"%d-%02d-%02d , %02d:%02d , %s, %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour,tm.tm_min,msg,filename);
    else
    sprintf(logBuffer,"%d-%02d-%02d , %02d:%02d , %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour,tm.tm_min,msg);
    hasToWrite++;
    pthread_cond_signal(&conditionToWrite);
    pthread_mutex_unlock(&readingMutex);
   
}
void serverWriter(void* params)
{
   
    struct parameters* func_param=(struct parameters*)params;
   

    int fd=open("log.txt",O_WRONLY|O_APPEND);
    while(TRUE)
    {
    pthread_mutex_lock(&readingMutex);
    
    while(hasToWrite==0)
    {
        pthread_cond_wait(&conditionToWrite,&readingMutex);
    }
    hasToWrite--;
    
    
    int total_size=strlen(logBuffer);
    int written=0;
    pthread_mutex_lock(&tidsList[func_param->myIndex].mutexThread);
    while(written<total_size)
    {
        int size=write(fd,logBuffer+written,total_size-written);
        written+=size;
    }
    logBuffer[0]='\0';
    pthread_mutex_unlock(&readingMutex);
    pthread_mutex_unlock(&tidsList[func_param->myIndex].mutexThread);
    }
    close(fd);
    
}
//Functie luata de pe internet
int FindFrequency(char* str, char* word)
{
    int len = 0;
    int wlen = 0;
    int cnt = 0;
    int flg = 0;

    int i = 0;
    int j = 0;

    len = strlen(str);
    wlen = strlen(word);

    for (i = 0; i <= len - wlen; i++) {

        flg = 1;
        for (j = 0; j < wlen; j++) {
            if (str[i + j] != word[j]) {
                flg = 0;
                break;
            }
        }

        if (flg == 1)
            cnt++;
    }

    return cnt;
}
//

void getMostFrequentInFile(struct file* fileEntry)
{
    int indexes[10];
    char delimit[] = " \r\n\t.,?!-";
    char* content = readFile(fileEntry->filename);
    char* token;
    char* saveptr;
    token = strtok_r(content, delimit,&saveptr);
    int size = 0;
    while (token != NULL)
    {
        size++;
        token = strtok_r(NULL, delimit,&saveptr);
    }
    content = readFile(fileEntry->filename);
    char* fileCopy=(char*)malloc((strlen(content)+1)*sizeof(char));
    memcpy(fileCopy, content, strlen(content) + 1);

    token = strtok_r(content, delimit,&saveptr);
    char** listWords =(char**) malloc((size + 1) * sizeof(char*));
    int* frequencyList = (int*)malloc((size + 1) * sizeof(int));
    int index = 0;
    while (token != NULL)
    {
        listWords[index] = (char*)malloc((strlen(token) + 1) * sizeof(char));
        strcpy(listWords[index],token);
        frequencyList[index] = FindFrequency(fileCopy, token);
        token = strtok_r(NULL, delimit,&saveptr);
        index++;
    }
   

    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j < size; j++) {
            if (strcmp(listWords[i],listWords[j])==0) {
                for (int k = j; k <= size; k++) {
                    listWords [k] = listWords [k + 1];
                    frequencyList[k] = frequencyList[k + 1];
                }
                j--;
                size--;
            }
        }
    }

    for (int c = 0; c < size - 1; c++)
    {
        for (int d = 0; d < size - c - 1; d++)
        {
            if (frequencyList[d] < frequencyList[d + 1])
            {
                int swap = frequencyList[d];
                frequencyList[d] = frequencyList[d + 1];
                frequencyList[d + 1] = swap;

                char* swapC = listWords[d];
                listWords[d] = listWords[d + 1];
                listWords[d + 1] = swapC;
            }
        }
    }
    if (size < 10)
    {
        fileEntry->cuvinteFrecvente.number = size;
        for (int i = 0; i < size; i++)
        {
            strcpy(fileEntry->cuvinteFrecvente.wordsList[i],listWords[i]);
        }

    }
    else
    {
        fileEntry->cuvinteFrecvente.number = 10;
        for (int i = 0; i < 10; i++)
        {
            strcpy(fileEntry->cuvinteFrecvente.wordsList[i], listWords[i]);
        }
    }


}

pthread_mutex_t startServer;
pthread_cond_t startServercond;
int countToStart=0;
void* indexThread(void* params)
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    struct parameters* func_param=(struct parameters*)params;
    
    pthread_mutex_lock(&startServer);
    for(int i=0;i<globalFilesList.size;i++)
    {
        getMostFrequentInFile(&globalFilesList.listOfFiles[i]);
    }
    countToStart++;
    pthread_cond_signal(&startServercond);
    pthread_mutex_unlock(&startServer);
    
   

    while(TRUE)
    {

        pthread_mutex_lock(&indexLock);
        while(count==0)
            pthread_cond_wait(&indexCond,&indexLock);
        count--;
        struct file* fila=getFileEntry(filenameOperation[count]);
        pthread_mutex_lock(&tidsList[func_param->myIndex].mutexThread);
        getMostFrequentInFile(fila);
        pthread_mutex_unlock(&tidsList[func_param->myIndex].mutexThread);
        pthread_mutex_unlock(&indexLock);
        
      
    }

   
}
int main()
{
    globalFilesList=_ls("./DIR");

    strcpy(globalFilesList.listOfFiles[1].cuvinteFrecvente.wordsList[1],"ceva");

   

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_RESETHAND;
	sa.sa_handler = gracefullTermination;
	sigaction(SIGINT, &sa, NULL);

    pthread_t writer_TID;
    struct parameters t1;
    t1.myIndex=tidsSize;
    pthread_create(&writer_TID,NULL,serverWriter,&t1);

   
    tidsList[tidsSize].tid=writer_TID;
    tidsSize++;
  

    pthread_t index_TID;
    struct parameters t2;
    t2.myIndex=tidsSize;
    pthread_create(&index_TID,NULL,indexThread,&t2);

   
    tidsList[tidsSize].tid=index_TID;
    tidsSize++;
    
   
    int listenSocket, connectSocket;
    struct sockaddr_in serverAddr, clientAddr;
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(IP_ADDRESS); 
    serverAddr.sin_port = htons(PORT);

    bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, NR_CONNECTIONS);

    pthread_mutex_lock(&startServer);
    while(countToStart==0)
    pthread_cond_wait(&startServercond,&startServer);
    countToStart--;
    pthread_mutex_unlock(&startServer);
    
 
     
    writeMessage("Server has started!",NULL);


   
    while (1) {
      
        int clientAddrLength = sizeof(clientAddr);
        connectSocket = accept(listenSocket, (struct sockaddr*)&clientAddr,&clientAddrLength);
        pthread_t tid;
        struct parameters alabala;
        alabala.socketId=connectSocket;
        alabala.myIndex=tidsSize;
        
        if(connectSocket==-1)
        {
            printf("Eroare la crearea conexiunii!\n");
            exit(1);
        }
        writeMessage("Noua conexiune cu un client-> porneste un nou thread!",NULL);
       

        pthread_create(&tid,NULL,communicate_function,&alabala);

        
        tidsList[tidsSize].tid=tid;
        tidsSize++;
       
      
    }
    shutdown(listenSocket,SHUT_RDWR);
    

    return 0;
}