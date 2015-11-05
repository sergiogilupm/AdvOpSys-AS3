/*
* File: server.c
* Author: Sergio Gil Luque
* Version: 1.4
* Date: 11-04-15
*/

#include <stdio.h>
#include <string.h>   
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "uthash.h" // For Hashtable implementation

#define NUM_PEERS 1
#define SERVER_PORT 5000

char *ip;



typedef struct peerStruct {
	int key;
	int peerSock;
	UT_hash_handle hh;
} peerStruct_t;




/* Hash table struct */

typedef struct keyStruct {
	char key[20];
	char value[1000];
	UT_hash_handle hh;
} keyStruct_t;


/* This is a double level hash table. First level -> Files ; Second level -> Peers that contain that file */
typedef struct fileStruct {
  char name[50];
  struct fileStruct *sub;
  int val;
  UT_hash_handle hh;
} fileStruct_t;


keyStruct_t *registeredKeys = NULL;
peerStruct_t *registeredPeers = NULL;



/* Look for peer in the hashtable */
int getPeerSock (int key) 
{ 
    	struct peerStruct *s;
    	HASH_FIND_INT (registeredPeers, &key, s);  /* s: output pointer */
    
	if (s == NULL)
	{
		return -1;
	}
	else
	{
		return s->peerSock;
	}
}


/* Insert new key in hash table  */
int insertPeerSock (int key, int peerSock)
{
	printf("Registering key %i with value %i\n", key, peerSock);

	struct peerStruct *newValue = malloc (sizeof (struct peerStruct));
	newValue -> key = key;
	newValue -> peerSock = peerSock;
	HASH_ADD_INT (registeredPeers, key, newValue);	

	return 0;
}


/* Look for key in the hashtable */
char *getValue (char* key) 
	{ 

    	struct keyStruct *s;
    	HASH_FIND_STR (registeredKeys, key, s );  /* s: output pointer */
    
	if (s == NULL)
	{
		return NULL;
	}
	else
	{
		return s->value;
	}
}

/* Insert new key in hash table  */
int insertValue (char* key, char* value)
{
	printf("Registering key %s with value %s\n", key, value);

	if (getValue(key) != NULL)
	{
		printf("*Error: Key %s has been already registered and it must remain unique\n", key);
		return -1;
	}
	struct keyStruct *newValue = malloc (sizeof (struct keyStruct));
	strncpy(newValue -> key, key, strlen(key));
	strncpy(newValue -> value, value, strlen(value));
	HASH_ADD_STR(registeredKeys, key, newValue);

	return 0;
}


/* Deletes a key from the hash table*/
int deleteKey (char* key) 
{
    	struct keyStruct *s;

    	HASH_FIND_STR (registeredKeys, key, s );  /* s: output pointer */
    
	if (s == NULL)
	{
		return -1;
	}
	else
	{
		HASH_DEL(registeredKeys, s);
		free(s);
		return 0;
	}
}

/* Hash function: Using algorithm djb2 with magic number 33 */
int getIndex (char *fileName) 
{

        unsigned long hash = 5381;
        int c;

        while (c = *fileName++)
	{
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}


        return hash % NUM_PEERS;

}

