/*
* File: peer.c
* Author: Sergio Gil Luque
* Version: 1.0
* Date: 10-28-15
*/

#include<stdlib.h>
#include<stdio.h>
#include<string.h>  
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<time.h>
#include<fcntl.h> 
#include<unistd.h> 
#include<pthread.h>

#include "uthash.h" // For Hashtable implementation

#define SERVER_PORT 5000
#define PEER_PORT 6001

/* Prototypes */
int sendFile (int sock, char* fileName);
int obtainFile (int port, char* fileName);
void showPromptMessage (int option);

/* Global variables */ /* TODO: Check the variables*/
int connected = 0;


int serverPort = 0;
char *peerID;
int testingMode = 0;
int seqRuns = 1;
int peerPort = 0;
int globalOpt = 1;




/* Hash table */

/* This is a double level hash table. First level -> Files ; Second level -> Peers that contain that file */
typedef struct fileStruct {
	char name[50];
	struct fileStruct *sub;
	UT_hash_handle hh;
} fileStruct_t;

fileStruct_t *registeredFiles = NULL;

/* Look for file in the hashtable */
struct fileStruct *find_file(char* fileName) {

    struct fileStruct *s;

    HASH_FIND_STR (registeredFiles, fileName, s );  /* s: output pointer */
    return s;
}

/*Look for the peer provided inside a file*/
struct fileStruct *findPeerInFile(char* peerPort, char* fileName) {

    	struct fileStruct *s;
    	struct fileStruct *i;

    	HASH_FIND_STR (registeredFiles, fileName, s );  /* s: pointer to file struct */

	for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
	{
		if (strcmp(i->name, peerPort)==0)
		{
			printf("Peer %s already registered file %s. Skipping...\n", peerPort, fileName);
			return i;
		}
	}

    	return NULL;
}


/* Look if a file is registered */
char* searchFileInDHT (char *fileName)
{
	struct fileStruct *s;
	struct fileStruct *i;
	char *reply = malloc(1024 * sizeof(char));
	char valStr[10];
	s = find_file(fileName);

	if (s == NULL)
	{
		printf ("ERROR: FILE NOT FOUND");
		strncpy(reply, "ERR", 3);
	}

	else
	{
		strncpy(reply, "OK", 2);
		strncat(reply, " ", 1);
		for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
		{
			strncat(reply, ":", 1);	
			strncat(reply, i->name, sizeof(i->name));
		

			/*snprintf(valStr, sizeof(i->val) + 1, "%i", i->val);
			strncat(reply, " ", 1);
			strncat(reply, i->name, sizeof(i->name));
			strncat(reply, ":", 1);
			strncat(reply, valStr, sizeof(valStr) + 1);
			strncat(reply, " ", 1);*/
		}
		strncat(reply, " ", 1);
		printf("Returning from DHT table: %s\n", reply);
		return reply;
	}	
	return reply;
}

/* Only for debugging purposes */
void print_files() 
{
	struct fileStruct *s;
	struct fileStruct *i;

	for(s=registeredFiles; s != NULL; s=(struct fileStruct*)(s->hh.next)) 
	{
		for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
		{
	        	printf("file %s: peer at port %s\n", s->name, i->name);
		}
	}
}


/* Add a file into the hashtable. If the file already exists, it will only add the peer in the second level hashtable */
int addFile (char* newFileName, char *newPort) {

   	// Find file in hashTable. If it doesnt exist, create new entry and go deeper. If exists, just go deeper
    	struct fileStruct *aux;
	struct fileStruct *aux2;

    	aux = find_file(newFileName); //Checking if the file already exists in the hashtable
	if (aux == NULL)
	{
		/* Adding file in first level of the hash table */
		aux = malloc(sizeof(struct fileStruct));
    		strcpy(aux->name, newFileName);
    		aux->sub = NULL;
		//aux->val = 0;
    		HASH_ADD_STR( registeredFiles, name, aux );

		/* Adding peer in second level of hashtable */
		fileStruct_t *s = malloc(sizeof(*s));
		strcpy(s->name, newPort);
		s->sub = NULL;
		//s->val = peerPort;
		HASH_ADD_STR(aux->sub, name, s);
	}
	else
	{
		aux2 = findPeerInFile(newPort, newFileName); //Checking if the peer has already registered that file before
		if (aux2 == NULL)
		{
			fileStruct_t *s = malloc(sizeof(*s));
			strcpy(s->name, newPort);
			s->sub = NULL;
			//s->val = peerPort;
			HASH_ADD_STR(aux->sub, name, s);
		}
	}

	return 0;
}





/*
 * This function will handle the connection for each request
 */

