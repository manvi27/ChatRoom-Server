#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <bits/stdc++.h>
#include <fstream>
#include "Helper.h"
#define NUM_GROUPS 10

using namespace std;



typedef struct 
{
  int ChatRoom;
  std::string NickName;
}stClientInfo_T;


typedef struct 
{
  /* data */
  int groupID;
  int senderID;
  int msgID;
  int MessageOrder;
  std::string MessageContent;
}Message;

void MessageDecode(std::string MessageContent,Message &m)
{
  m.MessageContent = MessageContent;
  std::istringstream ss(MessageContent);
  std::string token;
  int delimiterCount = 0;
  std::cout<<"Message Content = "<<MessageContent<<std::endl;

  while (std::getline(ss, token, '/')) {
    if(0 == delimiterCount)
    {
      m.groupID = std::stoi(token);
      std::cout<<"Group ID = "<<m.groupID<<std::endl;
    }
    else if(1 == delimiterCount)
    {
      m.senderID = std::stoi(token);
      std::cout<<"Sender ID = "<<m.senderID<<std::endl;
    }
    else if(2 == delimiterCount)
    {
      m.msgID = std::stoi(token);
      std::cout<<"Message ID = "<<m.msgID<<std::endl;
    }
    else if(3 == delimiterCount)
    {
      m.MessageContent = token;
      std::cout<<"Message Content = "<<m.MessageContent<<std::endl;
    }
    delimiterCount++;
  }
}


/****************************************Global Variables***********************************/
std::vector<std::string> ClientConnected;
std::map<std::string,stClientInfo_T> ClientAddrInfoMap;//Addr: Ipstr:Port
std::vector<std::tuple<std::string,int>> FwdAddress; //Forwarding address
int S[10] = {0}; // Counter value for each group
std::map<int,map<int,int>> R;// Last delivered message for each sender in each group
std::map<int,std::map<int,std::priority_queue<std::pair<int,string>, std::vector<std::pair<int,std::string>>, greater<std::pair<int,std::string>>>>> holdback;//holdback[sender ID][group]->(msgID,message)
int ServerId = 0;
bool DebugFlag = false;
int sock;
eOrdering_E eOrdering = UNORDERED;

/*Total Ordering*/
std::map<int,std::map<int,std::pair<int,int>>> ProposedNum;//[group][msgID] = (proposed number,senderID)
std::vector<int>LastProposed(NUM_GROUPS,0);//[group] = last proposed number
std::vector<int>LastAccepted(NUM_GROUPS,0);//[group] = last accepted number
std::map<int,std::map<int,pair<int,int>>> AcceptedNum;//[senderId][group] = accepted number
std::map<int,std::map<int,int>> ProposalCount;
std::map<int,std::vector<std::tuple<int,int,int,std::string>>> TotalOrder_holdback;//holdback[group]->(Proposed,proposing server,msgID,message)
/******************************************Global Functions**********************************/
void FO_multicast(int g, std::string m);
void B_multicast(int g,Message T);
void FO_deliver(int g, Message m);
void B_deliver(int g, std::string m);
bool CheckServerOrClient(std::string RcvAddr);
bool DecodeClientData(std::string ClientData, std::string &Command, std::string &Data);


/*Invoked by server on receiving message from local client*/
void FO_multicast(int g, std::string m) {
  int msgID = ++S[g];
  Message m2;
  // std::cout<<"Message received from local client"<<std::endl;
  m2.groupID = g;
  m2.senderID = ServerId;
  m2.msgID = msgID;
  m2.MessageContent = std::to_string(g) + '/' + std::to_string(ServerId) + '/' + std::to_string(m2.msgID) + '/' + m;
  // std::cout<<"Message to be multicasted = "<<m2.MessageContent<<std::endl;
  /*Multicast to other servers and other local client(which are in the same chatroom)*/
  B_multicast(g, m2);
}

