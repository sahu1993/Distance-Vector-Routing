/**
* @ssahu3_assignment3
* @author  Shivam Sahu <ssahu3@buffalo.edu>
* @version 1.0
*
* @section LICENSE
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details at
* http://www.gnu.org/copyleft/gpl.html
*
* @section DESCRIPTION
*
* This contains the main function. Add further description here....
*/

/**
* main function
*
* @param  argc Number of arguments
* @param  argv The argument list
* @return 0 EXIT_SUCCESS
*/
#include <iostream>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>

#define TRUE 1
#define FALSE 0
#define CNTRL_HEADER_SIZE 8
#define CNTRL_RESP_HEADER_SIZE 8
#define DATA_PACKET_SIZE 1036
#define DATA_PACKET_HEADER_SIZE 12
#define DATA_PACKET_PAYLOAD_SIZE 1024
#define AUTHOR_STATEMENT "I, ssahu3, have read and understood the course academic integrity policy."
#define RESPONSE_STATEMENT "Response"
#define PACKET_USING_STRUCT
#define TOTAL_ROUTER 5

struct __attribute__((__packed__)) fileStatsHeader{
  uint8_t transfer_id;
  uint8_t ttl;
  uint16_t padding;
};

struct __attribute__((__packed__)) dataPacketHeader {
  uint32_t destination_ip;
  uint8_t transfer_id;
  uint8_t ttl;
  uint16_t seq_no;
  uint16_t fin;
  uint16_t pad;
};

struct __attribute__((__packed__)) routerInfo{
  uint16_t id;
  uint16_t router_port;
  uint16_t data_port;
  uint16_t cost;
  uint32_t ip;
};

struct __attribute__((__packed__)) routingTable{
  uint16_t id;
  uint16_t pad;
  uint16_t next_hop;
  uint16_t cost;
  uint32_t ip;
  uint16_t router_port;
  uint16_t data_port;
};

struct __attribute__((__packed__)) distanceVectorUpdate{
  uint32_t ip;
  uint16_t router_port;
  uint16_t pad;
  uint16_t id;
  uint16_t cost;
};

struct __attribute__((__packed__)) myInfo{
  uint16_t noOfUpdateField;
  uint16_t my_router_port;
  uint32_t my_ip;
  distanceVectorUpdate dv[5];
};

struct __attribute__((__packed__)) CONTROL_HEADER
{
    uint32_t dest_ip_addr;
    uint8_t control_code;
    uint8_t response_time;
    uint16_t payload_len;
};

struct __attribute__((__packed__)) CONTROL_RESPONSE_HEADER
{
    uint32_t controller_ip_addr;
    uint8_t control_code;
    uint8_t response_code;
    uint16_t payload_len;
};

struct statsResponse{
  uint8_t transfer_id;
  uint8_t ttl;
  uint16_t seq_no;
};

struct timeval timeObj;

uint16_t control_port;
uint16_t no_of_router;
uint16_t update_time;
int router_socket, data_socket, control_socket, head_socket;
fd_set master_list, watch_list;
uint16_t** distance_matrix ;
int my_index;
bool firstTime = true;
bool firstTimeTcpConnect = true;
std::fstream output_file;
std::fstream sent_file;
char *lastPacket, *penultimatePacket;
int data_send_sock_foward, data_send_sock;
uint16_t time_of_router = 0;

struct routingTable *routingTableBuffer;
struct routerInfo *routerInfoBuffer;

std::vector<statsResponse> stats;
std::vector<routerInfo> neighbour;
std::vector<routerInfo> next_hop;

void init();
int create_control_sock();
int new_control_conn(int);
int new_data_conn(int);
void remove_control_conn(int);
void remove_data_conn(int);
bool isControl(int);
bool isData(int);
bool control_recv_hook(int);
bool data_recv_hook(int sock_index);

void author_response(int);
void init_response(int , char *);
void routingTable_response(int , char *r);
void update_response(int , char *r);
void crash_response(int , char *r);
void sendFile_response(int , char *r, uint16_t);
void sendFileStats_response(int , char *r);
void lastDataPacket_response(int , char *r);
void penultimateDataPacket_response(int , char *r);

int tcpConnect(int);
int findNextHopIndex(int);
void writePacketsIntoFile(uint16_t, uint16_t, char *);
void forwardDataPacket(uint32_t, uint16_t, char*);
char* create_response_header(int , uint8_t , uint8_t , uint16_t );
char * create_data_header(uint32_t , uint8_t, uint8_t , uint16_t , int, int);


void printRouterInfo();
void printroutingTable();
void listenOnRouterSocket();
void initializeDistanceMatrix();
void printNeighbour();
void packAndSendDistanceVector();
void unpackAndRecvDistanceVector(int);
char* getIPinPres(uint32_t);
void printDistanceMatrix();
void bellmanFord();
void updateRoutingTableBuffer();
void listenOnDataSocket();
void UpdateLastDataPacket(char *);

const uint16_t charToInt16(const char *, unsigned int &);
const uint32_t charToInt32(const char *, unsigned int &);
const uint8_t charToInt8(const char *, unsigned int &);
ssize_t recvALL(int , char *, ssize_t );
ssize_t sendALL(int , char *, ssize_t );
ssize_t recvALLD(int , struct dataPacket *, ssize_t );
ssize_t sendALLD(int , struct dataPacket *, ssize_t );

struct ControlConn
{
    int sockfd;
    LIST_ENTRY(ControlConn) next;
}*connection, *conn_temp;
LIST_HEAD(ControlConnsHead, ControlConn) control_conn_list;

struct DataConn
{
    int sockfd;
    LIST_ENTRY(DataConn) next;
}*data, *data_temp;
LIST_HEAD(DataConnsHead, DataConn) data_conn_list;

