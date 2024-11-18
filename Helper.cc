#include "Helper.h"


/*Function to decode the client data*/
bool DecodeClientData(std::string ClientData, std::string &Command, std::string &Data)
{
  if(ClientData.find("/") != 0)
  {
    std::cout<<"Not a command"<<std::endl;
    return false;
  }
  else 
  {
    std::cout<<"Checking for command"<<ClientData<<std::endl;
    Command = ClientData.substr(1,ClientData.find(" ")-1);
    Data = ClientData.substr(ClientData.find(" ")+1, ClientData.length() - Command.length() - 1);
    if((Command == "JOIN") || (Command == "join"))
    {
      std::cout<<"JOIN *"<<Data<<"* chat room"<<std::endl;
    }
    else if((Command == "PART") || (Command == "part"))
    {
      std::cout<<"Part *"<<Data<<"* chat room"<<std::endl; 
    }
    else if((Command == "NICK") || (Command == "nick"))
    {
      std::cout<<"Change nickname to *"<<Data<<"*"<<std::endl;
    }
    else if((Command == "QUIT") || (Command == "quit"))
    {
      std::cout<<"Remove the client connection"<<std::endl;
    }
    return true;
  }
}

bool handle_options(int argc, char *argv[], eOrdering_E &eOrdering)
{
  int opt;
  bool debug = false;
  while((opt = getopt(argc,argv,":vo:")) != -1)
  {
    switch(opt)
    {
      case 'v':
      {
        debug = true;
      }
      break;
      case 'o':
      {
        std::cout<<"Ordering = "<<optarg<<std::endl;
        if(strcmp(optarg,"unordered") == 0)
        {
          eOrdering = UNORDERED;
          std::cout<<"Unordered"<<std::endl;
        }
        else if(strcmp(optarg,"fifo") == 0)
        {
          eOrdering = FIFO;
          std::cout<<"FIFO"<<std::endl;
        }
        else if(strcmp(optarg,"total") == 0)
        {
          eOrdering = TOTAL;
          std::cout<<"Total"<<std::endl;
        }
      } 
      break;
     
    }
  }
  return debug;
}