void B_multicast(int g,Message m)
{
  // Message m = std::get<0>(T);
  int MsgID = m.msgID;
  std::string msg = m.MessageContent;/*send (g,<m,i,S[g]>)*/
  if (sock < 0) {
    fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
    exit(1);
  } 
  for(int i = 0; i < FwdAddress.size(); i++)
  {
    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(std::get<1>(FwdAddress[i]));/*Server port*/
    inet_pton(AF_INET, (std::get<0>(FwdAddress[i])).c_str(), &(dest.sin_addr));/*Server address*/
    if(true == DebugFlag)
      std::cout<<"Multicasting message"<<msg<<" to server *"<<std::get<0>(FwdAddress[i])<<":"<<std::get<1>(FwdAddress[i])<<"*"<<std::endl;
    sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
  } 
  /*Deliver to other local clients belonging to same group */
  if(true == DebugFlag)
    std::cout<<"Delivering message to local clients"<<std::endl;
}


void FO_deliver(int g, Message m){
  // std::cout<<"Message Content = "<<m.MessageContent<<std::endl;
  // std::cout<<"Group ID = "<<m.senderID<<std::endl;
  // std::cout<<"Message received from server"<<std::endl;
  holdback[m.senderID][g].push(make_pair(m.msgID,m.MessageContent));
  // std::cout<<"Message pushed to holdback queue"<<std::endl;
  int nextID = R[m.senderID][g]+1;
  while (holdback[m.senderID][g].top().first != nextID);
  // std::cout<<"Message popped from holdback queue"<<std::endl;
  while(!holdback[m.senderID][g].empty())
  {
    B_deliver(g, holdback[m.senderID][g].top().second);
    holdback[m.senderID][g].pop();
    R[m.senderID][g]++;
    nextID = R[m.senderID][g]+1;
  }
}