int main(int argc, char **argv)
{
  //std::cout << "Inside main" << '\n';
  timeObj.tv_sec = 1000;
  /*Start Here*/
  lastPacket = (char *) malloc(sizeof(char)*1036);
  bzero(lastPacket,1036);
  penultimatePacket = (char *) malloc(sizeof(char)*1036);
  bzero(penultimatePacket,1036);
  control_port = atoi(argv[1]);
  init();

  return 0;
}

void init(){
  //std::cout << "Inside init" << '\n';
  control_socket = create_control_sock();
  //std::cout << "control_socket: "<< control_socket << '\n';
  FD_ZERO(&master_list);
  FD_ZERO(&watch_list);
  /* Register the control socket */
  FD_SET(control_socket, &master_list);
  int selret, sock_index, fdaccept, data_fdaccept;

  head_socket = control_socket;

  while(TRUE){

    watch_list = master_list;

    selret = select(head_socket+1, &watch_list, NULL, NULL, &timeObj);
    if(selret < 0){
      //std::cout << "select failed" << '\n';
    }
    else if(selret == 0) {
      timeObj.tv_sec = update_time;
      packAndSendDistanceVector();

    }else{
      for(sock_index=0; sock_index <= head_socket; sock_index+=1){

        if(FD_ISSET(sock_index, &watch_list)){
          //std::cout << "sock_index inside select: "<< sock_index << '\n';
          /* control_socket */
          if(sock_index == control_socket){
            fdaccept = new_control_conn(sock_index);

            /* Add to watched socket list */
            FD_SET(fdaccept, &master_list);
            if(fdaccept > head_socket) {
              head_socket = fdaccept;
            }
          }

          /* router_socket */
          else if(sock_index == router_socket){
            //call handler that will call recvfrom() ...
            unpackAndRecvDistanceVector(sock_index);
          }

          /* data_socket */
          else if(sock_index == data_socket){
            //new_data_conn(sock_index);
            data_fdaccept = new_data_conn(sock_index);
            FD_SET(data_fdaccept, &master_list);
            if(data_fdaccept > head_socket) {
              head_socket = data_fdaccept;
            }
          }

          /* Existing connection */
          else{
              if(isControl(sock_index)){
                if(!control_recv_hook(sock_index)) {
                  FD_CLR(sock_index, &master_list);
                }
              }
              else if(isData(sock_index)){
                if(!data_recv_hook(sock_index)) {
                  FD_CLR(sock_index, &master_list);
                }
              }
              //else ERROR("Unknown socket index");
          }
        }
      }
    }
  }
}

int create_control_sock(){
  //std::cout << "Inside create_control_sock" << '\n';
  int sock ;
  struct sockaddr_in control_addr;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0){
    //std::cout << "Cannot create socket" << '\n';
  }

  bzero(&control_addr, sizeof(control_addr));

  control_addr.sin_family = AF_INET;
  control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  control_addr.sin_port = htons(control_port);

  if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0){
    //std::cout << "Bind failed" << '\n';
  }

  if(listen(sock, 5) < 0){
    //std::cout << "Unable to listen on port" << '\n';
  }
  return sock;
}

int new_control_conn(int sock_index)
{
    std::cout << "inside new_control_conn" << '\n';
    int fdaccept ;
    socklen_t caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);

    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, &caddr_len);
    if(fdaccept < 0){
      //std::cout << "Cannot accept connections" << '\n';
    }

    /* Insert into list of active control connections */
    connection = (struct ControlConn*)malloc(sizeof(struct ControlConn));
    connection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&control_conn_list, connection, next);

    return fdaccept;
}

int new_data_conn(int sock_index)
{
    std::cout << "inside new_data_conn" << '\n';
    int fdaccept ;
    socklen_t caddr_len;
    struct sockaddr_in remote_data_addr;

    caddr_len = sizeof(remote_data_addr);

    fdaccept = accept(sock_index, (struct sockaddr *)&remote_data_addr, &caddr_len);
    if(fdaccept < 0){
      //std::cout << "Cannot accept connections" << '\n';
    }

    //std::cout << "New socket for date send: "<< fdaccept << '\n';

    /* Insert into list of active data connections */
    data = (struct DataConn*)malloc(sizeof(struct DataConn));
    data->sockfd = fdaccept;
    LIST_INSERT_HEAD(&data_conn_list, data, next);
    return fdaccept;
}

void remove_control_conn(int sock_index)
{
    std::cout << "Inside remove_control_conn" << '\n';
    LIST_FOREACH(connection, &control_conn_list, next)
    {
        if(connection->sockfd == sock_index) LIST_REMOVE(connection, next); // this may be unsafe?
        free(connection);
    }
    close(sock_index);
}

void remove_data_conn(int sock_index)
{
    std::cout << "Inside remove_data_conn" << '\n';
    LIST_FOREACH(data, &data_conn_list, next)
    {
        if(data->sockfd == sock_index) LIST_REMOVE(data, next); // this may be unsafe?
        free(data);
    }
    close(sock_index);
}