/*void *connection_handler(void *socket_desc)
{
	char request_received[2000];
	int sock = *(int*)socket_desc;
	char *message;
	char *serverReply;
	     
	read (sock , request_received , 255);
	printf ("+A request has been received: %s\n", request_received);

	message = strtok(request_received ," ");

	//The only call that the peer can recieve is an obtain() call. Otherwise error

	if (strcmp(message,"OBTAIN")==0) 
	{	
		printf("+Obtain file request\n");
		char *fileName = strtok(NULL, " "); //Getting fileName from the request

    		if (sendFile(sock, fileName) < 0)
		{
			printf("*File %s could not be obtained\n", fileName);
		}
		else
		{
			printf("+File %s obtained successfully\n", fileName);
		}
		//showPromptMessage(globalOpt);
	}

	else
	{
		printf ("*Error: Bad request recieved\n");
		serverReply = "ERROR BAD_REQUEST";
		write(sock, serverReply, sizeof(serverReply));
	}

    //Free the socket pointer
    free(socket_desc);
    showPromptMessage(globalOpt);
    return (void *) 0;
}*/


/*void *incoming_connections_handler (void* data)
{
    //printf ("+Initializing incoming connection handler...\n");
     
    int IN_socket_desc , IN_new_socket , c , *IN_new_sock;
    struct sockaddr_in IN_server , client;
     
    //Create socket
    IN_socket_desc = socket(AF_INET , SOCK_STREAM , 0);

    if (IN_socket_desc == -1)
    {
        perror("*Could not create socket");
	return (void *) 1;
    }
     
    //Prepare the sockaddr_in structure
    IN_server.sin_family = AF_INET;
    IN_server.sin_addr.s_addr = INADDR_ANY;
    IN_server.sin_port = htons( peerPort );
     
    //Bind
    if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
    {
        perror("*Bind failed");
        return (void *) 1;
    }
     
    //Listen
    listen(IN_socket_desc , 1000); // The peer can handle 1000 simulteaneous connections
     
    //Accept and incoming connection
    //printf("+Waiting for incoming connections in port %i...\n", peerPort);
    //showPromptMessage (globalOpt);
    c = sizeof(struct sockaddr_in);
	
    while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        printf("+New connection accepted\n");
         
        pthread_t sniffer_thread;
        IN_new_sock = malloc(1);
        *IN_new_sock = IN_new_socket;
         
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) IN_new_sock) < 0)
        {
            perror("*Could not create thread");
            return (void *) 1;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        pthread_join( sniffer_thread , NULL);
        //puts("Handler assigned");
    }
     
    if (IN_new_socket<0)
    {
        perror("*Accept failed");
        return (void *) 1;
    }
     
    return (void *) 0;
}*/

/*int searchFile (char* fileName)
{
    int sock;
    struct sockaddr_in server;
    char serverReply[2000];
    char *serverCode;
    char request[80];
    char peerList[2000];
    char option[10];
    int fixedOption;
    int i;


    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("*Could not create socket");
    }
    //puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( SERVER_PORT );
 
    //Connect to server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("*Connection to server failed");
        return -1;
    }
     
    	printf("Connected to server\n");

	strcpy(request, "SEARCH");
	strcat(request, " ");
	strcat(request, fileName);

    	printf("\nThe request that is going to be sent is %s\n", request);
    	write(sock, request, strlen(request));
	read (sock , serverReply , sizeof(serverReply));

	strncpy(peerList, serverReply, sizeof(serverReply));
	//printf("Recieved: %s\n", serverReply);
	serverCode = strtok(serverReply," "); // The first string is the server reply
	printf ("Server reply is: %s\n", serverCode);

	if (strcmp(serverCode, "OK") == 0)
	{
		char *aux = strtok(NULL, " ");
		
		printf("File found in the following peers: \n");

		int count = 1;
		while (aux != NULL && strlen(aux)>1)
		{
			printf("\t[%i] - %s\n", count, aux);
			aux = strtok(NULL, " ");
			count++;
		}

		printf("Select the peer to download the file from [1 - %i]: ", count - 1);

		
		if (testingMode == 1)
		{
			fixedOption = 1; //Override peer choice
		}
		else
		{
			if(fgets (option, 10, stdin) == NULL ) 
	   		{
				printf("ERROR: WRONG PEER ID");	
				return -1;
			}

			fixedOption = atoi(option);
			if (fixedOption < 1 || fixedOption > count -1)
			{
				printf("Wrong option\n");
				return -1;
			}
		}
		char *peerTuple;

		strtok(peerList, " "); // Getting rid of the "OK" response

		for (i = 1; i < fixedOption; i++)
		{
			strtok(NULL, " ");
		}		

		peerTuple = strtok(NULL, " ");
		printf("You have selected option %i, which is peer %s\n", fixedOption, peerTuple);
		strtok(peerTuple,":");
		char* connectingPeer = strtok(NULL,":");
		
		printf ("Connecting to port %i, requesting file %s...\n",atoi(connectingPeer), fileName);
		if (obtainFile(atoi(connectingPeer), fileName) < 0)
		{
			printf("Error downloading file\n");
			return -1;
		}
	}

	else
	{
		printf("*Server returned error when searching for file %s\n", fileName);
		return -1;
	}

	return 0;
}*/


