#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <signal.h>
#include <regex>

/* port numbers */
#define HTTP_PORT 80

/* string sizes */
#define MESSAGE_SIZE 2048 

using namespace std; 

int lstn_sock, data_sock, web_sock;

void cleanExit(int sig){
	close(lstn_sock);
	close(data_sock);
	close(web_sock);
	exit(0);
}

string modified_response(int newResponseLength, string parsed_response){

	// changes anchor tags containing floppy to trolly
	regex a("<a href=\"(.*?)\">"); 
	parsed_response = regex_replace(parsed_response, a, "<a href=""./trollface.jpg"">");

	// changes image sources containing floppy to trolly
	regex imgsrc("<img src=.*[fF]loppy.*>"); 
	parsed_response = regex_replace(parsed_response, imgsrc, "<img src=\"./trollface.jpg\" width=\"300\" height=\"250\">");

	//changes occurences of floppy/Floppy to Trolly
	regex reFloppy("([fF]loppy)"); 
	parsed_response = regex_replace(parsed_response, reFloppy, "Trolly");

	// changes occurences of Italy to Germany
	regex reItaly("(\\sItaly)"); 
	parsed_response = regex_replace(parsed_response, reItaly, " Germany");

	// modified content length 
	regex contentLength("(Content-Length:.*)"); 
	parsed_response = regex_replace(parsed_response, contentLength, "Content-Length: " + to_string(newResponseLength));

	return parsed_response;
} 

int modify_Header(string parsed_response){

	int startSearch = 0; 
	int newResponseSize = 0; 
	int stringLength = parsed_response.length(); 
	int findCRLF = parsed_response.find("\r\n", startSearch);

	// strip out the message, to get true size 
	if (findCRLF != std::string::npos){
		string newResponseLength = parsed_response.substr(findCRLF, stringLength);
		newResponseSize = newResponseLength.length(); 
	}

	return newResponseSize;
}


int main(int argc, char* const argv[]){

	if (argc != 2){
		cout<< "Usage: Enter a valid port number as first argument."<<endl;
		exit(-1);
	}

	int PROXY_PORT = atoi(argv[1]);  

	char client_request[MESSAGE_SIZE], server_request[MESSAGE_SIZE], server_response[10*MESSAGE_SIZE], client_response[10*MESSAGE_SIZE];
	char url[MESSAGE_SIZE], host[MESSAGE_SIZE], path[MESSAGE_SIZE];
	int clientBytes, serverBytes, i;


    /* to handle ungraceful exits */
    signal(SIGTERM, cleanExit);
    signal(SIGINT, cleanExit);

    //initialize proxy address
	cout<<"Initializing proxy address..."<<endl; 
	struct sockaddr_in proxy_addr;
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(PROXY_PORT);
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//create listening socket
	cout<<"Creating lstn_sock..."<<endl;
	lstn_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (lstn_sock <0){
		perror("socket() call failed");
		exit(-1);
	}

	//bind listening socket
	cout<<"Binding lstn_sock..."<<endl;
	if (bind(lstn_sock, (struct sockaddr*)&proxy_addr, sizeof(struct sockaddr_in)) < 0){
		perror("bind() call failed");
		exit(-1);
	}

	//listen for client connection requests
	cout<<"Listening on lstn_sock..."<<endl;
	if (listen(lstn_sock, 20) < 0){
		perror("listen() call failed...\n");
		exit(-1);
	}

	//infinite while loop for listening
	while (1){
		cout<<"Accepting connections..."<<endl; 

		//Fork to accept multiple connections 
		pid_t pid = fork();
		
		//fork();
		// if (pid < 0){
		// 	cout << "Fork() failed proxing exiting....."<<endl;
		// 	exit(-1);
		// }

		//accept client connection request 
		data_sock = accept(lstn_sock, NULL, NULL);
		if (data_sock < 0){
			perror("accept() call failed\n");
			exit(-1);
		}

		//while loop to receive client requests
		while ((clientBytes = recv(data_sock, client_request, MESSAGE_SIZE, 0)) > 0){
			printf("%s\n", client_request);

			//copy to server_request to preserve contents (client_request will be damaged in strtok())
			strcpy(server_request, client_request);

			//tokenize to get a line at a time
			char *line = strtok(client_request, "\r\n");
			
			//extract url 
			sscanf(line, "GET http://%s", url);
			
			//separate host from path
			for (i = 0; i < strlen(url); i++){
				if (url[i] =='/'){
					strncpy(host, url, i);
					host[i] = '\0';
					break;
				}
			}
			bzero(path, MESSAGE_SIZE);
			strcat(path, &url[i]);

			cout<<"Host: "<<host<<", Path: "<<path<<endl;

			//initialize server address
			cout<<"Initializing server address..."<<endl;
			struct sockaddr_in server_addr;
			struct hostent *server;
			server = gethostbyname(host);

			bzero((char*)&server_addr, sizeof(struct sockaddr_in));
			server_addr.sin_family = AF_INET;
			server_addr.sin_port = htons(HTTP_PORT);
			bcopy((char*)server->h_addr, (char*)&server_addr.sin_addr.s_addr, server->h_length);

			//create web_socket to communicate with web_server
			web_sock = socket(AF_INET, SOCK_STREAM, 0);
			if (web_sock < 0){
				perror("socket() call failed\n");
			}

			//send connection request to web server
			if (connect(web_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))<0){
				perror("connect() call failed\n");
			}

			//send http request to web server
			if (send(web_sock, server_request, MESSAGE_SIZE, 0) < 0){
				perror("send(0 call failed\n");
			}

			//receive http response from server
			serverBytes = 0;
			while((serverBytes = recv(web_sock, server_response, MESSAGE_SIZE, 0)) > 0){

				//char response_array[10*MESSAGE_SIZE];
				// Modify response

				// check message type, if image dont modify 
				// if (messageType != std::string::npos){
				// 	//change modified response string to char array
				// 	cout<<"Changing response"<<endl;
				// 	cout<<response_array<<endl;
				// 	//strcpy(response_array, modified_response(server_response, modify_Header(server_response)).c_str()); 
				// 	bcopy(response_array, client_response, serverBytes);
				// }else{
				// 	cout<<"Not changing response"<<endl;
				// 	cout<<server_response<<endl;
				// 	bcopy(server_response, client_response, serverBytes);
				// }

				string parsed_response = string(server_response);
				int messageType = parsed_response.find("Content-Type: text/html", 0);

				// check message type, if image dont modify, else if text modify 
				if (messageType != std::string::npos){
					string response = modified_response(modify_Header(server_response), parsed_response);
					cout << response << endl; 
					//send http response to client 
					if (send(data_sock, response.data(), response.size(), 0) < 0){
						perror("send() call failed...\n");
					}
				}else{
					// copy server response to client response buffer 
					bcopy(server_response, client_response, serverBytes);

					//send http response to client 
					if (send(data_sock, client_response, serverBytes, 0) < 0){
						perror("send() call failed...\n");
					}
				}

				//clear buffers 
				bzero(client_response, MESSAGE_SIZE);
				bzero(server_response, MESSAGE_SIZE);
			}
		}

		close(data_sock);
	}
	return 0;
}