int putCall (char *fileName, char *port, int index)
{
	char *request = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	int bufferPointer = 0;
	int i;
	int sock;
	char *replyHeader;


	/* Preparing the Put call request */
	strncpy(request, "PUT ", 4);
	strncat(request, fileName, strlen(fileName));
	strncat(request, " ", 1);
	strncat(request, port, strlen(port));
	bufferPointer = 4 + strlen(fileName) + 1 + strlen(port);

	for (i = bufferPointer; i < 1024; i++)
	{
		strncat(request, " ", 1);
	}

	printf("Sending request to server/peer: %s\n", request);
	sock = getPeerSock(index);


	write(sock, request, 1024);
	read (sock, serverReply , 1024);
	printf ("Reply: %s\n", serverReply);
	replyHeader = strtok(serverReply, " ");

	if (strcmp(replyHeader,"OK")==0) 
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int registerCall (char *fileName, char *port, int DHT_index)
{

	int replicaPrev = -1;
	int replicaNext = -1;

	if (DHT_index == 0)
	{
		replicaNext = DHT_index + 1;
		replicaPrev = NUM_PEERS - 1; //Replica can not be in index -1. Go to highest index
	}

	else if (DHT_index == (NUM_PEERS -1))
	{
		replicaNext = 0;
		replicaPrev = DHT_index - 1; //Replica can not be in index -1. Go to highest index
	}

	else
	{
		replicaNext = DHT_index + 1;
		replicaPrev = DHT_index - 1; //Replica can not be in index -1. Go to highest index
	}

	/* Performing put calls to the peers including replicas */
	/* TODO: Check for results. How many peers have to fail in order to consider the call a fail? */
	putCall(fileName, port, DHT_index);
	//putCall(fileName, port, replicaPrev);
	//putCall(fileName, port, replicaNext);

	return 0;
}

char* getCall (char *fileName,  int index)
{
	char *request = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	int bufferPointer = 0;
	int i;
	int sock;
	char *replyHeader;
	char *res;

	/* Preparing the Put call request */
	strncpy(request, "GET ", 4);
	strncat(request, fileName, strlen(fileName));
	bufferPointer = 4 + strlen(fileName);

	for (i = bufferPointer; i < 1024; i++)
	{
		strncat(request, " ", 1);
	}

	printf("Sending request to server/peer: %s\n", request);
	sock = getPeerSock(index);


	write(sock, request, 1024);
	read (sock , serverReply , 1024);
	printf("Get call to peer returned: %s\n", serverReply);
	replyHeader = strtok(serverReply, " ");

	if (strcmp(replyHeader,"OK")==0) 
	{
		res = strtok(NULL, " ");  // the peer ports will be separated by ":"
	}
	else
	{
		res = "ERR";
	}

	return res;
}

char* searchCall (char *fileName, int DHT_index)
{
	char* finalRes;
	int replicaPrev = -1;
	int replicaNext = -1;
	int i = 0;
	char *mainRes;
	char *firstReplicaRes;
	char *secondReplicaRes;

	if (DHT_index == 0)
	{
		replicaNext = DHT_index + 1;
		replicaPrev = NUM_PEERS - 1; //Replica can not be in index -1. Go to highest index
	}

	else if (DHT_index == (NUM_PEERS -1))
	{
		replicaNext = 0;
		replicaPrev = DHT_index - 1; //Replica can not be in index -1. Go to highest index
	}

	else
	{
		replicaNext = DHT_index + 1;
		replicaPrev = DHT_index - 1; //Replica can not be in index -1. Go to highest index
	}

	/* Performing put calls to the peers including replicas */
	/* TODO: Check for results. How many peers have to fail in order to consider the call a fail? */
	
	mainRes = getCall(fileName, DHT_index);
	/*if (strcmp(mainRes,"ERR") == 0) 
	{
		firstReplicaRes = getCall(fileName, replicaPrev);
		if (strcmp(firstReplicaRes,"ERR") == 0) 
		{
			secondReplicaRes = getCall(fileName, replicaNext);
			if (strcmp(secondReplicaRes,"ERR") == 0) 
			{
				finalRes = "ERR";
			}
			else
			{
				finalRes = secondReplicaRes;
			}
		}
		else
		{
			finalRes = firstReplicaRes;
		}
	}
	else
	{
		finalRes = mainRes;
	}*/
	

	/* TODO */
	return mainRes;
}



/* Handles incoming connections */
void *connection_handler(void *socket_desc)
{

   	char *requestReceived = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	char *header;
	char *firstArgv;
	char *secondArgv;
	int DHT_index = -1;
	int bufferPointer;
	char *res;


    	
    	int i;
    	int sock = *(int*)socket_desc;

    	if (read (sock, requestReceived, 1024) == 0)
	{
		printf("*Error: Connection lost\n");
		return (void *) -1;
	}
	
	//printf("Request is: %s\n", requestReceived);
	header = strtok(requestReceived, " ");


	/* Registrer call */
	if (strcmp(header, "REG")==0) 
	{
		firstArgv = strtok(NULL, " ");  // File to register
		secondArgv = strtok(NULL, " "); // Peer port
		DHT_index = getIndex(firstArgv);  // Get hash from file name
		printf("Register call: %s, %s, index %i\n", firstArgv, secondArgv, DHT_index);
		
		if (registerCall(firstArgv, secondArgv, DHT_index) < 0)
		{
			strncpy(serverReply, "ERR", 3);
			bufferPointer = 3;
			
			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}
			
			write(sock, serverReply, 1024);
		}
		else
		{
			strncpy(serverReply, "OK", 2);
			bufferPointer = 2;
			
			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}
			
			write(sock, serverReply, 1024);
		}
	}

	/* Get call */
	else if (strcmp(header,"SEARCH") == 0) 
	{
		firstArgv = strtok (NULL, " "); //File to search
		DHT_index = getIndex(firstArgv); //Get hash from file name

		res = searchCall(firstArgv, DHT_index); 

		if (strcmp(res,"ERR")==0)
		{
			strncpy(serverReply, "ERR", 3);
			bufferPointer = 3;
			
			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}
			
		}
		else
		{
			strncpy(serverReply, "OK", 2);
			strncat(serverReply, " ", 1);
			strncat(serverReply, res, strlen(res));
			bufferPointer = 3 + strlen(res);
			
			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}
		}

		write(sock, serverReply, 1024);
		
	}

	else 
	{
		printf ("*Error: Bad request\n");

		strncpy("ERR ", serverReply, 4);
		write(sock, serverReply, 1024);
	
    		//Free the socket pointer
    		free(socket_desc);
     
    		return (void *) -1;
	}

	close (sock);
	free(socket_desc);
	return (void *) 0;
}