/*int registerFile (char* fileName)
{
    int sock;
    struct sockaddr_in server;
    char serverReply[2000];
    char request[80];
    char port[10];


    snprintf(port, sizeof( peerPort ) + 1, "%i", peerPort);

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    //puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( SERVER_PORT );
 
    //Connect to server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Connection to server failed");
        return 1;
    }
     
    printf("Connected to server\n");
    
	strcpy(request, "REGISTRY");
	strcat(request, " ");
	strcat(request, fileName);
	strcat(request, " ");
	strcat(request, peerID);
	strcat(request, ":");
	strcat(request, port);

    	printf("\nThe request that is going to be sent is %s\n", request);
    	write(sock, request, strlen(request));
	read (sock , serverReply , sizeof(serverReply));
	printf ("Server reply is: %s\n", serverReply);
	return 0;
}*/






void showPromptMessage (int option)
{
	/* This function triggers when the thread shows info about incoming connections into the terminal
	 * By doing this the user will have prompt message visible again instead of getting lost because of thread asynchronized messages
	 */

	switch(option)
	{
		case 1 :
			printf ("\nThis is peer %s. Available options:\n",peerID);
			printf ("\t[1] - Register a file to the index server\n");
			printf ("\t[2] - Obtain a file from another peer\n");
			printf (">>Select option number below\n");
		break;

		case 2 :
			printf ("\nOption 2: Register a file.\n");
			printf (">>Type below the file you want to register\n");
		break;

		case 3 :
			printf("\nOption 3: Search for a file\n");
			printf(">>Type below the file you want to search\n"); 
		break;

		default :
			printf("\n*Error: Bad prompt code\n"); 
	}
}

/* Obtains a file from a peer (Peer port is being used at this stage)*/
int obtainFile (int port, char* fileName)
{
	int sock;
	struct sockaddr_in server;
	char *request = malloc(1024 * sizeof(char));
	char *destination = malloc(250 * sizeof(char));
	int bufferPointer;
	int i;
	char buffer[256];
	int bytes_read = -1;

/**/
	char *serverReply = malloc(1024 * sizeof(char));
	char *reply;
	char myPort[5];
	char *listOfPeers;


	strncpy(destination, "./shared_folder/", 16);
	strncat(destination, fileName, strlen(fileName));

	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("Could not create socket");
		return -1;
	}
	//puts("Socket created");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( port  );

	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("*Connection to peer failed. Error");
		return -1;
	}

	printf("Connected to port %i\n", port);

	strncpy(request, "OBT", 3);
	strncat(request, " ", 1);
	strncat(request, fileName, strlen(fileName));
	
	bufferPointer = 3 + 1 + strlen(fileName);

	for (i = bufferPointer; i < 1024; i++)
	{
		strncat(request, " ", 1);
	}

	printf("\n Requesting file %s to peer in port %i\n", fileName, port);
	write(sock, request, 1024);


	bytes_read = read (sock , buffer , sizeof(buffer));
	if (bytes_read == 0)
	{
		printf("Error. The file coudln't be recieved\n");
		close(sock);
		return -1;
	}

	//strncpy(bufferAux,buffer,bytes_read - 1);

	FILE *fp;
	fp=fopen(destination, "w");

	if (fp != NULL)
	{
		int blockCount = 0;
		printf ("File %s is being transferred...", fileName);

		while( (bytes_read) > 0 )
		{
			if (blockCount % 20000 == 0)
			{
				printf(".");
			}
			fwrite (buffer, 1, bytes_read, fp);
			bytes_read = read (sock , buffer , sizeof(buffer));
			blockCount++;
		}

		printf("\nFile %s sucessfully downloaded\n", fileName); 
		//printf("VVVV FILE CONTENT (FIRST 256 BYTES) VVVV\n");
		//printf("%s\n\n", buffer);
		fclose (fp);
	}
	else 
	{
		perror ("Error opening file");
		close(sock);
		return -1;
	}

	close(sock);
	return 0;
}



/*int initHashTable()
{

}*/


