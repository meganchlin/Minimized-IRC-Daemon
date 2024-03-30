#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <queue>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>  
#include <set>

using namespace std;

void err_sys(const char* x) 
{ 
    perror(x); 
    exit(1); 
}

int main(int argc, char **argv)
{
	int i, j, maxi, maxfd, listenfd, connfd, sockfd;
    int nready, nwready, client[FD_SETSIZE];
    char* client_name[FD_SETSIZE];
    set<int> client_chan[FD_SETSIZE];
    char* channel[100];
    char* topic[100];
    ssize_t n;
    fd_set rset, wset, allset;
    char buf[1024];
    socklen_t clilen;
    struct sockaddr_in cliaddr, cliaddrs[FD_SETSIZE], servaddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

    signal(SIGPIPE, SIG_IGN);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = ntohs((unsigned short)strtoul(argv[1], NULL, 0));
    
    bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, 512);

    maxfd = listenfd; /* initialize */
    maxi = -1; /* index into client[] array */
    for (i = 0; i < FD_SETSIZE; i++){
        client[i] = -1; /* -1: available entry */
        client_chan[i].clear();
        client_name[i] = NULL;
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    int cli = 0;
    int new_cli[FD_SETSIZE];
    string tmp;

    int chan_n = 0;
    int chan_p[100];
    for (i = 0; i < 100; i++){
        chan_p[i] = 0;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;

    for (i = 0; i < FD_SETSIZE; i++){
        new_cli[i] = -1;
    }

	while(1) {
        rset = allset; /* structure assignment */
        nready = select(maxfd+1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset)) { /* new client connection */
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (sockaddr *) &cliaddr, &clilen);
            for (i = 0; i < FD_SETSIZE; i++){
                if (client[i] < 0) {
                    //cout << i + 1 << " new client\n";
                    client[i] = connfd; /* save descriptor */
                    cliaddrs[i] = cliaddr;
                    cli++;
                    new_cli[i] = 0;
                    cout << "* client connected from " << inet_ntoa(cliaddrs[i].sin_addr) << ":" << ntohs(cliaddrs[i].sin_port) << "\n";
                    break;
                }
            }
            if (i == FD_SETSIZE) perror("too many clients");
            FD_SET(connfd, &allset); /* add new desc. to set */

            if (connfd > maxfd) maxfd = connfd; /* for select */

            if (i > maxi) maxi = i; /* max index in client[] array */
            nready--;
        }

        for (i = 0; i <= maxi; i++) { /* check all clients for data */
            //cout << "read ready: " << nready << '\n';

            if ( (sockfd = client[i]) < 0)
                continue;
            if (FD_ISSET(sockfd, &rset)){
                bzero(&buf, 1024);
                if ( (n = read(sockfd, buf, 1024)) <= 0) {
                    /* connection closed by client */
                    //cout << "connection closed by " << sockfd << '\n';
                    close(sockfd);
                    cli--;
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    for (set<int>::iterator it = client_chan[i].begin(); it != client_chan[i].end(); it++) {
                        chan_p[*it]--;
                    }
                    client_chan[i].clear();

                    if (i == maxi){
                        for (j = maxi - 1; j >= 0; j--){
                            if (client[j] >= 0){
                                maxi = j;
                                break;
                            }
                        }
                    }

                    if (sockfd == maxfd){
                        for (j = maxfd - 1; j >= 0; j--){
                            if (FD_ISSET(j, &allset)){
                                maxfd = j;
                                break;
                            }
                        }
                    } 

                    cout << "* client " << inet_ntoa(cliaddrs[i].sin_addr) << ":" << ntohs(cliaddrs[i].sin_port) << " disconnected\n";
                    //cout << client_name[i] << '\n';
                    delete[] client_name[i];
                } else {
                    char tok[1024];
                    strcpy(tok, buf);
                    char *saveptr1, *saveptr2;
                    char* command_l = strtok_r(tok, "\r\n", &saveptr1);
                    //cout << sizeof(command_l) << "\n";

                    while (command_l != NULL){
                        char* command = strtok_r(command_l, " \n", &saveptr2);
                        cout << command << "\n";
                        if (command == NULL){
                        }
                        else if (!strcmp(command, "NICK")){
                            char* name = strtok_r(NULL, " \n", &saveptr2);
                            cout << name << '\n';
                            if (name == NULL){
                                tmp = ":mircd 431 :No nickname given\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            } else {
                                for (j = 0; j <= maxi; j++){
                                    //cout << "line 178\n";
                                    //cout << client[j] << endl;
                                    //cout << (client_name[j] != NULL) << endl;
                                    if (client[j] != -1 && client_name[j] != NULL && !strcmp(name, client_name[j])){
                                        //delete[] client_name[i];
                                        tmp = ":mircd 436 " + (string)name + " :Nickname collision KILL\n";
                                        send(sockfd, tmp.c_str(), tmp.length(), 0);
                                        break;
                                    }
                                }
                                //cout << "line 186\n";
                                if (j > maxi){
                                    if (client_name[i] != NULL && new_cli[i] != 0 && new_cli[i] != 2){
                                        //cout << "line 170\n";
                                        delete[] client_name[i];
                                    }
                                    //cout << "line 192\n";
                                    client_name[i] = new char[strlen(name)];
                                    strcpy(client_name[i], name);
                                    //cout << "line 195\n";
                                    if (new_cli[i] == 0){
                                        new_cli[i] = 1;
                                        //cout << "line 201\n";
                                    } else if (new_cli[i] == 2){
                                        cout << "in" << endl;
                                        tmp = ":mircd 001 " + string(name) + " :Welcome to the minimized IRC daemon!\n";
                                        tmp += ":mircd 251 " + string(name) + " :There are " + to_string(cli) + " users and 0 invisible on 1 server\n";
                                        tmp += ":mircd 375 " + string(name) + " :- mircd Message of the day -\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  Hello, World!\n";
                                        tmp += ":mircd 372 " + string(name) + " :-               @                    _ \n";
                                        tmp += ":mircd 372 " + string(name) + " :-   ____  ___   _   _ _   ____.     | |\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  | | | | | | | | | |   | |    /  _  |\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  | | | | | | | | | |   | |__  | |_| |\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\n";
                                        tmp += ":mircd 372 " + string(name) + " :-  minimized internet relay chat daemon\n";
                                        tmp += ":mircd 372 " + string(name) + " :-\n";
                                        tmp += ":mircd 376 " + string(name) + " :End of message of the day\n";
                                        send(sockfd, tmp.c_str(), tmp.length(), 0);
                                        new_cli[i] = -1;
                                        //cout << "line 193\n";
                                    }
                                }
                                //cout << "line 197\n";
                            }
                        } else if (!strcmp(command, "USER")){
                            //cout << "line 200\n";
                            char* user = strtok_r(NULL, " \n", &saveptr2);
                            char* host = strtok_r(NULL, " \n", &saveptr2);
                            char* server = strtok_r(NULL, " \n", &saveptr2);
                            char* real = strtok_r(NULL, " \n", &saveptr2);
                            //cout << "line 206\n";
                            if (user == NULL || host == NULL || server == NULL || real == NULL){
                                if (client_name[i] == NULL){
                                    tmp = ":mircd 461 " + string(command) + " :Not enought parameters\n";
                                } else {
                                    tmp = ":mircd 461 " + string(client_name[i]) + " " + string(command) + " :Not enought parameters\n";
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                //cout << "line 210\n";
                            } else {
                                //cout << "line 212\n";
                                if (new_cli[i] == 0){
                                    new_cli[i] = 2;
                                    //cout << "line 215\n";
                                } else if (new_cli[i] == 1){
                                    //cout << "line 216\n";
                                    tmp = ":mircd 001 " + string(client_name[i]) + " :Welcome to the minimized IRC daemon!\n";
                                    tmp += ":mircd 251 " + string(client_name[i]) + " :There are " + to_string(cli) + " users and 0 invisible on 1 server\n";
                                    tmp += ":mircd 375 " + string(client_name[i]) + " :- mircd Message of the day -\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  Hello, World!\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-               @                    _ \n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-   ____  ___   _   _ _   ____.     | |\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  | | | | | | | | | |   | |    /  _  |\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  | | | | | | | | | |   | |__  | |_| |\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-  minimized internet relay chat daemon\n";
                                    tmp += ":mircd 372 " + string(client_name[i]) + " :-\n";
                                    tmp += ":mircd 376 " + string(client_name[i]) + " :End of message of the day\n";
                                    send(sockfd, tmp.c_str(), tmp.length(), 0);
                                    new_cli[i] = -1;
                                    //cout << "line 230\n";
                                }
                            }
                        } else if (!strcmp(command, "PING")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* server = strtok_r(NULL, " \n", &saveptr2);
                            if (server == NULL){
                                tmp = ":mircd 409 " + string(client_name[i]) + " :No origin specified\n";
                            } else {
                                tmp = "PONG " + string(server) + "\n";
                                char* server2 = strtok_r(NULL, " \n", &saveptr2);
                                if (server2 != NULL){
                                    tmp += "PONG " + string(server) + "\n";
                                }
                            }
                            send(sockfd, tmp.c_str(), tmp.length(), 0);
                        } else if (!strcmp(command, "LIST")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chann = strtok_r(NULL, " \n", &saveptr2);
                            tmp = ":mircd 321 " + string(client_name[i]) + " Channel :Users  Name\n";
                            for (j = 0; j < chan_n; j++){
                                if (chann != NULL && strcmp(chann, channel[j])){
                                    continue;
                                }
                                if (topic[j] == NULL){
                                    tmp += ":mircd 322 " + string(client_name[i]) + " " + string(channel[j]) + " " + to_string(chan_p[j]) + "\n";
                                } else {
                                    tmp += ":mircd 322 " + string(client_name[i]) + " " + string(channel[j]) + " " + to_string(chan_p[j]) + " :" + string(topic[j]) + "\n";
                                }
                                if (chann != NULL && !strcmp(chann, channel[j])) break;
                            }
                            tmp += ":mircd 323 " + string(client_name[i]) + " :End of /LIST\n";
                            send(sockfd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                        } else if (!strcmp(command, "JOIN")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chan = strtok_r(NULL, " \n", &saveptr2);
                            //cout << chan << "\n";
                            if (chan == NULL){
                                if (client_name[i] == NULL){
                                    tmp = ":mircd 461 " + string(command) + " :Not enought parameters\n";
                                } else {
                                    tmp = ":mircd 461 " + string(client_name[i]) + " " + string(command) + " :Not enought parameters\n";
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            } else {
                                tmp = ":" + string(client_name[i]) + " JOIN " + string(chan) + "\n";
                                for (j = 0; j < chan_n; j++){
                                    if (!strcmp(chan, channel[j])){
                                        break;
                                    }
                                }
                                for (int k = 0; k <= maxi; k++){
                                    if ( k != i && !client_chan[k].empty() && client_chan[k].count(j) != 0){
                                        send(client[k], tmp.c_str(), tmp.length(), 0);
                                    }
                                }
                                if (j == chan_n){
                                    channel[j] = new char[strlen(chan)];
                                    strcpy(channel[j], chan);
                                    chan_n++;
                                    topic[j] = NULL;
                                }
                                client_chan[i].insert(j);
                                chan_p[j]++;
                                if (topic[j] == NULL){
                                    tmp += ":mircd 331 " + string(client_name[i]) + " " + string(chan) + " :No topic is set\n";
                                } else {
                                    tmp += ":mircd 332 " + string(client_name[i]) + " " + string(chan) + " :" + string(topic[j]) + "\n";
                                }
                                for (int k = 0; k <= maxi; k++){
                                    if (client[k] != -1 && client_chan[k].count(j) != 0){
                                        tmp += ":mircd 353 " + string(client_name[i]) + " " + string(chan) + " :" + string(client_name[k]) + "\n";
                                    }
                                }
                                tmp += ":mircd 366 " + string(client_name[i]) + " " + string(chan) + " :End of Names List\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            }

                        } else if (!strcmp(command, "TOPIC")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chan = strtok_r(NULL, " \n", &saveptr2);
                            if (chan == NULL){
                                if (client_name[i] == NULL){
                                    tmp = ":mircd 461 " + string(command) + " :Not enought parameters\n";
                                } else {
                                    tmp = ":mircd 461 " + string(client_name[i]) + " " + string(command) + " :Not enought parameters\n";
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            } else {
                                for (j = 0; j < chan_n; j++){
                                    if (!strcmp(chan, channel[j])){
                                        break;
                                    }
                                }
                                if (client_chan[i].count(j) == 0){
                                    tmp = ":mircd 442 " + string(client_name[i]) + " " + string(chan) + " :You are not on that channel\n";
                                } else {
                                    char* top = strtok_r(NULL, ":", &saveptr2);
                                    if (top == NULL){
                                        if (topic[j] == NULL){
                                            tmp = ":mircd 331 " + string(client_name[i]) + " " + string(chan) + " :No topic is set\n";
                                        } else {
                                            tmp = ":mircd 332 " + string(client_name[i]) + " " + string(chan) + " :" + string(topic[j]) + "\n";
                                        }
                                    } else {
                                        if (topic[j] != NULL){
                                            delete[] topic[j];
                                        }
                                        topic[j] = new char[strlen(top)];
                                        strcpy(topic[j], top);
                                        tmp = ":mircd 332 " + string(client_name[i]) + " " + string(chan) + " :" + string(top) + "\n";
                                    }
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            }

                        } else if (!strcmp(command, "NAMES")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chan = strtok_r(NULL, " \n", &saveptr2);
                            if (!(chan == NULL && client_chan[i].empty())){
                                //cout << "line 365\n";
                                if (chan == NULL) {
                                    tmp = ":mircd 353 " + string(client_name[i]) + " Nicks " + string(channel[*(client_chan[i].begin())]) + " :";
                                } else {
                                    tmp = ":mircd 353 " + string(client_name[i]) + " Nicks " + string(chan) + " :";
                                }
                                //cout << "line 371\n";
                                int first = 0;
                                int channel_num = -1;
                                for (j = 0; j < chan_n; j++){
                                    if (!strcmp(chan, channel[j])){
                                        channel_num = j;
                                        break;
                                    }
                                }
                                for (j = 0; j <= maxi; j++){
                                    if ( client[j] < 0 || client_chan[j].empty()){
                                        //cout << client_name[j] << " line 375\n";
                                        continue;
                                    }
                                    //cout << "line 377\n";
                                    if (chan == NULL && client_chan[j].count(*(client_chan[i].begin())) == 0){
                                        continue;
                                    } else if (chan != NULL && client_chan[j].count(channel_num) == 0){
                                        continue;
                                    }
                                    //cout << "line 383\n";
                                    if (first != 0){
                                        tmp += " ";
                                    } else {
                                        first = 1;
                                    }
                                    //cout << "line 389\n";
                                    tmp += client_name[j];
                                }
                                tmp += "\n";
                                //cout << "line 393\n";
                                if (chan == NULL) {
                                    tmp += ":mircd 366 " + string(client_name[i]) + " " + string(channel[*(client_chan[i].begin())]) + " :End of /NAMES list\n";
                                }else {
                                    tmp += ":mircd 366 " + string(client_name[i]) + " " + string(chan) + " :End of /NAMES list\n";
                                }
                                //cout << "line 399\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            }
                        } else if (!strcmp(command, "PART")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chan = strtok_r(NULL, " \n", &saveptr2);
                            if (chan == NULL){
                                if (client_name[i] == NULL){
                                    tmp = ":mircd 461 " + string(command) + " :Not enought parameters\n";
                                } else {
                                    tmp = ":mircd 461 " + string(client_name[i]) + " " + string(command) + " :Not enought parameters\n";
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            } else{
                                tmp = ":" + string(client_name[i]) + " PART :" + string(chan) + "\n";
                                for (j = 0; j < chan_n; j++){
                                    if (!strcmp(chan, channel[j])){
                                        break;
                                    }
                                }
                                for (int k = 0; k <= maxi; k++){
                                    if ( k != i && !client_chan[k].empty() && client_chan[k].count(j) != 0){
                                        send(client[k], tmp.c_str(), tmp.length(), 0);
                                    }
                                }
                                if (j == chan_n){
                                    tmp += ":mircd 403 " + string(client_name[i]) + " " + string(chan) + " :No such channel\n";
                                } else if (client_chan[i].count(j) == 0) {
                                    tmp += ":mircd 442 " + string(client_name[i]) + " " + string(chan) + " :You are not on that channel\n";
                                } else {
                                    //cout << client_name[i] << "leave channel\n";
                                    client_chan[i].erase(j);
                                    chan_p[j]--;
                                }
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                            }
                        } else if (!strcmp(command, "USERS")) {
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            tmp = ":mircd 392 " + string(client_name[i]) + " :UserID                         Terminal  Host\n";
                            for (j = 0; j <= maxi; j++){
                                if (client[j] < 0)
                                    continue;
                                char buffer[128] = {0};
                                if (!strcmp(inet_ntoa(cliaddrs[i].sin_addr), "127.0.0.1")){
                                    snprintf(buffer, 128, " :%-30s %-9s %-8s\n", client_name[j], "-", "localhost");
                                } else {
                                    snprintf(buffer, 128, " :%-30s %-9s %-8s\n", client_name[j], "-", inet_ntoa(cliaddrs[i].sin_addr));
                                }
                                tmp += ":mircd 393 " + string(client_name[i]) + buffer;
                            }
                            tmp += ":mircd 394 " + string(client_name[i]) + " :End of users\n";
                            send(sockfd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                        } else if (!strcmp(command, "PRIVMSG")){
                            if (new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            char* chan = strtok_r(NULL, " \n", &saveptr2);
                            char* msg = strtok_r(NULL, ":", &saveptr2);
                            if (chan == NULL){
                                tmp = ":mircd 411 " + string(client_name[i]) + " :No recipient given (" + string(command) + ")\n";
                                send(sockfd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                            } else if (msg == NULL){
                                tmp = ":mircd 412 " + string(client_name[i]) + " :No text to send\n";
                                send(sockfd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                            } else {
                                cout << "line 412\n";
                                for (j = 0; j < chan_n; j++){
                                    cout << "line 414\n";
                                    if (!strcmp(chan, channel[j])){
                                        tmp = ":" + string(client_name[i]) + " PRIVMSG " + string(chan) + " :" + string(msg) +"\n";
                                        for (int k = 0; k <= maxi; k++){
                                            if (i != k && client_chan[k].count(j) != 0){
                                                cout << "line 419\n";
                                                send(client[k], tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                                            }
                                        }
                                        break;
                                    }
                                }
                                int k;
                                for (k = 0; k < maxi; k++){
                                    cout << "line 532\n";
                                    if (client_name[k] != NULL && !strcmp(chan, client_name[k])){
                                        tmp = ":" + string(client_name[i]) + " PRIVMSG " + string(client_name[k]) + " :" + string(msg) +"\n";
                                        cout << "line 536\n";
                                        send(client[k], tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                                        break;
                                    }
                                }
                                if (j == chan_n && k == maxi){
                                    tmp = ":mircd 401 " + string(client_name[i]) + " " + string(chan) + " :No such nick/channel\n";
                                    send(sockfd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL);
                                }
                            }
                        } else if (!strcmp(command, "QUIT")){
                            close(sockfd);
                            cli--;
                            FD_CLR(sockfd, &allset);
                            client[i] = -1;

                            for (set<int>::iterator it = client_chan[i].begin(); it != client_chan[i].end(); it++) {
                                chan_p[*it]--;
                            }
                            client_chan[i].clear();

                            if (i == maxi){
                                for (j = maxi - 1; j >= 0; j--){
                                    if (client[j] >= 0){
                                        maxi = j;
                                        break;
                                    }
                                }
                            }

                            if (sockfd == maxfd){
                                for (j = maxfd - 1; j >= 0; j--){
                                    if (FD_ISSET(j, &allset)){
                                        maxfd = j;
                                        break;
                                    }
                                }
                            } 

                            cout << "* client " << inet_ntoa(cliaddrs[i].sin_addr) << ":" << ntohs(cliaddrs[i].sin_port) << " disconnected\n";
                            //cout << client_name[i] << '\n';
                            delete[] client_name[i];
                        } else {
                            if (strcmp(command, "CAP") && new_cli[i] != -1){
                                tmp = ":mircd 451  :You have not registered\n";
                                send(sockfd, tmp.c_str(), tmp.length(), 0);
                                break;
                            }
                            //cout << "line 458\n";
                            if (new_cli[i] != -1 && new_cli[i] != 1){
                                tmp = ":mircd 421  " + (string)command + " :Unknown command\n";
                            } else {
                                tmp = ":mircd 421 " + string(client_name[i]) + " " + (string)command + " :Unknown command\n";
                            }
                            
                            send(sockfd, tmp.c_str(), tmp.length(), 0);
                        }
                        //cout << "line 462\n";
                        command_l = strtok_r(NULL, "\n\r", &saveptr1);
                    }
                }
                nready--;
            }
            if (nready <= 0) {
                break; /* no more readable descriptors */
            }
        }
    }
	exit(0);
}