bool isControl(int sock_index)
{
    //std::cout << "Inside isControl" << '\n';
    LIST_FOREACH(connection, &control_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool isData(int sock_index)
{
    //std::cout << "Inside isData" << '\n';
    LIST_FOREACH(data, &data_conn_list, next)
        if(data->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool control_recv_hook(int sock_index)
{
    char *cntrl_header, *cntrl_payload;
    uint8_t control_code;
    uint16_t payload_len;

    /* Get control header */
    cntrl_header = (char *) malloc(sizeof(char)*CNTRL_HEADER_SIZE);
    bzero(cntrl_header, CNTRL_HEADER_SIZE);

    if(recvALL(sock_index, cntrl_header, CNTRL_HEADER_SIZE) < 0){
        remove_control_conn(sock_index);
        free(cntrl_header);
        return FALSE;
    }

    struct CONTROL_HEADER *header = (struct CONTROL_HEADER *) cntrl_header;
    control_code = header->control_code;
    payload_len = ntohs(header->payload_len);
    //std::cout << "payload_len: " << payload_len << '\n';

    free(cntrl_header);

    /* Get control payload */
    if(payload_len != 0){
        cntrl_payload = (char *) malloc(sizeof(char)*payload_len);
        bzero(cntrl_payload, payload_len);
        if(recvALL(sock_index, cntrl_payload, payload_len) < 0){
            //std::cout << "Inside if" << '\n';
            remove_control_conn(sock_index);
            free(cntrl_payload);
            return FALSE;
        }
    }


    /* Triage on control_code */
    switch(control_code){
        case 0: author_response(sock_index);
                break;

        case 1: init_response(sock_index, cntrl_payload);
                break;

        case 2: routingTable_response(sock_index, cntrl_payload);
                break;

        case 3: update_response(sock_index, cntrl_payload);
                break;

        case 4: crash_response(sock_index, cntrl_payload);
                break;

        case 5: sendFile_response(sock_index, cntrl_payload, payload_len);
                break;

        case 6: sendFileStats_response(sock_index, cntrl_payload);
                break;

        case 7: lastDataPacket_response(sock_index, cntrl_payload);
                break;

        case 8: penultimateDataPacket_response(sock_index, cntrl_payload);
                break;

              /* .......
            .........
           .......
         ......*/
    }

    if(payload_len != 0) free(cntrl_payload);
    return TRUE;
}

void author_response(int sock_index)
{
  // std::cout << "Inside author_response" << '\n';
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

	payload_len = sizeof(AUTHOR_STATEMENT)-1; // Discount the NULL chararcter
	cntrl_response_payload = (char *) malloc(payload_len);
	memcpy(cntrl_response_payload, AUTHOR_STATEMENT, payload_len);

	cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);

	response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);
	/* Copy Payload */
	memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
	free(cntrl_response_payload);

	sendALL(sock_index, cntrl_response, response_len);
	free(cntrl_response);
}

void init_response(int sock_index, char *req_payload)
{
  // std::cout << "Inside init_response" << '\n';
  unsigned increment = 0;
  no_of_router = charToInt16(req_payload, increment);
  update_time = charToInt16(req_payload, increment);
  timeObj.tv_sec = update_time;

  //printf( "No of router: %d\n",no_of_router);
  //printf( "update_time: %d\n",update_time);

  routerInfoBuffer = (struct routerInfo*)malloc(no_of_router*sizeof(struct routerInfo));
  routingTableBuffer = (struct routingTable*)malloc(no_of_router*sizeof(struct routingTable));

  distance_matrix = new uint16_t*[no_of_router];
  for(int i = 0; i < no_of_router; ++i){
      distance_matrix[i] = new uint16_t[no_of_router];
  }

  for (int i = 0; i < no_of_router; i++){
    for (int j = 0; j < no_of_router; j++){
      if(i == j){
          distance_matrix[i][j] = 0;
      }else{
          distance_matrix[i][j] = 65535;
      }
    }
  }

  for(int i = 0; i < no_of_router; i++){
    routerInfoBuffer[i].id = charToInt16(req_payload, increment);
    routerInfoBuffer[i].router_port = charToInt16(req_payload, increment);
    routerInfoBuffer[i].data_port = charToInt16(req_payload, increment);
    routerInfoBuffer[i].cost = charToInt16(req_payload, increment);
    routerInfoBuffer[i].ip = charToInt32(req_payload, increment);
    if(routerInfoBuffer[i].cost > 0 && routerInfoBuffer[i].cost < 65535){
      neighbour.push_back(routerInfoBuffer[i]);
    }
  }

  for(int i = 0; i< no_of_router; i++ ){
    if(routerInfoBuffer[i].cost == 0){
      my_index = i;
      break;
    }
  }

  for (int i = 0; i < no_of_router; i++){
    routingTableBuffer[i].id  = routerInfoBuffer[i].id;
    routingTableBuffer[i].pad = 0;

    routingTableBuffer[i].cost = routerInfoBuffer[i].cost;
    if(routerInfoBuffer[i].cost == 65535){
        routingTableBuffer[i].next_hop = 65535;
    }else{
        routingTableBuffer[i].next_hop = routerInfoBuffer[i].id;
    }
    routingTableBuffer[i].ip = routerInfoBuffer[i].ip ;
    routingTableBuffer[i].data_port = routerInfoBuffer[i].data_port ;
    routingTableBuffer[i].router_port = routerInfoBuffer[i].router_port ;
  }

  listenOnRouterSocket();
  listenOnDataSocket();
  initializeDistanceMatrix();

	char *cntrl_response_header, *cntrl_response;
	cntrl_response_header = create_response_header(sock_index, 1, 0, 0);

	cntrl_response = (char *) malloc(CNTRL_RESP_HEADER_SIZE);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, CNTRL_RESP_HEADER_SIZE);
}


void  routingTable_response(int sock_index, char *req_payload)
{
  //std::cout << "Inside routingTable_response" << '\n';
  uint16_t payload_len, response_len;
  char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;
  payload_len = 8*no_of_router; // Discount the NULL chararcter
  cntrl_response_payload = (char *) malloc(payload_len);
  int increment = 0;
  for (int i = 0; i < no_of_router; i++){
    struct routingTable *temp = (struct routingTable *) (cntrl_response_payload +(increment * 8));
    temp->id = htons(routingTableBuffer[i].id) ;
    temp->pad = htons(0);
    temp->cost = htons(routingTableBuffer[i].cost) ;
    temp->next_hop = htons(routingTableBuffer[i].next_hop);
    ++increment;
  }
  //memcpy(cntrl_response_payload, (&routingTableBuffer), payload_len);
  cntrl_response_header = create_response_header(sock_index, 2, 0, payload_len);

  response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
  cntrl_response = (char *) malloc(response_len);
  /* Copy Header */
  memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
  free(cntrl_response_header);
  /* Copy Payload */
  memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
  free(cntrl_response_payload);

  sendALL(sock_index, cntrl_response, response_len);

  free(cntrl_response);
}

void update_response(int sock_index, char *req_payload){

  unsigned increment = 0;

  uint16_t router_id = charToInt16(req_payload, increment);
  uint16_t cost = charToInt16(req_payload, increment);

  // std::cout << '\n';
  // std::cout << "Changed router_id: " << router_id << '\n';
  // std::cout << "Changed cost: " << cost << '\n';
  // std::cout << '\n';

  int i = 0;
  for(i = 0; i < no_of_router; i++){
    if(routingTableBuffer[i].id == router_id){
        routingTableBuffer[i].cost = cost;
        if(cost != 65535){
          routingTableBuffer[i].next_hop = router_id;
        }
        break;
    }
  }
  distance_matrix[my_index][i] = cost;
  // std::cout << "Printing routingTableBuffer after update " << '\n';
  // printroutingTable();
  // std::cout << "Printing distance_matrix after update " << '\n';
  // printDistanceMatrix();
  bellmanFord();
  updateRoutingTableBuffer();

  // std::cout << "Printing routingTableBuffer after update after updateRoutingTableBuffer " << '\n';
  // printroutingTable();
  // std::cout << "Printing distance_matrix after update after bellmanFord " << '\n';
  // printDistanceMatrix();

  char *cntrl_response_header, *cntrl_response;
	cntrl_response_header = create_response_header(sock_index, 3, 0, 0);

	cntrl_response = (char *) malloc(CNTRL_RESP_HEADER_SIZE);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, CNTRL_RESP_HEADER_SIZE);

}