void B_deliver(int g, std::string m)
{
  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  std::string  ClientAddrInfo;
  std::string  ClientAddr;
  std::string  ClientPort;

  for (int i = 0; i < ClientConnected.size(); i++) {

    // std::cout<<"Checking for client *"<<ClientConnected[i]<<"*"<<std::endl;
    if(ClientAddrInfoMap[ClientConnected[i]].ChatRoom == g)
    {
      ClientAddrInfo = ClientConnected[i];
      ClientAddr = ClientAddrInfo.substr(0,ClientAddrInfo.find(":"));
      ClientPort = ClientAddrInfo.substr(ClientAddrInfo.find(":")+1,ClientAddrInfo.length()-ClientAddrInfo.find(":")-1);
      
      dest.sin_port = htons(atoi(ClientPort.c_str()));/*UDP port*/
      inet_pton(AF_INET, ClientAddr.c_str(), &(dest.sin_addr));
      std::string m_tosend;
      if(ClientAddrInfoMap[ClientConnected[i]].NickName.length() > 0)
      {
        m_tosend = "<" + ClientAddrInfoMap[ClientConnected[i]].NickName + ">" + m;
      }
      else
      {
        m_tosend = "<" + ClientAddr+":"+ClientPort + ">"+ m ;
      }
      if(DebugFlag == true)
        std::cout<<"Message "<<m_tosend<<"sent to"<<ClientAddr<<std::endl;
      // std::cout<<"Message to send = "<<m_tosend<<std::endl;
      sendto(sock, m_tosend.c_str(), m_tosend.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
    }
  }
}

bool CheckServerOrClient(std::string RcvAddr)
{
  // std::cout<<"Rcvaddr = "<<RcvAddr<<std::endl;
  for(int i = 0;i < FwdAddress.size(); i++)
  {
    // std::cout<<"Checking for server or client addr"<<std::get<0>(FwdAddress[i])<<"<>"<<RcvAddr.substr(0,RcvAddr.find(":"))<<std::endl;
    // std::cout<<"Checking for server or client port"<<std::get<1>(FwdAddress[i])<<"<>"<< atoi((RcvAddr.substr(RcvAddr.find(":")+1,RcvAddr.length()-RcvAddr.find(":")-1)).c_str())<<std::endl;
    if(std::get<0>(FwdAddress[i]) == RcvAddr.substr(0,RcvAddr.find(":")) && \
      (std::get<1>(FwdAddress[i]) == atoi((RcvAddr.substr(RcvAddr.find(":")+1,RcvAddr.length()-RcvAddr.find(":")-1)).c_str())))
      return true;
  }
  return false;
}

typedef enum
{
  PROPOSE_PROMPT,
  PROPOSE_MADE,
  PROPOSE_ACCEPT
}ePROPOSE_STATE_E;

ePROPOSE_STATE_E DecodeTotalOrderingMessage(std::string MessageContent,Message &m)
{
  m.MessageContent = MessageContent;
  std::istringstream ss(MessageContent);
  std::string token;
  int delimiterCount = 0;
  ePROPOSE_STATE_E IsProposePrompt = PROPOSE_PROMPT;
  // std::cout<<"Message Content = "<<MessageContent<<std::endl;

  while (std::getline(ss, token, '/')) {
    if(0 == delimiterCount)
    {
      m.groupID = std::stoi(token);
      // std::cout<<"Group ID = "<<m.groupID<<std::endl;
    }
    else if(1 == delimiterCount)
    {
      m.senderID = std::stoi(token);
      // std::cout<<"Sender ID = "<<m.senderID<<std::endl;
    }
    else if(2 == delimiterCount)
    {
      m.msgID = std::stoi(token);
      // std::cout<<"Message ID = "<<m.msgID<<std::endl;
    }
    else if(3 == delimiterCount)
    {
      if(token == "*")
      {
        IsProposePrompt = PROPOSE_PROMPT;
        // std::cout<<"Propose prompt"<<std::endl;
      }  
      if(token == "?")
      {
        IsProposePrompt = PROPOSE_MADE;
        // std::cout<<"Propose made"<<std::endl;
      }
      else if(token == "!")
      {
        IsProposePrompt = PROPOSE_ACCEPT;
        // std::cout<<"Propose accept"<<std::endl;
      }
    }
    else if(4 == delimiterCount)
    {
      m.MessageOrder = std::stoi(token);
        // std::cout<<"Proposed Message order  = "<<m.MessageOrder<<std::endl;
      if(IsProposePrompt == PROPOSE_ACCEPT)
      {
        if(DebugFlag == true)
          std::cout<<"Accepted Message order  = "<<m.MessageOrder<<std::endl;
        AcceptedNum[m.groupID][m.msgID].first = m.MessageOrder;
        AcceptedNum[m.groupID][m.msgID].second = m.senderID;
      }  
    }
    else if(5 == delimiterCount)
    {
      m.MessageContent = token;
      // std::cout<<"Message Content = "<<m.MessageContent<<std::endl;
    }
    delimiterCount++;
  }
  return IsProposePrompt;
}

bool sortholdqueue(const tuple<int, int, int,string>& a,  
              const tuple<int, int, int,string>& b) 
{ 
  if(get<0>(a) < get<0>(b))
    return true;
  else if(get<0>(a) == get<0>(b))
  {
    if(get<1>(a) < get<1>(b))
      return true;
    else
      return false;
  }
  return false;
} 

void TotalO_deliver(int g,Message m)
{
  // std::cout<<"Message multicast in total ordering to local clients with holdback queue size"<<TotalOrder_holdback[g].size()<<std::endl;
  
  for(int i = 0;i<TotalOrder_holdback[g].size();i++)
  {
    if(std::get<2>(TotalOrder_holdback[g][i]) == m.msgID)
    {
      // std::cout<<"Message order for message id"<<m.msgID<<"is"<<AcceptedNum[m.groupID][m.msgID].first<<std::endl;
      std::get<0>(TotalOrder_holdback[g][i]) = AcceptedNum[m.groupID][m.msgID].first;
      std::get<1>(TotalOrder_holdback[g][i]) = AcceptedNum[m.groupID][m.msgID].second;
      break;
    }
  }
  sort(TotalOrder_holdback[g].begin(),TotalOrder_holdback[g].end(),sortholdqueue);

  for(int i = 0; i < TotalOrder_holdback[g].size();i++)
  {
      // std::cout<<"Message id = "<<ProposalCount[g][get<2>(TotalOrder_holdback[g][i])]<<" Message order = "<<std::get<0>(TotalOrder_holdback[g][i])<<std::endl;
      if(ProposalCount[g][get<2>(TotalOrder_holdback[g][i])] == FwdAddress.size())
      {
        B_deliver(g, std::get<3>(TotalOrder_holdback[g][i]));
        TotalOrder_holdback[g].erase(TotalOrder_holdback[g].begin()+i);
      }  

      else
        break;
      // TotalOrder_holdback[g].pop();
  }
}

void signalHandlerForMain(int signNum) {
  if(signNum != SIGINT) return;
  close(sock);
  exit(0);
}

int main(int argc, char *argv[]) { //argv[1] = file name, argv[2] = server ID

  if(argc < 3)
  {
    fprintf(stderr, "*** Author: Manvi Agarwal / iamanvi\n");
    exit(1);
  }
  std::ifstream file;
  std::string AddrInfo;
  std::string BindConn;
  std::string BindAddr;
  std::string BindPort;
  DebugFlag = handle_options(argc,argv,eOrdering);
  // std::cout<<"filename = "<<argv[optind]<<std::endl;
  file.open(argv[optind++]);
  if(!file)
  {
    std::cout<<"File not found"<<std::endl;
    exit(1);
  }
  else
  {
    ServerId = std::stoi(argv[optind]);  // Convert the argument to an integer
    int i = 0;
    while (std::getline(file, AddrInfo))
    {
        if (i == ServerId) {
          // std::cout << "Line " << ServerId << ": " << AddrInfo << std::endl;
            /*initialize forwarding addresses*/
          if(AddrInfo.find("\n") != std::string::npos)
          {
            BindConn  = AddrInfo.substr(AddrInfo.find(",")+1,AddrInfo.length()-AddrInfo.find(",")-2);
          }
          else
          {
            BindConn  = AddrInfo.substr(AddrInfo.find(",")+1,AddrInfo.length()-AddrInfo.find(",")-1);
          }
          // std::cout<<"Addr Info length =  *"<<AddrInfo.length()<<"*"<<std::endl;
          BindAddr = BindConn.substr(0,BindConn.find(":"));
          BindPort = BindConn.substr(BindConn.find(":")+1,BindConn.length()-BindConn.find(":")-1);
          // std::cout<<"Binding address: *"<<BindAddr<<":"<<BindPort<<"*"<<std::endl;
        }
        else
        {
          std::string FwdConn = AddrInfo.substr(0,AddrInfo.find(","));
          FwdAddress.push_back(std::make_tuple(FwdConn.substr(0,AddrInfo.find(":")),\
                                              atoi((FwdConn.substr(FwdConn.find(":")+1,FwdConn.length()-FwdConn.find(":")-1)).c_str())));

          // std::cout<<"Forwarding address: *"<<std::get<0>(FwdAddress[FwdAddress.size()-1])<<":"<<std::get<1>(FwdAddress[FwdAddress.size()-1])<<"*"<<std::endl;
        }
        ++i;
    }
  }
  // std::cout<<"Binding address"<<BindAddr<<":"<<BindPort<<std::endl;
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  int opt = 1;
  int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt));

  std::string RcvdData;
  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(BindAddr.c_str());
  servaddr.sin_port = htons(atoi(BindPort.c_str()));
  bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr));
  socklen_t addr_size = sizeof(struct sockaddr_in);
  if (getsockname(sock, (struct sockaddr*)&servaddr, &addr_size) == -1) {
    perror("getsockname");
  } else {
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &servaddr.sin_addr, ipstr, sizeof(ipstr));
    // std::cout << "Server is listening on " << ipstr << ":" << ntohs(servaddr.sin_port) << std::endl;
  }

  bool IsCommand = false;
  std::string Command;
  std::string Data;
  char ipstr[INET_ADDRSTRLEN];
  int port;
  std::string RcvAddr;

  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  
  struct sigaction sa = {};
  sa.sa_handler = signalHandlerForMain;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);

  while (true) {
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    char buf[100];
    int rlen = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&src, &srclen);
    // std::cout<<"echoing message"<<std::endl;
    buf[rlen] = 0;
    RcvdData += buf;
    // Convert the source address to a human-readable form
    inet_ntop(AF_INET, &(src.sin_addr), ipstr, sizeof(ipstr));
    // Convert the source port to host byte order
    port = ntohs(src.sin_port);
    RcvAddr = ipstr;
    // std::cout<<"received from port = "<<ntohs(src.sin_port)<<" and server addr = "<<RcvAddr<<std::endl;
    RcvAddr = RcvAddr + ":" + std::to_string(port);
    dest.sin_port = htons(port);/*UDP port*/
    inet_pton(AF_INET,ipstr, &(dest.sin_addr));
    bool IsFellowServer = CheckServerOrClient(RcvAddr);
    if(true == DebugFlag)
    {
      std::cout<<"<"<<RcvAddr<<">"<<RcvdData<<std::endl;
    }
    /*If not server then check if new client connected*/
    if(false == IsFellowServer)
    {
      // std::cout<<"Message received from client"<<RcvdData<<std::endl;
      /*If new client connection, initialize the chat room to -1(invalid chat room value)*/
      if(ClientAddrInfoMap.count(RcvAddr) == 0)
      {
        ClientConnected.push_back(RcvAddr);
        ClientAddrInfoMap[RcvAddr].ChatRoom = -1;
        ClientAddrInfoMap[  RcvAddr].NickName = "";
        // std::cout<<"New client connected with ip*"<<ClientConnected[ClientConnected.size()-1]<<"*"<<std::endl;
      }
      /*Check if command or data*/
      IsCommand = DecodeClientData(RcvdData,Command,Data);
      /*If command then respond accordingly*/
      if(true == IsCommand)
      {
        if(Command == "JOIN" || Command == "join")
        {
          if(-1 == ClientAddrInfoMap[RcvAddr].ChatRoom)
          {
            ClientAddrInfoMap[RcvAddr].ChatRoom = atoi(Data.c_str());
            string m = "+OK Joined chat room *"+Data+"*";
            // std::cout<<"Sending message to client *"<<std::endl;
            sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
            if(true == DebugFlag)
            {
               std::cout<<"<Server Resp>"<<m<<std::endl;
            }
          }  
          else
          {
            string m = "-ERR Already in chat room *"+Data+"*";
            // std::cout<<"Sending message to client *"<<std::endl;
            sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
            // std::cout<<"Already in a chat room"<<std::endl;
            if(true == DebugFlag)
              std::cout<<"<Server Resp>"<<m<<std::endl;
          }
        }  
        else if(Command == "NICK" || Command == "nick")
        {
          ClientAddrInfoMap[RcvAddr].NickName = Data;
          string m = "+OK Nick name set to *"+Data+"*";
          sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
          if(true == DebugFlag)
            std::cout<<"<Server Resp>"<<m<<std::endl;
        }
        else if(Command == "QUIT" || Command == "quit")
        {
          ClientAddrInfoMap.erase(RcvAddr);
          string m = "+OK Bye";
          sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
          if(true == DebugFlag)
            std::cout<<"<Server Resp>"<<m<<std::endl;
        }
        else if(Command == "PART" || Command == "part")
        {
          if(atoi(Data.c_str()) == ClientAddrInfoMap[RcvAddr].ChatRoom)
          {
            ClientAddrInfoMap[RcvAddr].ChatRoom = -1;
            string m = "+OK Left chat room "+Data;
            sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
            if(true == DebugFlag)
              std::cout<<"<Server Resp>"<<m<<std::endl;
          }  
          else
          {
            // std::cout<<"Not in the chat room"<<std::endl;
            string m = "-ERR Not in the chat room "+Data;
            sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
            if(true == DebugFlag)
              std::cout<<"<Server Resp>"<<m<<std::endl;
          }
        }  
        else
        {

          // std::cout<<"Invalid command"<<std::endl;
          string m = "-ERR Not in the chat room "+Data;
          sendto(sock, m.c_str(), m.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
          if(true == DebugFlag)
            std::cout<<"<Server Resp>"<<m<<std::endl;
        }
      } 
      /*If not command then multicast the message to other servers and local client from same group*/
      else
      {
        if(UNORDERED == eOrdering)
        {
          // std::cout<<"unordered multicast"<<std::endl;
          Message m;
          m.groupID = ClientAddrInfoMap[RcvAddr].ChatRoom;
          m.senderID = ServerId;
          m.msgID = S[m.groupID]++;
          m.MessageContent =  std::to_string(m.groupID) + '/' + std::to_string(ServerId) + '/' + std::to_string(m.msgID) + '/' + RcvdData;
          B_multicast(m.groupID,m);
          B_deliver(m.groupID,RcvdData);
        }
        else if(FIFO == eOrdering)
        {
          // std::cout<<"FIFO multicast"<<std::endl;
          FO_multicast(ClientAddrInfoMap[RcvAddr].ChatRoom, RcvdData);
          B_deliver(ClientAddrInfoMap[RcvAddr].ChatRoom, RcvdData);
        }
        else if(TOTAL == eOrdering)
        {
          // std::cout<<"Total multicast"<<std::endl;
          Message m;
          m.groupID = ClientAddrInfoMap[RcvAddr].ChatRoom;
          m.senderID = ServerId;
          m.msgID = S[m.groupID]++;
          m.MessageContent = std::to_string(m.groupID) + '/' + std::to_string(m.senderID) + '/' + std::to_string(m.msgID) + '/' + '*' + '/' + to_string( ProposalCount[m.groupID][m.msgID] + 1) + '/' + RcvdData;
          TotalOrder_holdback[m.groupID].push_back(make_tuple(ProposedNum[m.groupID][m.msgID].first,ServerId,m.msgID,RcvdData));
          B_multicast(ClientAddrInfoMap[RcvAddr].ChatRoom,m);

        }
      }
    
    }
    else
    {
      // std::cout<<"Message received from server "<<RcvAddr<<std::endl;
      if( ClientAddrInfoMap[RcvAddr].ChatRoom != -1)
      {
        if(UNORDERED == eOrdering)
        {
          Message m;
          MessageDecode(RcvdData,m);
          B_deliver(m.groupID,m.MessageContent);
        }
        else if(FIFO == eOrdering)
        {
          Message m;
          MessageDecode(RcvdData,m);
          FO_deliver(m.groupID,m);
        }
        else if(TOTAL == eOrdering)
        {
          Message m;
          ePROPOSE_STATE_E eProposeState = DecodeTotalOrderingMessage(RcvdData,m);
          // std::cout<<"Propose state = "<<eProposeState<<std::endl;
          if(PROPOSE_PROMPT == eProposeState)
          {
            /*Request to propose an order for the given message for the given group*/
            S[m.groupID]++;
            ProposalCount[m.groupID][m.msgID] = 0;
            ProposedNum[m.groupID][m.msgID].second = ServerId;
            ProposedNum[m.groupID][m.msgID].first = std::max(LastProposed[m.groupID],LastAccepted[m.groupID]) + 1;
            LastProposed[m.groupID] = ProposedNum[m.groupID][m.msgID].first;
            TotalOrder_holdback[m.groupID].push_back(make_tuple(ProposedNum[m.groupID][m.msgID].first,ServerId,m.msgID,m.MessageContent));
            m.MessageContent =  "?/" + std::to_string(ProposedNum[m.groupID][m.msgID].first) + '/' + m.MessageContent;
            /*Respond back to the server that had requested ordering proposal*/
            m.MessageContent = std::to_string(m.groupID) + '/' + std::to_string(ServerId) + '/' + std::to_string(m.msgID) + '/' + m.MessageContent;
            sendto(sock, m.MessageContent.c_str(), m.MessageContent.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
          }
          else if(PROPOSE_MADE == eProposeState)
          {

            // std::cout<<"Propose made by server"<<ProposedNum[m.groupID][m.msgID].second<<" is "<<ProposedNum[m.groupID][m.msgID].first<<std::endl;
            // std::cout<<"Max proposed number for the group"<<AcceptedNum[m.groupID][m.msgID].first<<std::endl;
            ProposalCount[m.groupID][m.msgID] += 1;
            if((ProposedNum[m.groupID][m.msgID].first) < (m.MessageOrder))
            {              
              AcceptedNum[m.groupID][m.msgID].first = m.MessageOrder;
              AcceptedNum[m.groupID][m.msgID].second = m.senderID;
              
            }
            if(ProposalCount[m.groupID][m.msgID] == FwdAddress.size())
            {
              LastAccepted[m.groupID] = AcceptedNum[m.groupID][m.msgID].first;
              m.MessageContent = std::to_string(m.groupID) + '/' + std::to_string(AcceptedNum[m.groupID][m.msgID].second) + '/' + std::to_string(m.msgID) + "/!/" + std::to_string(AcceptedNum[m.groupID][m.msgID].first) + '/' +  m.MessageContent;
              B_multicast(m.groupID,m);
              TotalO_deliver(m.groupID,m);
            }
            // std::cout<<"Propose made by "<<ProposalCount[m.groupID][m.msgID]<<"servers"<<std::endl;
          }
          else if(PROPOSE_ACCEPT == eProposeState)
          {
            // std::cout<<"Total ordering delivery to make"<<std::endl;
            ProposalCount[m.groupID][m.msgID] = FwdAddress.size();
            TotalO_deliver(m.groupID,m);
          }
        }
      }
      else
      {
      }
    }
    RcvdData.clear();
  }
}