/*void *index_server_handler (void *socket_desc)  
{
	int sock = *(int*)socket_desc;
	char *requestReceived = malloc(1024 * sizeof(char));
	char *header = malloc(1024 * sizeof(char));
	char *key = malloc(1024 * sizeof(char));
	char *message = malloc (1024 * sizeof(char));

	char *serverReply = malloc (1024 * sizeof(char));
	     
	read (sock , requestReceived , 1024);
	printf ("+A request has been received: %s\n", requestReceived);

	strncpy(header, requestReceived, 4);
	strncpy(key, &requestReceived[4], 20);


	//The only call that the peer can recieve is an obtain ("OBT") call. Otherwise error

	if (strcmp(header,"INIT")==0) 
	{	
		printf("+Obtain value from key %s\n", key);
		
		message = getValue(key);

		if (message == NULL)
		{
			printf("*Error: Key %s not found\n", key);
			strncpy(serverReply, "ERR ", 4);
		}
		else 
		{
			strncpy(serverReply, "OK  ", 4);
			strncat(serverReply, "                    ", 20);
			strncat(serverReply, message, strlen(message)); 
		}

		write(sock, serverReply, strlen(serverReply)); 
	}

	else
	{
		printf("*Error: Bad request\n");
		strncpy("ERR ", serverReply, 4);
		//strncpy("BAD_REQUEST ", &serverReply[24], 12);
		write(sock, serverReply, sizeof(serverReply));
	}

    return (void *) 0;
}*/

/*int indexServerConnection()
{

    printf ("\t+Initializing index server connection...\n");
     
    int IN_socket_desc, IN_new_socket, c, *IN_new_sock;
    struct sockaddr_in IN_server, client;
     
    //Create socket
    IN_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (IN_socket_desc == -1)
    {
        perror("*Could not create socket");
	return -1;
    }
     
    //Prepare the sockaddr_in structure
    IN_server.sin_family = AF_INET;
    IN_server.sin_addr.s_addr = INADDR_ANY;
    IN_server.sin_port = htons( SERVER_PORT );
     
    //Bind
    if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
    {
        perror("*Bind failed");
        return -1;
    }
     
    //Listen
    listen(IN_socket_desc , 1); // The peer can handle 1000 simulteaneous connections
     
    //Accept an incoming connection
    printf("\t+Waiting for index server to initialize\n");
    c = sizeof(struct sockaddr_in);
	
    //while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    if (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))
    {
        printf("+Connecting to index server\n");
         
        pthread_t sniffer_thread;
        IN_new_sock = malloc(1);
        *IN_new_sock = IN_new_socket;
         
        if(pthread_create (&sniffer_thread , NULL ,  index_server_handler , (void*) IN_new_sock) < 0)
        {
            perror("*Could not create thread");
            return -1;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
    }
     
    if (IN_new_socket<0)
    {
        perror("*Accept failed");
        return -1;
    }
     
    return 0;
}*/

/*int initListener()
{

}*/


/*int putCall()
{

	char *request = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	int serverIndex = 0;
	int sock = 0;
	int i;
	char *key;
	char auxPort[5];


	//sprintf(auxPort, "%d", port);

	
	strncpy(request, "PUT ", 4);
	strncat(request, key, strlen(key));
	for (i = 4 + strlen(key); i < 24; i++)
	{
		strncat(request, " ", 1);
	}
	strncat(request, auxPort, strlen(auxPort));

	serverIndex = getServerFromHash(key);
	printf("Sending request to server %i: %s\n", serverIndex, request);
	sock = 0; //serverSockets[serverIndex];

	write(sock, request, 1024);
	read (sock , serverReply , 1024);
	
	if (strcmp(serverReply,"OK  ")==0) 
	{
		return 0;
	}
	else
	{
		return -1;
	}

}*/

int selectPeer(char *listOfPeers)
{

	int peersArray[8];
	char *currentPeer;
	int selectedPeer;
	int count = 0;
	char auxOption[10];
	int option;

	printf("\nThe list of peers that hold the file is as follows:\n");
	currentPeer = strtok(listOfPeers, ":");
	
	while (currentPeer != NULL)
	{
		peersArray[count] = atoi(currentPeer);
		printf("\t[%i] - Peer at port %s\n", count + 1, currentPeer);
		currentPeer = strtok(NULL, ":");
		count ++;
	}
	
	/*currentPeer = strtok(NULL, " ");
	peersArray[count] = atoi(currentPeer);
	printf("\t[%i] - Peer at port %s\n", count, currentPeer);*/

	printf ("\n**SELECT OPTION NUMBER:");
		

	fgets(auxOption, sizeof(auxOption), stdin);
	option = atoi(auxOption);
	selectedPeer = peersArray[option - 1];

	return selectedPeer;
}