void crash_response(int sock_index, char *req_payload){
  char *cntrl_response_header, *cntrl_response;
	cntrl_response_header = create_response_header(sock_index, 4, 0, 0);

	cntrl_response = (char *) malloc(CNTRL_RESP_HEADER_SIZE);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, CNTRL_RESP_HEADER_SIZE);
  close(sock_index);

}

void sendFile_response(int sock_index, char *req_payload, uint16_t payload_len){
  std::cout << "Inside sendFile_response" << '\n';
  unsigned increment = 0;

  uint32_t destination_ip = charToInt32(req_payload,increment);
  uint8_t ttl = charToInt8(req_payload,increment);
  uint8_t transfer_id = charToInt8(req_payload,increment);
  uint16_t seq_no = charToInt16(req_payload,increment);

  //std::cout << "destination_ip: " << getIPinPres(destination_ip) << '\n';
  //printf("ttl: %u\n",ttl );
  //printf("transfer_id: %u\n",transfer_id );
  //std::cout << "seq_no: " <<  seq_no << '\n';
  uint16_t file_size = payload_len - 8 ;

  std::string fileName = std::string((req_payload + 8),file_size);

  bool flag = true;
  int j;
  for(j = 0; j < no_of_router; j++){
    if(routingTableBuffer[j].ip == destination_ip){
      flag = false;
      break;
    }
  }

  if(flag){
    return;
  }
  //std::cout << "printing destination index for file sending: " << j <<'\n';
  data_send_sock =  tcpConnect(j);
  //std::cout << "data_send_sock: " << data_send_sock <<'\n';
  long file_length;
  sent_file.open(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate );
  sent_file.seekg (0, sent_file.end);
  file_length = sent_file.tellg();
  sent_file.seekg (0, sent_file.beg);
  long total_packet = file_length / DATA_PACKET_PAYLOAD_SIZE;
  //std::cout << "total_packet: " <<  total_packet << '\n';
  //std::cout  << '\n';
  //for (int i = 0; i < total_packet; i++)
  uint16_t data_payload_length = DATA_PACKET_SIZE;
  for (int i = 0; i < total_packet; i++)
  {
    char *data_response_header, *data_response_payload, *data_response;
    data_response_header = create_data_header(destination_ip, ttl, transfer_id, seq_no, total_packet, i);

    struct statsResponse obj;
    obj.transfer_id = transfer_id;
    obj.ttl = ttl;
    obj.seq_no = seq_no;
    stats.push_back(obj);

    data_response_payload = (char *) malloc(sizeof(char)*DATA_PACKET_PAYLOAD_SIZE);
    bzero(data_response_payload, DATA_PACKET_PAYLOAD_SIZE);
    sent_file.read(data_response_payload, DATA_PACKET_PAYLOAD_SIZE);

    data_response = (char *) malloc(sizeof(char)*DATA_PACKET_SIZE);
    memcpy(data_response, data_response_header, DATA_PACKET_HEADER_SIZE);
    memcpy(data_response+DATA_PACKET_HEADER_SIZE, data_response_payload, DATA_PACKET_PAYLOAD_SIZE);
    UpdateLastDataPacket(data_response);
    free(data_response_header);
    free(data_response_payload);

    sendALL(data_send_sock, data_response, DATA_PACKET_SIZE);

    seq_no = seq_no + 1;
    free(data_response);
  }

  sent_file.close();

  char *cntrl_response_header, *cntrl_response;
	cntrl_response_header = create_response_header(sock_index, 5, 0, 0);

	cntrl_response = (char *) malloc(CNTRL_RESP_HEADER_SIZE);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, CNTRL_RESP_HEADER_SIZE);
  free(cntrl_response);

}

