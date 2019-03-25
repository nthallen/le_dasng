/** @file dasio/server.h */
#ifndef DASIO_SERVER_H_INCLUDED
#define DASIO_SERVER_H_INCLUDED

#include <map>
#include <string>
#include "socket.h"
#include "loop.h"

namespace DAS_IO {

  class SubService;
  class Authenticator;
  class Server;
  class Serverside_client;
  
  /**
   * Defines a function type for switching interface classes after
   * negotiation. The function can return zero if the connection is
   * refused for reasons beyond the authentication. For example, the
   * TM_server class will refuse a new connection if the specified
   * datum is already being written to by another process.
   * @return Pointer to a Socket subclass or zero if an error occurs.
   */
  typedef Serverside_client *(* socket_clone_t)(Authenticator *, SubService *);
  
  class SubService {
    public:
      /**
       * @param svcs The service/subservice string
       * @param func Factory function to the appropriate Socket subclass
       * @param svc_data subservice-specific data reference
       */
      SubService(std::string name, socket_clone_t func, void *svc_data);
      ~SubService();
      std::string name;
      socket_clone_t func;
      void *svc_data;
  };
  
  class SubServices {
    public:
      SubServices();
      ~SubServices();
      /**
       * @return true if the subservice was added successfully, false if
       * it was already defined.
       */
      bool add_subservice(SubService *def);
      /**
       * @param subservice The service/subservice string
       * @return true if the subservice was removed successfully, false if
       * it was not defined.
       */
      bool rm_subservice(std::string subservice);
      SubService *find_subservice(std::string subservice);
    private:
      std::map<std::string,SubService *> subs;
  };
  
  /**
   * A Server_socket object sets up a listener of the designated socket_type_t.
   * When a connection is accepted, it creates and Authenticator
   * object to handle the negotiation. The Authenticator uses
   * subservice information from the negotiation to decide what type
   * of object to transform into.
   *
   * A server application then needs to provide socket_clone_t
   * function(s) that do what is necessary.
   *
   * Subsp is a pointer here because we expect the list of SubServices
   * to be shared between two server sockets (Unix & TCP) in many
   * cases.
   */
  class Server_socket : public Socket {
    friend class Authenticator;
    public:
      Server_socket(const char *iname, const char *service,
        Socket::socket_type_t socket_type, Server *srvr);
      ~Server_socket();
      Socket *new_client(const char *iname, int fd);
    protected:
      Server *srvr;
  };
  
  class Serverside_client : public Socket {
    public:
      Serverside_client(Authenticator *orig, const char *iname, int ibufsize);
      ~Serverside_client();
    private:
      Server *srvr;
  };

  class Authenticator : public Socket {
    friend class Serverside_client;
    public:
      Authenticator(Server_socket *orig, const char *iname, int fd);
      ~Authenticator();
      /**
       * Currently processes a single line of input, and either accepts
       * or rejects.
       * @return true if the server's event loop should terminate
       */
      bool protocol_input();
      inline const char *get_client_app() { return client_app; }
    protected:
      bool not_word(const char *&w, int &len);
      bool not_svc(const char *&svc, int &len);
      Server *srvr;
      const char *client_app;
  };
  
  class Server {
    friend class Authenticator;
    public:
      Server(const char *service, int passive_quit_threshold = 0);
      ~Server();
      
      inline bool add_subservice(SubService *ss) {
        return Subs.add_subservice(ss);
      }
      inline bool rm_subservice(std::string svcs) {
        return Subs.rm_subservice(svcs);
      }
      
      typedef enum { Srv_Unix = 1, Srv_TCP = 2, Srv_Both = 3 } Srv_type;
      /**
       * Creates the specified server sockets, prints a startup message,
       * runs the event loop, and then prints a termination message.
       */
      void Start(Srv_type which);
      /**
       * Initiates the server shutdown sequence, closing the listening
       * sockets and signaling the Loop to close all connections.
       */
      void Shutdown();
      /** Called by Serverside_client objects on creation.
       *  Increments the client count.
       */
      void client_added();
      /** Called by Serverside_client objects on deletion.
       *  Decrements the client count and may trigger shutdown of
       *  the server.
       */
      void client_removed();
      /** Sets the passive exit threshold.
       * @param N The new passive exit threshold value
       * If N is positive, the server will shutdown after at
       * least N total_clients have connected and all clients have
       * disconnected.
       */
      void set_passive_exit_threshold(int N);
      Server_socket *Unix;
      Server_socket *TCP;
      Loop ELoop;
      inline const char *get_service() { return service; }
    protected:
      /** The number of currently connected clients */
      int active_clients;
      /** The total number of clients that have connected */
      int total_clients;
      /** See set_passive_exit_threshold() */
      int passive_exit_threshold;
      SubServices Subs;
      const char *service;
  };

}

#endif