int searchCall(char *fileName)
{

	char *request = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	char *reply;
	int bufferPointer;
	int i;
	int sock;
	char myPort[5];
	char *listOfPeers;
	int selectedPeer;
	struct sockaddr_in server;


	//Create socket
	sock = socket(AF_INET, SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("Could not create socket");
		return -1;
	}
	//puts("Socket created");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( SERVER_PORT );
 
	//Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return -1;
	}

    	printf("Connected to port %i\n", SERVER_PORT);

    	//write(sock, request, strlen(request));
	sprintf(myPort, "%d", PEER_PORT);

	
	strncpy(request, "SEARCH", 6);
	strncat(request, " ", 1);
	strncat(request, fileName, strlen(fileName));

	//strncat(request, myPort ,strlen(myPort));
	bufferPointer = 6 + 1 + strlen(fileName);

	for (i = bufferPointer; i < 1024; i++)
	{
		strncat(request, " ", 1);
	}

	write(sock, request, 1024);
	read (sock , serverReply , 1024);
	close(sock);	
	reply = strtok(serverReply, " ");
	
	printf ("Reply: %s\n", reply);

	if (strcmp(serverReply,"OK")==0) 
	{
		listOfPeers = strtok(NULL, " ");
		printf ("List of peers returned is: %s\n", listOfPeers);
		strncat(listOfPeers, ":", 1);
		selectedPeer = selectPeer(listOfPeers);

		if (obtainFile(selectedPeer, fileName) < 0)
		{
			printf("*Error when obtaining file %s\n", fileName);
			return -1;
		}
		else
		{
			printf("File %s was successfully obtained\n", fileName);
			return 0;
		}

	}
	else
	{
		return -1;
	}

	/* Prepare SEARCH FILE_NAME and send */
	/* Select peer and obtain file */
	return 0;
}


int registerCall(char *fileName)
{

	char *request = malloc(1024 * sizeof(char));
	char *serverReply = malloc(1024 * sizeof(char));
	char *reply;
	int bufferPointer;
	int i;
	int sock;
	char myPort[5];
	struct sockaddr_in server;


	//Create socket
	sock = socket(AF_INET, SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("Could not create socket");
		return -1;
	}
	//puts("Socket created");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( SERVER_PORT );
 
	//Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return -1;
	}

    	printf("Connected to port %i\n", SERVER_PORT);

    	//write(sock, request, strlen(request));
	sprintf(myPort, "%d", PEER_PORT);

	
	strncpy(request, "REG ", 4);
	strncat(request, fileName, strlen(fileName));
	strncat(request, " ", 1);
	strncat(request, myPort ,strlen(myPort));
	bufferPointer = 4 + strlen(fileName) + 1 + strlen(myPort);

	for (i = bufferPointer; i < 1024; i++)
	{
		strncat(request, " ", 1);
	}

	write(sock, request, 1024);
	read (sock , serverReply , 1024);
	close(sock);	
	reply = strtok(serverReply, " ");
	
	printf ("Reply: %s\n", reply);

	if (strcmp(serverReply,"OK")==0) 
	{
		return 0;
	}
	else
	{
		return -1;
	}
}




int selectOption(int option, char *overrideKey)
{
	
	char fileName[50];
	char value[1000];
	char *fixedFileName;
	char *fixedValue;


	switch(option)
	{
	   case '1' :  /* REGISTER OPTION */
	
		printf("Register a new file\n" );
		printf("Type in the file you want to register: \n");

		if (testingMode)
		{
			fixedFileName = overrideKey;
			fixedValue = "test";
		}

		else 
		{
			if(fgets (fileName, 50, stdin) == NULL ) 
			{
				perror("Error when saving the file name");	
				return -1;
			}
			//Ask for the new key and save it

			/*In order to get rid of that annoying '\n' escape char*/
			fixedFileName = strtok(fileName, "\n"); 
			//printf("Now type the value for that key: \n");

			//Ask for value and save it
			/*if(fgets (value, 1000, stdin) == NULL ) 
			{
				perror("Error when saving the value");	
				return -1;
			}*/
			/*In order to get rid of that annoying '\n' escape char*/
			//fixedValue = strtok(value, "\n"); 
	
		}
		


		printf ("Registering file %s...\n", fixedFileName);

		if (registerCall(fixedFileName) < 0)
		{
			printf("*Error: Key %s could not be registered into the server\n", fixedFileName);
			return -1;
		}
		else
		{
			printf("File %s registered sucessfully\n", fixedFileName);
		}

		break;

	   case '2' : /* SEARCH OPTION */

		globalOpt = 3;
		//showPromptMessage(globalOpt);

		printf("Search for a file\n" );
		//globalOpt = 2;
		printf("Type in the file you want to search: \n");

		if (testingMode)
		{
			fixedFileName = overrideKey;	
		}
		else
		{
			//Ask for the new key and save it
			if(fgets (fileName, 50, stdin) == NULL ) 
			{
				perror("Error when saving the file name");	
				return -1;
			}
			/*In order to get rid of that annoying '\n' escape char*/
			fixedFileName = strtok(fileName, "\n"); 
	
		}

		if (searchCall(fixedFileName) < 0)
		{
			printf("*Error when searching for file %s\n", fixedFileName);
			return -1;
		}

		break;


	   /*case '3' :   
	

		printf("Delete a key\n" );
		printf("Type in the key you want to delete: \n");

		if (testingMode)
		{
			fixedKey = overrideKey;
		}
		else
		{
			//Ask for the new key and save it
			if(fgets (key, 20, stdin) == NULL ) 
			{
				perror("Error when saving the key");	
				return -1;
			}
			
			fixedKey = strtok(key, "\n"); 
		}
		
		
		if (getValue(fixedKey) == NULL)
		{
			printf("*Error: You can not delete keys from other peers\n");
			return -1;
		}
		if (deleteCall(fixedKey) < 0)
		{
			printf("*Error: Key %s could not be deleted\n", fixedKey);
			return -1;
		}
		else
		{
			deleteKey(fixedKey); 
			printf("The key %s was sucessfully deleted\n", fixedKey);
		}

		break;*/

	   case '3' :
		printf("The content of the DHT is shown below. Blank means the DHT is empty for this peer\n");
		printf("\n\nVV START OF LIST VV\n");
		print_files();
		printf("\n\nAA END OF LIST AA\n\n");
		break;

	   default :
	      printf("Invalid option\n" );
	}
	return 0;
}