void sendFileStats_response(int sock_index, char *req_payload){

  unsigned increment = 0;
  uint8_t transfer_id = charToInt8(req_payload,increment);
  printf("transfer_id in sendFileStats_response: %u\n", transfer_id);
  char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;
  uint16_t payload_len = 4;
  uint16_t response_len;

  for(int i =0; i < stats.size(); i++){
    if(stats[i].transfer_id == transfer_id){
        payload_len = payload_len + 2;
    }
  }

  cntrl_response_payload = (char *) malloc(payload_len);
  bzero(cntrl_response_payload, payload_len);

  struct fileStatsHeader *header = (struct fileStatsHeader *) cntrl_response_payload;
  header->padding = htons(0);

  unsigned increment1 = 4;

  for(int i =0; i < stats.size() ;i++){
    if(stats[i].transfer_id == transfer_id){
     header->ttl = stats[i].ttl;
     header->transfer_id =  transfer_id;
     uint16_t seq_no = htons(stats[i].seq_no);
     memcpy(cntrl_response_payload + increment1, &seq_no, 2);
     increment1 = increment1 + 2;
    }
  }

  cntrl_response_header = create_response_header(sock_index, 6, 0, payload_len);

  response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
  cntrl_response = (char *) malloc(response_len);
  /* Copy Header */
  memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
  free(cntrl_response_header);
  /* Copy Payload */
  memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
  free(cntrl_response_payload);

  sendALL(sock_index, cntrl_response, response_len);

  free(cntrl_response);
}


void initializeDistanceMatrix(){
  //std::cout << "Inside initializeDistanceMatrix" << '\n';
  for(int j = 0; j < no_of_router; j++){
    distance_matrix[my_index][j] = routingTableBuffer[j].cost;
  }
  printDistanceMatrix();
  //std::cout << '\n' ;
}

void printDistanceMatrix(){
  for (int x = 0; x < no_of_router; x++){
    for (int y = 0; y < no_of_router; y++){
      //std::cout << distance_matrix[x][y] << '\t';
    }
    //std::cout << '\n' ;
  }
}

void printNeighbour(){
  //std::cout << "Inside printNeighbour " << '\n';
  for (int i=0; i<neighbour.size();i++){
    // std:: cout << neighbour[i].id << '\t';
    // std:: cout << neighbour[i].router_port << '\t';
    // std:: cout << getIPinPres(neighbour[i].ip) << '\t';
    // std:: cout << neighbour[i].cost << '\n';
  }
  //std::cout << '\n';
}

void packAndSendDistanceVector(){
  //std::cout << "Inside packAndSendDistanceVector" << '\n';

  char *send = (char *)malloc(sizeof(char)*68);
  bzero(send,68);

  struct myInfo *myInfoObjSend = (struct myInfo *) send;
  myInfoObjSend->noOfUpdateField = htons(no_of_router);
  myInfoObjSend->my_router_port = htons(routerInfoBuffer[my_index].router_port) ;
  myInfoObjSend->my_ip = htonl(routerInfoBuffer[my_index].ip) ;

  for (int i = 0; i < no_of_router; i++){
      myInfoObjSend->dv[i].ip = htonl(routerInfoBuffer[i].ip) ;
      myInfoObjSend->dv[i].router_port = htons(routerInfoBuffer[i].router_port) ;
      myInfoObjSend->dv[i].pad = htons(0);
      myInfoObjSend->dv[i].id = htons(routerInfoBuffer[i].id) ;
      myInfoObjSend->dv[i].cost = htons(routingTableBuffer[i].cost) ;
  }


  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0){
    //std::cout << "Cannot create socket in packAndSendDistanceVector " << '\n';
  }

  for(int i = 0; i < no_of_router; i++){

    if(routingTableBuffer[i].next_hop != 65535 && routingTableBuffer[i].next_hop != routingTableBuffer[my_index].id){
      int j = findNextHopIndex(routingTableBuffer[i].next_hop);
      struct sockaddr_in neighbour_addr;
      bzero(&neighbour_addr, sizeof(neighbour_addr));
      neighbour_addr.sin_family = AF_INET;
      neighbour_addr.sin_port = htons(routingTableBuffer[j].router_port);
      inet_pton(AF_INET, getIPinPres(routingTableBuffer[j].ip) , &(neighbour_addr.sin_addr));
      int bytes = sendto(sockfd, send, 68, 0, (struct sockaddr *)&neighbour_addr, sizeof(neighbour_addr));
      //std::cout << "bytes: " << bytes <<'\n';
      if(bytes < 0){
         //std::cout << "sendto failed" << '\n';
      }
    }
  }
  free(send);
}

void unpackAndRecvDistanceVector(int sockfd){
  //std::cout << "Inside unpackAndRecvDistanceVector" << '\n';
  char *recv = (char *) malloc(sizeof(char)*68);
  bzero(recv, 68);

  socklen_t len;
  struct sockaddr_in from;
  bzero(&from, sizeof(from));
  socklen_t fromlen = sizeof(from);
  if(recvfrom(sockfd, recv, 68, 0, (struct sockaddr *)&from, &fromlen) < 0){
     perror("ERROR: ");
  }

  struct myInfo *myInfoObjRec = (struct myInfo *) recv;


  int neighbour_index = 0;
  for(neighbour_index = 0; neighbour_index < no_of_router; neighbour_index++ ){
    if(ntohs(myInfoObjRec->dv[neighbour_index].cost) == 0)
      break;
  }
  //std::cout << "neighbour_index: " << neighbour_index <<'\n';
  //std::cout << "Source Router Port" << ntohs(myInfoObjRec.my_router_port) <<'\n';
  for(int j = 0 ; j < no_of_router ;j++){
      distance_matrix[neighbour_index][j] = ntohs(myInfoObjRec->dv[j].cost);
  }

  //std::cout << "my_index: " << my_index <<'\n';
  //printDistanceMatrix();
  //std::cout << "Before bellmanFord but after update" << '\n';
  //printDistanceMatrix();
  bellmanFord();
  //std::cout << "After bellmanFord" << '\n';
  //g();
  //std::cout << '\n';
  updateRoutingTableBuffer();
  //std::cout << "After bellmanFord Routing Table" << '\n';
  //printroutingTable();
  //printRouterInfo();
  free(recv);
}

