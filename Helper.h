#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <bits/stdc++.h>
#include <fstream>


typedef enum
{
  UNORDERED,
  FIFO,
  TOTAL
}eOrdering_E;


bool DecodeClientData(std::string ClientData, std::string &Command, std::string &Data);
bool handle_options(int argc, char *argv[],eOrdering_E &eOrdering);

#undef HELPER_H
#endif // HELPER_H