int sendFile (int sock, char* fileName)
{
	char origin[200];
	strncpy(origin, "./shared_folder/", 16);
	strncat(origin, fileName, strlen(fileName));
	long int size;
	char buffer[256];
	float progress;
	int count = 1;
	float singleBuffSize = 256;

	FILE *fp;
	fp = fopen(origin, "r");
	int bytes_read = -1;
	if (fp != NULL)
	{
		printf ("File %s opened\n", fileName);
		fseek(fp, 0, SEEK_END); // seek to end of file
		size = ftell(fp); // get current file pointer
		fseek(fp, 0, SEEK_SET); // seek back to beginning of file
		printf("Size: %lu\n", size);
		//sleep(20);
		while ( (bytes_read = fread (buffer, 1, sizeof (buffer), fp)) > 0)
		{
			progress = (singleBuffSize * count / (float)size) * 100;

			write(sock, buffer, bytes_read);

			if (count % 1000 == 0)  //Print after 100 blocks of 256 bytes have been sent. Otherwise you print a lot!
			{
				//clear();
				printf("\rSending file %s... %.4f%%\n", fileName, progress);
			}
			count++;
		}

		fclose (fp);	
		close (sock);
		return 0;
	}

    	else 
	{
		perror ("*Error opening file");
		close (sock);
		return -1;
	}
}



/* Handles incoming connections */
void *connection_handler(void *socket_desc)
{
	
   	char *requestReceived = malloc(1024 * sizeof(char));
	char *header;
	char *firstArgv;
	int bufferPointer;
	char *serverReply = malloc(1024 * sizeof(char));


	char *res = malloc(1024 * sizeof(char));
	//char *header = malloc(1024 * sizeof(char));
	char *key = malloc(1024 * sizeof(char));
	char *finalKey;
	char *message = malloc (1024 * sizeof(char));

    	
    	int i;
    	int sock = *(int*)socket_desc;
     

    	if (read (sock, requestReceived , 1024) == 0)
	{
		printf("*Error: Connection lost\n");
		return (void *) -1;
	}

	header = strtok(requestReceived, " ");
	
	/* Obtain call */
	if (strcmp(header, "OBT") == 0) 
	{
		firstArgv = strtok (NULL, " ");  // fileName

    		if (sendFile(sock, firstArgv) < 0)
		{
			printf("*File %s could not be sent\n", firstArgv);
		}
		else
		{
			printf("+File %s sent successfully\n", firstArgv);
		}

		
		/*strncpy(serverReply, "OK", 2);
		bufferPointer = 2;

		for (i = bufferPointer; i < 1024; i++)
		{
			strncat(serverReply, " ", 1);
		}

		write(sock, serverReply, 1024);*/
		
		/*strncpy(message, &requestReceived[24], 1000);

		if (insertValue(finalKey, message) < 0)
		{
			printf("*Error registering new key");
			strncpy(serverReply, "ERR ", 4);
			write(sock, serverReply, 1024);
		}
		else
		{
			printf("New key %s has been registered\n", finalKey);
			strncpy(serverReply, "OK  ", 4);
			write(sock, serverReply, 1024);
		}*/
	}

	else 
	{
		printf ("*Error: Bad request\n");


		strncpy("ERR ", serverReply, 4);

		bufferPointer = 4;

		for (i = bufferPointer; i < 1024; i++)
		{
			strncat(serverReply, " ", 1);
		}

		write(sock, serverReply, 1024);
	
    		//Free the socket pointer
    		free(socket_desc);
     
    		return (void *) -1;

	}
}