void bellmanFord(){
  for (int i = 0; i < no_of_router; i++) {
    for (int j = 0; j < no_of_router; j++){
      if(distance_matrix[my_index][i] > distance_matrix[my_index][j]+distance_matrix[j][i]){
        distance_matrix[my_index][i] = distance_matrix[my_index][j]+distance_matrix[j][i] ;
        routingTableBuffer[i].next_hop = routingTableBuffer[j].id;
      }
    }
  }
}

void updateRoutingTableBuffer(){
  for (int j = 0; j < no_of_router; j++){
    routingTableBuffer[j].cost = distance_matrix[my_index][j];
  }
}


bool data_recv_hook(int sock_index){
  //std::cout << "/* Inside data_recv_hook */" << '\n';
  uint32_t destination_ip;
  uint8_t transfer_id ;
  uint8_t ttl;
  uint16_t seq_no;
  uint16_t fin;

  char *data_header, *data_payload;
  data_header = (char *) malloc(sizeof(char)*DATA_PACKET_HEADER_SIZE);
  bzero(data_header, DATA_PACKET_HEADER_SIZE);

  if(recvALL(sock_index, data_header, DATA_PACKET_HEADER_SIZE) < 0){
      remove_data_conn(sock_index);
      free(data_header);
      return FALSE;
  }

  struct dataPacketHeader *header = (struct dataPacketHeader *) data_header;
  destination_ip = ntohl(header->destination_ip);
  //std::cout << "dest_ip" << getIPinPres(destination_ip) <<'\n';
  transfer_id = header->transfer_id;
  //printf("t_id: %u\n",transfer_id );
  ttl = header->ttl;
  //header->ttl = header->ttl - 1;
  //printf("t_id: %u\n",ttl );
  seq_no = ntohs(header->seq_no);
  //std::cout << "seq_no" << seq_no <<'\n';
  fin = ntohs(header->fin);
  //std::cout << "fin" << fin <<'\n';
  free(data_header);

  ttl = ttl - 1;
  if(ttl == 0)
  {
    return TRUE;
  }

  data_payload = (char *) malloc(sizeof(char)*DATA_PACKET_PAYLOAD_SIZE);
  bzero(data_payload, DATA_PACKET_PAYLOAD_SIZE);
  if(recvALL(sock_index, data_payload, DATA_PACKET_PAYLOAD_SIZE) < 0){
      remove_data_conn(sock_index);
      free(data_header);
      free(data_payload);
      return FALSE;
  }
   //std::cout << "Printing data recieved: "<< data_payload << '\n';

  char *again_pack_data_header;
  again_pack_data_header = (char *) malloc(sizeof(char)*DATA_PACKET_HEADER_SIZE);
  bzero(again_pack_data_header, DATA_PACKET_HEADER_SIZE);
  struct dataPacketHeader *again_header = (struct dataPacketHeader *) again_pack_data_header;
  again_header->destination_ip = htonl(destination_ip) ;
  again_header->transfer_id = transfer_id;
  again_header->ttl = ttl ;
  again_header->seq_no = htons(seq_no);
  again_header->fin = htons(fin);
  again_header->pad = htons(0);

  char *again_pack_data_packet;
  again_pack_data_packet = (char *) malloc(sizeof(char)*DATA_PACKET_SIZE);
  bzero(again_pack_data_packet, DATA_PACKET_SIZE);

  memcpy(again_pack_data_packet,again_pack_data_header,DATA_PACKET_HEADER_SIZE);
  memcpy(again_pack_data_packet+DATA_PACKET_HEADER_SIZE,data_payload,DATA_PACKET_PAYLOAD_SIZE);

  //UpdateLastDataPacket(again_pack_data_header, data_payload);
  UpdateLastDataPacket(again_pack_data_packet);
  free(again_pack_data_header);

  struct statsResponse obj;
  obj.transfer_id = transfer_id;
  obj.ttl = ttl;
  obj.seq_no = seq_no;

  stats.push_back(obj);

  //std::cout << "File content on recieving: "<< data_payload << '\n';
  if(destination_ip == routingTableBuffer[my_index].ip){
    writePacketsIntoFile(transfer_id, fin ,data_payload);
  }else{
    forwardDataPacket(destination_ip, fin, again_pack_data_packet);
  }
  free(again_pack_data_packet);
  free(data_payload);
  return TRUE;
}

void forwardDataPacket(uint32_t destination_ip, uint16_t fin_bit ,char *forward_data_packet){

  bool flag = true;
  int j;
  for(j = 0; j < no_of_router; j++){
    if(routingTableBuffer[j].ip == destination_ip){
      flag = false;
      break;
    }
  }
  if(flag){
    return;
  }
  if(firstTimeTcpConnect){
    data_send_sock_foward =  tcpConnect(j);
    firstTimeTcpConnect = false;
  }

  if(fin_bit == 0x8000){
    firstTimeTcpConnect = true;
  }

  char *data_response;
  //data_response_header = create_data_header(destination_ip, ttl, transfer_id, seq_no, 0, 0, fin_bit, true );

  data_response = (char *) malloc(sizeof(char)*DATA_PACKET_SIZE);

  // memcpy(data_response, data_response_header, DATA_PACKET_HEADER_SIZE);
  // free(data_response_header);

  memcpy(data_response, forward_data_packet, DATA_PACKET_SIZE);

  sendALL(data_send_sock_foward, data_response, DATA_PACKET_SIZE);
  free(data_response);

}

