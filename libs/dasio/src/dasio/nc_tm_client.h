#ifndef NC_TM_CLIENT_H_INCLUDED
#define NC_TM_CLIENT_H_INCLUDED
/** @file nc_tm_client.h
 * Header file for ncurses TM clients
 */

#include "dasio/tm_client.h"

namespace DAS_IO {
  
  class nc_tm_client : public tm_client {
    public:
      inline nc_tm_client(int bufsize, bool fast = true) : tm_client(bufsize, fast) {}
      void process_data();
  };

}

#endif