/* Handles incoming connections */
void *server_connection_handler(void *socket_desc)
{
	
   	char *requestReceived = malloc(1024 * sizeof(char));
	char *header;
	char *firstArgv;
	char *secondArgv;
	char *serverReply; // = malloc(1024 * sizeof(char));
	char *listOfPeers; // = malloc(1024 * sizeof(char));
	char *listOfPeersFixed;
	int bufferPointer;

	char *res = malloc(1024 * sizeof(char));
	//char *header = malloc(1024 * sizeof(char));
	char *key = malloc(1024 * sizeof(char));
	char *finalKey;
	char *message = malloc (1024 * sizeof(char));

    	
    	int i;
    	int sock = *(int*)socket_desc;
     
	while (1)
	{
		serverReply = malloc (1024 * sizeof(char));
		listOfPeers = malloc(1024 * sizeof(char));

	    	if (read (sock, requestReceived , 1024) == 0)
		{
			printf("*Error: Connection lost\n");
			return (void *) -1;
		}

		header = strtok(requestReceived, " ");
		

		/* TODO: TO remove */
		/*strncpy(header, requestReceived, 3);
		strncpy(key, &requestReceived[4], 20);
	
		if (strlen(key) < 24)
		{
			finalKey = strtok(key, " ");
		}
		else
		{
			finalKey = key;
		}*/

		// Incoming header can be PUT, GET or DEL


		/* Put call */
		if (strcmp(header, "PUT") == 0) 
		{
			firstArgv = strtok (NULL, " ");  // fileName
			secondArgv = strtok (NULL, " "); // peer port

			addFile(firstArgv, secondArgv);
			printf("**File %s registered into DHT for peer at port %s\n", firstArgv, secondArgv);
			print_files();
			/**putin*/
			strncpy(serverReply, "OK", 2);
			bufferPointer = 2;

			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}

			write(sock, serverReply, 1024);
			
			/*strncpy(message, &requestReceived[24], 1000);

			if (insertValue(finalKey, message) < 0)
			{
				printf("*Error registering new key");
				strncpy(serverReply, "ERR ", 4);
				write(sock, serverReply, 1024);
			}
			else
			{
				printf("New key %s has been registered\n", finalKey);
				strncpy(serverReply, "OK  ", 4);
				write(sock, serverReply, 1024);
			}*/
		}

		/* Get call */
		else if (strcmp(header,"GET") == 0) 
		{

			
			firstArgv = strtok (NULL, " ");  // fileName
			printf("get branch. getting peers from file %s\n", firstArgv);
			listOfPeers = searchFileInDHT(firstArgv);

			if (strcmp(listOfPeers, "ERR") == 0)
			{
				printf("*Error: File not found\n");
				strncpy(serverReply, "ERR", 3);
				strncat(serverReply, " ", 1);
				bufferPointer = 3 + 1;

				for (i = bufferPointer; i < 1024; i++)
				{
					strncat(serverReply, " ", 1);
				}
			}

			else
			{
				strtok(listOfPeers, ":");
				listOfPeersFixed = strtok(NULL, " ");
				printf("Returned %s (fixed)\n", listOfPeersFixed);

				strncpy(serverReply, "OK", 2);
				strncat(serverReply, " ", 1);
				strncat(serverReply, listOfPeersFixed, strlen(listOfPeersFixed));

				bufferPointer = 2 + 1 + strlen(listOfPeersFixed);

				for (i = bufferPointer; i < 1024; i++)
				{
					strncat(serverReply, " ", 1);
				}
			}

			
			write(sock, serverReply, 1024);

		}
	
		/* Delete call TODO REMOVE THIS CALL*/
		/*else if (strcmp(header,"DEL")==0)
		{
			if (deleteKey(finalKey) < 0)
			{
				printf("*Error deleting key %s\n", finalKey);
				strncpy(serverReply, "ERR ", 4);
				write(sock, serverReply, 1024);
			}
			else
			{
				printf("Key %s has been deleted\n", finalKey);
				strncpy(serverReply,"OK  ", 4);
				write(sock, serverReply, 1024);
			}
		}*/

		else 
		{
			printf ("*Error: Bad request\n");


			strncpy("ERR ", serverReply, 4);

			bufferPointer = 4;

			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}

			write(sock, serverReply, 1024);
		
	    		//Free the socket pointer
	    		free(socket_desc);
	     
	    		return (void *) -1;
		}

		free(serverReply);
		free(listOfPeers);
	}
}