void writePacketsIntoFile(uint16_t transfer_id, uint16_t fin ,char *fileContent){
  //printf("Inside writePacketsIntoFile \n");
  std::string file_name;
  if(firstTime){
    std::stringstream file;
    file << "file-" << transfer_id;
    file_name = file.str();
    //std::cout << "Filename: "<< file_name  << '\n';
    firstTime = false;
    output_file.open(file_name.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
  }
  output_file.write(fileContent,DATA_PACKET_PAYLOAD_SIZE);
  if(fin == 0x8000){
    // std::cout << "fin == 1" << '\n';
    output_file.close();
    firstTime = true;
    //firstTimeTcpConnect = true;
  }
}


char * create_data_header(uint32_t destination_ip, uint8_t ttl, uint8_t transfer_id, uint16_t seq_no, int total_packet, int i){
  //std::cout << "Inside create_data_header" << '\n';
  //std::cout << "fin_bit: " << fin_bit << '\n';
  //std::cout << "foward_flag: " << foward_flag << '\n';
  char *buffer;
  struct dataPacketHeader *data_packet_header;
  buffer = (char *) malloc(sizeof(char)*DATA_PACKET_HEADER_SIZE);
  data_packet_header = (struct dataPacketHeader *) buffer;
  data_packet_header->destination_ip = htonl(destination_ip) ;
  data_packet_header->transfer_id = transfer_id;
  data_packet_header->ttl = ttl ;
  data_packet_header->seq_no = htons(seq_no);
  if (i != (total_packet - 1)) {
  //if(i != 1){
      data_packet_header->fin = htons(0);
  } else {
      data_packet_header->fin = htons(0x8000);
  }
  data_packet_header->pad = htons(0);
  return buffer;
}

int tcpConnect(int dest_index){
  //std::cout << "/* In tcpConnect */" << '\n';
  //std::cout << "dest_index: " << dest_index  <<'\n';
  uint16_t next_hop_id = routingTableBuffer[dest_index].next_hop;
  int next_hop_index = findNextHopIndex(next_hop_id);
  //std::cout << "next_hop_index: " << next_hop_index  <<'\n';
  uint16_t next_hop_data_port = routingTableBuffer[next_hop_index].data_port;
  //std::cout << "next_hop data_port: " << next_hop_data_port << '\n';
  //std::cout << "next_hop_ip: "<< getIPinPres(routingTableBuffer[next_hop_index].ip) << '\n';


  int socket_for_file = socket(AF_INET, SOCK_STREAM, 0);
  if(socket_for_file < 0){
      //std::cout << "problem in socket creation in tcpConnect" << '\n';
  }

  struct sockaddr_in next_hop_address;
  bzero(&next_hop_address, sizeof(next_hop_address));
  next_hop_address.sin_family = AF_INET;
  next_hop_address.sin_port = htons(next_hop_data_port);
  inet_pton(AF_INET, getIPinPres(routingTableBuffer[next_hop_index].ip), &next_hop_address.sin_addr);

  int fdaccept = connect(socket_for_file, (struct sockaddr *) &next_hop_address, sizeof(next_hop_address));
  //std::cout << "Connect return value: " << fdaccept << '\n';
  if(fdaccept == -1){
      //std::cout << "problem in connect in tcpConnect" << '\n';
  }
  return socket_for_file;
}

int findNextHopIndex(int next_hop_id){
  //std::cout << "inside findNextHopIndex" << '\n';
  //std::cout << "next_hop_id: " << next_hop_id <<'\n';
    int i;
    for(i = 0; i < no_of_router; i++ )
    {
      if(routingTableBuffer[i].id == next_hop_id){
        break;
      }
    }
    return i;
}


void lastDataPacket_response(int sock_index, char *req_payload){
  std::cout << "Inside lastDataPacket_response" << '\n';
  char *cntrl_response_header, *cntrl_response;
  uint16_t response_len;

  cntrl_response_header = create_response_header(sock_index, 7, 0, DATA_PACKET_SIZE);

  response_len = CNTRL_RESP_HEADER_SIZE + DATA_PACKET_SIZE;
  cntrl_response = (char *) malloc(response_len);
  /* Copy Header */
  memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
  free(cntrl_response_header);
  /* Copy Payload */
  memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, lastPacket, DATA_PACKET_SIZE);

  ssize_t bytes = sendALL(sock_index, cntrl_response, response_len);
  //std::cout << "bytes in lastDataPacket_response: "<< bytes << '\n';
  free(cntrl_response);

}

void penultimateDataPacket_response(int sock_index, char *req_payload){
  std::cout << "Inside penultimateDataPacket_response" << '\n';
  char *cntrl_response_header, *cntrl_response;
  uint16_t response_len;

  cntrl_response_header = create_response_header(sock_index, 8, 0, DATA_PACKET_SIZE);

  response_len = CNTRL_RESP_HEADER_SIZE + DATA_PACKET_SIZE;
  cntrl_response = (char *) malloc(response_len);
  /* Copy Header */
  memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
  free(cntrl_response_header);
  /* Copy Payload */
  memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, penultimatePacket, DATA_PACKET_SIZE);

  sendALL(sock_index, cntrl_response, response_len);
  free(cntrl_response);
}

/* These function are taken from geekforgeeks.com */
const uint8_t charToInt8(const char *buffer, unsigned int &increment) {
  uint8_t value  = 0;
  memcpy(&value, buffer + increment, 1);
  increment = increment + 1;
  return value;
}


const uint16_t charToInt16(const char *buffer, unsigned int &increment) {
  uint16_t value  = 0;
  memcpy(&value, buffer + increment, 2);
  increment = increment + 2;
  return ntohs(value);
}

const uint32_t charToInt32(const char *buffer, unsigned int &increment) {
  uint32_t value = 0;
  memcpy(&value, buffer + increment, 4);
  increment = increment + 4;
  return value;
}

/* Function taken from geekforgeeks.com Ends */


void printRouterInfo(){
  //std::cout << "in printRouterInfo: "<<'\n';
  for(int i = 0; i < no_of_router; i++)
  {
    // std::cout << "r_id: " << routerInfoBuffer[i].id <<'\t';
    // std::cout << "r_port1: " << routerInfoBuffer[i].router_port <<'\t';
    // std::cout << "r_port2: " << routerInfoBuffer[i].data_port <<'\t';
    // std::cout << "r_cost: " << routerInfoBuffer[i].cost <<'\t';
    // //std::cout << "ip: "<< routerInfoBuffer[i].ip <<'\n';
    // std::cout << "ip: "<< getIPinPres(routerInfoBuffer[i].ip) <<'\n';
  }
}