void *incoming_connections_handler (void* data)
{
     
    printf ("\t+Initializing incoming connection handler...\n");
     
    int IN_socket_desc, IN_new_socket, c, *IN_new_sock;
    struct sockaddr_in IN_server , client;
     
    //Create socket
    IN_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (IN_socket_desc == -1)
    {
        perror("*Error creating socket");
    }
     
    //Prepare the sockaddr_in structure
    IN_server.sin_family = AF_INET;
    IN_server.sin_addr.s_addr = INADDR_ANY;
    IN_server.sin_port = htons( SERVER_PORT );
     
    //Bind
    if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
    {
        perror("*Error binding");
        return (void *) 1;
    }
     
    //Listen
    listen(IN_socket_desc , 0); // The server can handle 1000 simulteaneous connections
     
    //Accept an incoming connection
    printf("\t+Waiting for incoming connections in port %i...\n", SERVER_PORT);
    c = sizeof(struct sockaddr_in);

    while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts(">New connection accepted");
         
        pthread_t sniffer_thread;
        IN_new_sock = malloc(1);
        *IN_new_sock = IN_new_socket;
         
        if( pthread_create( &sniffer_thread, NULL,  connection_handler, (void*) IN_new_sock) < 0)
        {
            perror("*Error creating thread for incoming request");
	    free(IN_new_sock);
            return (void *) 1;
        }
    }
     
    if (IN_new_socket<0)
    {
        perror("*Error accepting new request");
        return (void *) 1;
    }
     
    return (void *) 0;
}



int connectToPeer (int port)
{
	int sock;
    	struct sockaddr_in server;
	char *ip = "127.0.0.1";

	//Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);

    	if (sock == -1)
	{
		printf("*Could not create socket");
		return -1;
   	}
	     
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_family = AF_INET;
	server.sin_port = htons( port );
	 
	//Connect to server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("*Connection to server failed");
		return -1;
	}
	else
	{
		return sock;
	}
}




int initDistHashTable(char *configFilePath)
{

	FILE *fp;
	fp = fopen (configFilePath, "r");
	char line[512];
	int count = 0;
	int socket = 0;

    	if (fp != NULL)
    	{
		/*Reads the line that correspond to this server configuration */
		printf ("File %s opened\n", configFilePath);
	
		do
		{
			fgets (line, sizeof(line), fp); /*TODO: Size or length? */
			printf ("Read: %s", line); /* TODO: Check line is int and not something else */

			socket = connectToPeer(atoi(line));

			if (socket < 0)
			{
				printf("*Error: Could not connect to peer at port %i\n", atoi(line));
				return -1;
			}
			insertPeerSock(count, socket);
			count++;
		} 
		while (count < NUM_PEERS);

		fclose(fp);
		
	}

	else
	{
		printf("\n*Error when opening configuration file %s\n", configFilePath);
		perror("");
		return -1;
	}

	return 0;
}


int main(int argc , char *argv[])
{
	int res = 0;
	char *fileName;

	if (argc != 2)
	{
		printf("Usage: %s CONFIG_FILE_PATH\n", argv[0]);
		return 0;
	}

	fileName = argv[1];

	/* INITIALIZATION */
	printf("\n\n-Initializing server... \n");

	// Accessing configuration file and loading internal values
	if (initDistHashTable(fileName) < 0)
	{
		printf ("*Error initializing distributed hash table\n");
		return -1;
	}
	

	printf("\t+Setting up... ");

	/* We create the thread that will handle the incoming connections */
	pthread_t incoming_connections_thread;
	if ( pthread_create (&incoming_connections_thread, NULL, incoming_connections_handler, NULL) < 0)
	{
		printf ("ERROR\n");
		perror("*Error when creating listening thread");
		return 1;
	}
	printf("OK\n");
	printf("\t+Server ready\n");

	//Now join the thread, so that we dont terminate before the thread
	pthread_join (incoming_connections_thread, NULL);
	
	return 0;
}