void *initIndexServerConnection(void *data)
{
	printf ("\t+Initializing DHT connection handler...\n");

	int IN_socket_desc , IN_new_socket , c , *IN_new_sock;
	struct sockaddr_in IN_server , client;

	//Create socket
	IN_socket_desc = socket(AF_INET , SOCK_STREAM , 0);

	if (IN_socket_desc == -1)
	{
		perror("*Error creating socket");
	}

	//Prepare the sockaddr_in structure
	IN_server.sin_family = AF_INET;
	IN_server.sin_addr.s_addr = INADDR_ANY;
	IN_server.sin_port = htons( PEER_PORT );

	//Bind
	if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
	{
		perror("*Error binding");
		return (void *) -1;
	}

	//Listen
	listen(IN_socket_desc , 3); 

	//Accept an incoming connection
	printf("\t+Waiting for DHT index server connection in port %i...\n", PEER_PORT);
	c = sizeof(struct sockaddr_in);

	//First connection will be the index server
	if (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))
	{
		puts(">Connection accepted");
	 
		pthread_t sniffer_thread;
		IN_new_sock = malloc(1);
		*IN_new_sock = IN_new_socket;
	 
		if (pthread_create( &sniffer_thread, NULL,  server_connection_handler, (void*) IN_new_sock) < 0)
		{
			perror("*Error creating thread for incoming request");
			free(IN_new_sock);
			return (void *) -1;
		}
	}

	connected = 1;
	// Following connections will be the ones from the peers requesting files
	while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		puts(">New connection accepted");
	 
		pthread_t new_sniffer_thread;
		IN_new_sock = malloc(1);
		*IN_new_sock = IN_new_socket;
	 
		if (pthread_create( &new_sniffer_thread, NULL,  connection_handler, (void*) IN_new_sock) < 0)
		{
			perror("*Error creating thread for new incoming request");
			free(IN_new_sock);
			return (void *) -1;
		}
	}

	if (IN_new_socket<0)
	{
		perror("*Error accepting new request");
		return (void *) -1;
	}

	return (void *) 0;
}

int main (int argc, char *argv[])
{

	char auxOption[10];
    	int option;
	int *a;
	globalOpt = 1;
	clock_t begin, end;
	double time_spent;

	if (argc < 2  || argc > 3)
	{
		printf("Usage: %s SERVER_PORT [-t]\n", argv[0]);
		return 0;
	}
	
	serverPort = atoi(argv[1]); /*TODO: Check conversion */


	pthread_t new_sniffer_thread;
 
	if (pthread_create( &new_sniffer_thread, NULL,  initIndexServerConnection, (void *) a) < 0)
	{
		perror("*Error creating thread for new incoming request");
		return -1;
	}


	sleep(1);

	while(!connected)
	{}

	while(1)
	{
		//globalOpt = 1;

		printf ("Peer initialized. Available options:\n");
		printf("\t[1] - Register a file\n");
		printf("\t[2] - Search for a file\n");
		printf("\t[3] - [Debug] Show peer DHT contents\n");
	
		printf ("\n**SELECT OPTION NUMBER:");
		

		fgets(auxOption, sizeof(auxOption), stdin);
		option = auxOption[0];

		/* TODO: Testing mode */
		/*if (testingMode == 1 && option != '4')
		{
			printf("Select the range of keys to perform this operation\n");
			printf("Lower range: ");
			fgets(lowerRange, sizeof(lowerRange), stdin);
			printf("Upper range: ");
			fgets(upperRange, sizeof(upperRange), stdin);
			selectOptionTestMode(option, lowerRange, upperRange);
		}
		else
		{*/
			selectOption(option, NULL);
		//}

		
	}

	return 0;

}



/* TODO: RUBBISH BELOW */

/*int main(int argc , char *argv[])
{

    	char option[10];
	globalOpt = 1;
	clock_t begin, end;
	double time_spent;



	if (argc < 3 || argc == 4 || argc > 5)
	{
		printf("Usage: %s PEER_ID PORT [-t SEQUENTIAL_RUNS]\n", argv[0]);
		return 0;
	}

	peerID = argv[1];
	peerPort = atoi(argv[2]);
    

	if (argc == 5 && (strcmp(argv[3], "-t") == 0))
	{
		testingMode = 1;
		seqRuns = atoi(argv[4]);
		printf ("\n\t************************\n");
		printf ("\t***** TESTING MODE *****\n");
		printf ("\t************************\n\n");
	}

	pthread_t incoming_connections_thread;
		 
	if( pthread_create( &incoming_connections_thread , NULL ,  incoming_connections_handler , NULL ) < 0)
	{
		perror("Could not create thread");
		return 1;
	}
         
	//Now join the thread, so that we dont terminate before the thread
	//pthread_join( incoming_connections_thread , NULL);
	//puts("Handler assigned");
    

   	while(1)
    	{
		globalOpt = 1;
		showPromptMessage(globalOpt);

		/*printf ("Peer initialized. Available options:\n");
		printf("\t[1] - Register a file to the index server\n");
		printf("\t[2] - Search for a file\n");
	
		

		fgets(option, sizeof(option), stdin);

		if (testingMode == 1)
		{
			begin = clock();
			checkOption(option);
			end = clock();
			time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

			printf("\n************************************\n");
			printf("******** PERFORMANCE STATS *********\n");
			printf("************************************\n\n");

			printf("+Number of executions: %i\n", seqRuns);
			printf("+Total time: %f seconds\n", time_spent);
			printf("\n************************************\n\n");
			sleep(2); // Just to show the results before showing the prompt again

		}
		else
		{
			checkOption(option);
		}
    	}   

    	return 0;
}*/