void printroutingTable(){
  //std::cout << "in printroutingTable: "<<'\n';
  for(int i = 0; i < no_of_router; i++)
  {
    // std::cout << "id: " << routingTableBuffer[i].id <<'\t';
    // std::cout << "pad: " << routingTableBuffer[i].pad <<'\t';
    // std::cout << "next_hop: " << routingTableBuffer[i].next_hop <<'\t';
    // std::cout << "cost: " << routingTableBuffer[i].cost <<'\t';
    // std::cout << "ip: "<< getIPinPres(routingTableBuffer[i].ip) <<'\t';
    // //std::cout << "ip: "<< routingTableBuffer[i].ip <<'\t';
    // std::cout << "router_port: "<< routingTableBuffer[i].router_port <<'\t';
    // std::cout << "data_port: "<< routingTableBuffer[i].data_port <<'\t';
  }
  //std::cout << '\n';
}

/*This function is taken from stackoverflow.com */
char* getIPinPres(uint32_t ip)
{
   unsigned char bytes[4];
   bytes[0] = ip & 0xFF;
   bytes[1] = (ip >> 8) & 0xFF;
   bytes[2] = (ip >> 16) & 0xFF;
   bytes[3] = (ip >> 24) & 0xFF;
   char *s_ip = NULL;
	 asprintf(&s_ip, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
   return(s_ip);
}
/*Function Ends*/

char* create_response_header(int sock_index, uint8_t control_code, uint8_t response_code, uint16_t payload_len)
{
    //std::cout << "Inside create_response_header" << '\n';
    char *buffer;
    struct CONTROL_RESPONSE_HEADER *cntrl_resp_header;

    struct sockaddr_in addr;
    socklen_t addr_size;
    addr_size = sizeof(struct sockaddr_in);
    getpeername(sock_index, (struct sockaddr *)&addr, &addr_size);

    buffer = (char *) malloc(sizeof(char)*CNTRL_RESP_HEADER_SIZE);
    cntrl_resp_header = (struct CONTROL_RESPONSE_HEADER *) buffer;

    /* Controller IP Address */
    memcpy(&(cntrl_resp_header->controller_ip_addr), &(addr.sin_addr), sizeof(struct in_addr));
    /* Control Code */
    cntrl_resp_header->control_code = control_code;
    /* Response Code */
    cntrl_resp_header->response_code = response_code;
    /* Payload Length */
    cntrl_resp_header->payload_len = htons(payload_len);

    return buffer;
}

void UpdateLastDataPacket(char *payload) {
  memcpy(penultimatePacket,lastPacket,DATA_PACKET_SIZE);
  memcpy(lastPacket, payload, DATA_PACKET_SIZE);
}

ssize_t recvALL(int sock_index, char *buffer, ssize_t nbytes)
{
    //std::cout << "Inside recvALL" << '\n';
    ssize_t bytes = 0;
    bytes = recv(sock_index, buffer, nbytes, 0);
    //std::cout << "bytes" << bytes << '\n';
    //printf("%u\n",buffer);
    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += recv(sock_index, buffer+bytes, nbytes-bytes, 0);

    return bytes;
}


ssize_t sendALL(int sock_index, char *buffer, ssize_t nbytes)
{
    //std::cout << "Inside sendALL" << '\n';
    //std::cout << '\n';
    ssize_t bytes = 0;
    bytes = send(sock_index, buffer, nbytes, 0);

    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += send(sock_index, buffer+bytes, nbytes-bytes, 0);

    return bytes;
}


void listenOnRouterSocket(){
    //std::cout << "Inside listenOnRouterSocket" << '\n';

    struct sockaddr_in router_addr;
    router_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(router_socket < 0){
      //std::cout << "Cannot create socket in listenOnRouterSocket" << '\n';
    }

    bzero(&router_addr, sizeof(router_addr));

    //std::cout << "Router Port:" << routerInfoBuffer[my_index].router_port <<'\n';
    //std::cout << "Router scoket:" << router_socket <<'\n';

    router_addr.sin_family = AF_INET;
    router_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    router_addr.sin_port = htons(routerInfoBuffer[my_index].router_port);

    if(bind(router_socket, (struct sockaddr *)&router_addr, sizeof(router_addr)) < 0){
      //std::cout << "Bind failed in listenOnRouterSocket" << '\n';
    }
    //std::cout<<"head_socket after before router_socket"<< head_socket <<'\n';
    FD_SET(router_socket, &master_list);
    if(router_socket > head_socket) {
      head_socket = router_socket;
    }
    //std::cout << '\n';
}

void listenOnDataSocket(){

  //std::cout << "Inside listenOnDataSocket" << '\n';

  data_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(data_socket < 0){
    //std::cout << "Cannot create socket in listenOnDataSocket" << '\n';
  }

  struct sockaddr_in data_addr;
  bzero(&data_addr, sizeof(data_addr));

  //std::cout << "Data Port:" << routerInfoBuffer[my_index].data_port <<'\n';
  //std::cout << "Data scoket:" << data_socket <<'\n';

  data_addr.sin_family = AF_INET;
  data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  data_addr.sin_port = htons(routerInfoBuffer[my_index].data_port);

  if(bind(data_socket, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0){
    // std::cout << "Bind failed in listenOnDataSocket" << '\n';
  }

  int listen_reply = listen(data_socket, 5);
  if(listen_reply < 0){
    //std::cout << "Unable to listen on port" << '\n';
  }

  FD_SET(data_socket, &master_list);
  if(data_socket > head_socket) {
    head_socket = data_socket ;
  }
  //std::cout << '\n';
}
