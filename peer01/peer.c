/*
* File: peer.c
* Author: Sergio Gil Luque
* Version: 1.5
* Date: 11-03-15
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
#define PEER_ID 1

/* Prototypes */
int sendFile (int sock, char* fileName);
int obtainFile (int port, char* fileName);
void showPromptMessage ();



/* Global variables */ 
int connected = 1;
int serverPort = 0;
char *peerID;
int testingMode = 0;
int initialized = 0;
int peerPort = 0;
int globalOpt = 1;
int seqRuns = 1;
double elapsed = 0;
int opType = 0;




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

	s = find_file(fileName);

	if (s == NULL)
	{
		//printf ("ERROR: FILE NOT FOUND");
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
    		HASH_ADD_STR( registeredFiles, name, aux );

		/* Adding peer in second level of hashtable */
		fileStruct_t *s = malloc(sizeof(*s));
		strcpy(s->name, newPort);
		s->sub = NULL;
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



void showPromptMessage ()
{
	/* This function triggers when the thread shows info about incoming connections into the terminal
	 * By doing this the user will have prompt message visible again instead of getting lost because of thread asynchronized messages
	 */

	switch(globalOpt)
	{
		case 1 :
			printf ("Peer initialized. Available options:\n");
			printf("\t[1] - Register a file\n");
			printf("\t[2] - Search for a file\n");
			printf("\t[3] - Show peer DHT contents [Not available in testing mode]\n");
	
			printf ("\n>>Select option number below:\n");
		break;

		case 2 :
			printf ("\nOption 2: Register a file.\n");
			printf (">>Type below the file you want to register\n");
		break;

		case 3 :
			printf("\nOption 3: Search for a file\n");
			printf(">>Type below the file you want to search\n"); 
		break;

		case 4 :
			printf("\n************************************\n");
			printf("******** PERFORMANCE STATS *********\n");
			printf("************************************\n\n");



			printf("+Number of executions: %i\n", seqRuns);
			printf("+Operation type: ");
			if (opType == '1')
			{
				printf("PUT\n");
			}
			else
			{
				printf("GET\n");
			}
			printf("+Total time used by the CPU: %f seconds\n", elapsed);
			printf("+Average time per single call: %f seconds\n", elapsed / seqRuns);
			printf("\n************************************\n\n");
			printf("Please wait. The peer is restarting...\n");

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
		printf("VVVV FILE CONTENT (FIRST 256 BYTES) VVVV\n");
		printf("%s\n\n", buffer);
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


int selectPeer(char *listOfPeers, int override)
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

	printf ("\n**SELECT OPTION NUMBER:");
		
	if (override)
	{
		int randPeer = rand() % count++;
		selectedPeer = peersArray[randPeer];
	}
	else
	{
		fgets(auxOption, sizeof(auxOption), stdin);
		option = atoi(auxOption);
		selectedPeer = peersArray[option - 1];
	}
	

	return selectedPeer;
}


int searchCall(char *fileName, int override)
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

	sprintf(myPort, "%d", PEER_PORT);

	
	strncpy(request, "SEARCH", 6);
	strncat(request, " ", 1);
	strncat(request, fileName, strlen(fileName));

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
		selectedPeer = selectPeer(listOfPeers, override);

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




int selectOption(int option, int seqRuns)
{
	
	char fileName[50];
	char *fixedFileName;
	int i;
	char testFileName[100];
	
	

	switch(option)
	{
	   case '1' :  /* REGISTER OPTION */
	
		if (!testingMode)
		{
			globalOpt = 2;
			showPromptMessage();
		}


		if (testingMode)
		{
			//globalOpt = 4;
			for (i = 1; i <= seqRuns; i++)
			{
				sprintf(testFileName, "test%i_%i", PEER_ID, i);
				printf ("Registering file %s...\n", testFileName);
				printf ("Execution run number %i\n", i);
				if (registerCall(testFileName) < 0)
				{
					printf("*Error: File %s could not be registered into the server\n", testFileName);
					return -1;
				}
				else
				{
					printf("File %s registered sucessfully\n", testFileName);
				}
			}


		}

		else
		{
			if(fgets (fileName, 50, stdin) == NULL ) 
			{
				perror("Error when saving the file name");	
				return -1;
			}
			fixedFileName = strtok(fileName, "\n"); 

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

		}
		

		
		break;

	   case '2' : /* SEARCH OPTION */


		globalOpt = 3;
		showPromptMessage();
		//Ask for the new key and save it
		if(fgets (fileName, 50, stdin) == NULL ) 
		{
			perror("Error when saving the file name");	
			return -1;
		}
		/*In order to get rid of that annoying '\n' escape char*/
		fixedFileName = strtok(fileName, "\n"); 

		

		if (testingMode)
		{
			globalOpt = 4;
			for (i = 1; i <= seqRuns; i++)
			{
				if (PEER_ID == 8)
				{
					sprintf(testFileName, "test%i_%i", 1, i);
				}
				else
				{
					sprintf(testFileName, "test%i_%i", PEER_ID + 1, i);
				}
				
				printf("*Execution run number %i with file %s\n", i, testFileName);
				if (searchCall(testFileName, 1) < 0)
				{
					printf("*Error when searching for file %s\n", testFileName);
					//return -1;
				}
			}
	

		}
		else
		{
			if (searchCall(fixedFileName, 0) < 0)
			{
				printf("*Error when searching for file %s\n", testFileName);
				//return -1;
			}
		}


		break;

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
		while ( (bytes_read = fread (buffer, 1, sizeof (buffer), fp)) > 0)
		{
			progress = (singleBuffSize * count / (float)size) * 100;

			write(sock, buffer, bytes_read);

			if (count % 1000 == 0)  //Print after 100 blocks of 256 bytes have been sent. Otherwise you print a lot!
			{
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

	if (!testingMode)
	{
		showPromptMessage();
	}
	else
	{
		if (initialized)
		{
			globalOpt = 4;
			showPromptMessage();
		}
		globalOpt = 1;
		showPromptMessage();
	}
	return (void *) 0;
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

    	
    	int i;
    	int sock = *(int*)socket_desc;
     
	while (1)
	{
		serverReply = malloc (1024 * sizeof(char));
		listOfPeers = malloc(1024 * sizeof(char));

	    	if (read (sock, requestReceived , 1024) == 0)
		{
			printf("*Error: Connection lost\n");
			free(serverReply);
			free(listOfPeers);
			return (void *) -1;
		}

		header = strtok(requestReceived, " ");


		/* Put call */
		if (strcmp(header, "PUT") == 0) 
		{
			firstArgv = strtok (NULL, " ");  // fileName
			secondArgv = strtok (NULL, " "); // peer port

			addFile(firstArgv, secondArgv);
			printf("**File %s registered into DHT for peer at port %s\n", firstArgv, secondArgv);
			//print_files();
			/**putin*/
			strncpy(serverReply, "OK", 2);
			bufferPointer = 2;

			for (i = bufferPointer; i < 1024; i++)
			{
				strncat(serverReply, " ", 1);
			}

			write(sock, serverReply, 1024);
			
		}

		/* Get call */
		else if (strcmp(header,"GET") == 0) 
		{

			
			firstArgv = strtok (NULL, " ");  // fileName
			printf("get branch. getting peers from file %s\n", firstArgv);
			listOfPeers = searchFileInDHT(firstArgv);

			if ( (strcmp(listOfPeers, "ERR") == 0) || listOfPeers == NULL)
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
			free(listOfPeers);
	     
	    		return (void *) -1;
		}

		free(serverReply);
		free(listOfPeers);
		/*prompt*/
		if (!testingMode)
		{
			showPromptMessage();
		}
		else
		{
			if (initialized)
			{
				globalOpt = 4;
				showPromptMessage();
			}
			globalOpt = 1;
			showPromptMessage();
		}
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
		connected = 0;
		return (void *) -1;
	}

	//Listen
	listen(IN_socket_desc , 3); 

	//Accept an incoming connection
	printf("\t+Waiting for DHT index server connection in port %i...\n", PEER_PORT);
	c = sizeof(struct sockaddr_in);

	//First connection will be the index server
	if ((IN_new_socket = accept (IN_socket_desc, (struct sockaddr*)&client, (socklen_t*)&c)) )
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
	int *a = NULL;
	globalOpt = 1;
	struct timespec start, finish;

	if (argc != 2  && argc != 4)
	{
		printf("Usage: %s SERVER_PORT [-t SEQ_RUNS]\n", argv[0]);
		return 0;
	}
	
	serverPort = atoi(argv[1]);

	if (argc == 4)
	{
		if (strcmp(argv[2], "-t") == 0)
		{
			testingMode = 1;
			seqRuns = atoi(argv[3]);
		}
		else
		{
			printf("Usage: %s SERVER_PORT [-t SEQ_RUNS]\n", argv[0]);
			return 0;
		}
	}

	pthread_t new_sniffer_thread;
 
	if (pthread_create( &new_sniffer_thread, NULL,  initIndexServerConnection, (void *) a) < 0)
	{
		perror("*Error creating thread for new incoming request");
		return -1;
	}

	
	if (!connected)
	{
		printf("Please close the program and wait for a few minutes before retrying\n");
		return -1;
	}

	while(1)
	{
		/*prompt*/
		
		if (testingMode)
		{
			printf ("\n\t************************\n");
			printf ("\t***** TESTING MODE *****\n");
			printf ("\t************************\n\n");
		}

		globalOpt = 1;
		showPromptMessage();
		

		fgets(auxOption, sizeof(auxOption), stdin);
		option = auxOption[0];

		if (testingMode)
		{
			opType = option;
			if (option == '3')
			{
				printf("Option 3 is not available in testing mode\n");
				return 0;
			}

			else
			{
				globalOpt = 4;
				printf("You are about to perform %i sequential calls\n", seqRuns);
				printf("The test will start in 10 seconds...\n");
				sleep(10);

				// Starts counting
				clock_gettime(CLOCK_MONOTONIC, &start);

				selectOption(option, seqRuns);

				// Stops counting
				clock_gettime(CLOCK_MONOTONIC, &finish);

				elapsed = (finish.tv_sec - start.tv_sec);
				elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

				//showPromptMessage();

				printf("\n************************************\n");
				printf("******** PERFORMANCE STATS *********\n");
				printf("************************************\n\n");



				printf("+Number of executions: %i\n", seqRuns);
				printf("+Operation type: ");
				if (option == '1')
				{
					printf("PUT\n");
				}
				else
				{
					printf("GET\n");
				}
				printf("+Total time used by the CPU: %f seconds\n", elapsed);
				printf("+Average time per single call: %f seconds\n", elapsed / seqRuns);
				printf("\n************************************\n\n");
				printf("Please wait. The peer is restarting...\n");
				sleep (3);
			}
			
		}
		else
		{
			selectOption(option, seqRuns);
		}

		
	}

	return 0;
}


