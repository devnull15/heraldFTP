#include <threadpool.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <netpoll.h>
#include <unistd.h>
#include <signal.h>
#include <server.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdatomic.h>



/**
 * @brief prints command line usage information, separated from main to reduce
 * clutter
 */
static void usage(void);

/**
 * @brief sighandler to catch SIGINT and shutdown gracefully
 *
 * @param signal number
 */
static void inthandler(int signo);

/**
 * @brief revent handler passed to netpoll
 *
 * @param sfd - socket that triggered event
 *
 * @param args - arguments for the revent handler in this case:
 *        args[0] - session_list *sessions
 *        args[1] - user_list *users
 *
 */
static void rh(int sfd, void **args);

/**
 * @brief parses data from client and generates a response message
 *
 * @param recv - data from client
 *
 * @param snd - buffer where reponse to client is to be stored
 *
 * @returns 0 on success -1 on error
 *
 */
static int parsecmd(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions);

/**
 * @brief parses the user command and sends to appropriate function based on user flag code
 *
 * @param recv - data from client
 *
 * @param snd - buffer where reponse to client is to be stored
 *
 * @param users - list of user accounts
 *
 * @param sessions - list of active and inactive sessions
 *
 *
 * @returns 0 on success -1 on error
 *
 */
static int parseusercmd(unsigned char *recv, unsigned char *snd,  user_list *users, session_list *sessions);

/**
 * @brief process a user login request; generates a success or failure message
 *        based on a check of the username and password in the global users list
 *
 * @param recv - data from client
 *
 * @param snd - buffer where reponse to client is to be stored
 *
 * @param users - list of user accounts
 *
 * @param sessions - list of active and expired sessions
 *
 * @returns 0 on success -1 on error
 *
 */
static int userlogin(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions);

/**
 * @brief process a user delete request; generates a success or failure message
 *        based on the permissions of the requesting user and the user account to be
 *        deleted
 *
 * @param recv - data from client
 *
 * @param snd - buffer where reponse to client is to be stored
 *
 * @param users - list of user accounts
 *
 * @param sessions - list of active and expired sessions
 *
 * @returns 0 on success -1 on error
 *
 */
static int userdelete(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions);


/**
 * @brief checks to see if username-password combo is in the users list; this
 *        function in itself is not thread safe, a lock should be set before
 *        calling this function in a threaded application
 *
 * @param user - username
 *
 * @param pass - password
 *
 * @param users - list of user accounts
 *
 *
 * @returns pointer to authenticated user, or NULL if failed/error
 *
 */
static user* authenticate(char *user, char *pass, user_list *users);

/**
 * @brief adds session to the session list
 *
 * @param usr - pointer to the user object associated with the session
 *
 * @param sessions - list of active and expired sessions
 *
 * @returns 0 on success -1 if not found
 *
 */
static session *add_session(user *usr, session_list *sessions);

/**
 * @brief adds user to the user list; note that new_usr should be alloc'd by the
 *        caller and then freed ONLY IF add_user fails (-1 return); the account
 *        if successfully added will be freed by ll_destroy() when the server is closed
 *
 * @param users - user list
 *
 * @param req_usr - user requesting account to be added
 * 
 * @param new_usr - user account to be added
 *
 * @returns 0 on success -1 if failed
 *
 */
static int add_user(user_list *users, user *req_usr, user *new_usrr);


/**
 * @brief creates a user of the specified permissions given the user flag code in the
 *        @param recv'd messge
 *
 * @param recv - data from client
 *
 * @param snd - buffer where reponse to client is to be stored
 *
 * @param users - list of user accounts
 *
 * @param sessions - list of active and expired sessions
 *
 * @returns 0 on success -1 on error
 *
 */
static int usercreatecmd(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions);

/**
 * @brief checks session ID to see if it has expired
 *
 * @param sesid - session ID to check
 *
 * @param sessions - list of active and expired sessions
 *
 * @returns user associated with session, NULL on error
 *
 */
static user *check_sesid(unsigned int sesid, session_list *sessions);

/**
 * @brief updates the time stamp for a session and marks as expired if necessary
 *
 * @param s - pointer to a session to be updated
 *
 * @param timeout - time in seconds 
 *
 * @returns nothing
 *
 */
static void update_time(session *s, uint timeout);

/**
 * @brief node free function for linked list
 *
 * @param n - node
 *
 * @returns nothing
 *
 */
static void ll_free(void *p);

/**
 * @brief checks if the given permission level can perform the action
 *        specified in op.
 *
 * @param op - OPCODE of the server action to be performed by the caller
 *
 * @param perms - permissions of the user requesting the action
 * 
 * @returns 0 on action allowed, -1 not allowed
 *
 */
static int check_perms(int op, char perms);

/**
 * @brief checks if user account exists for the given name
 *
 * @param name - username to be checked
 *
 * @returns 0 on action allowed, -1 not allowed
 *
 */
static int is_user(char *name, user_list *users);


int
main(int argc, char **argv)
{
    int   ret     = 0;
    uint  timeout = 0;
    char *serv_dir = NULL;
    uint  port = 0;
    char  c    = 0;
    char *err  = NULL;

    struct sigaction sigact = {0};
    sigact.sa_handler = inthandler;
    sigact.sa_flags = SA_RESTART;
    sigfillset(&sigact.sa_mask);

    ret = sigaction(SIGINT, &sigact, NULL);
    if(-1 == ret) {
      fprintf(stderr, "! signalaction failed\n");
      goto ERR;
    }


    if (7 != argc)
    {
        usage();
        ret = -1;
        goto ERR;
    }

    while ((c = getopt(argc, argv, "t:d:p:")) != -1)
    {
        switch (c)
        {
            case 't': // timeout will default to UINT_MAX if number overflows a
                      // long
                timeout = strtoul(optarg, &err, 10);
                if (0 != *err)
                {
                    fprintf(stderr, "! Invalid value for -t <timeout_seconds>\n");
                    ret = -1;
                    goto ERR;
                }
                break;
            case 'd':
                // check if valid directory?
                serv_dir = optarg;
                break;
            case 'p':
                port = strtoul(optarg, &err, 10);
                if (0 != *err ||  port > USHRT_MAX)
                {
                    fprintf(stderr, "! Invalid value for -p <port_number>\n");
                    ret = -1;
                    goto ERR;
                }
                break;
            case '?':
                if (optopt == 't' || optopt == 'd' || optopt == 'p')
                {
                    fprintf(
                        stderr, "Option -%c requires an argument.\n", optopt);
                }
                else
                {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                usage();
                ret = -1;
                goto ERR;
            default:
                usage();
                ret = -1;
                goto ERR;
        }
    }

    printf("timeout = %u / server dir = %s / port = %hu\n", timeout, serv_dir, port);

    
    session_list *sessions = NULL;    
    sessions = calloc(1, sizeof(session_list));
    if(NULL == sessions) {
      fprintf(stderr, "! calloc session_array failed\n");
      ret = -1;
    goto ERR;
  }

    sessions->seslist = ll_init();
    if(NULL == sessions->seslist) {
      fprintf(stderr, "! init'ing users list failed\n");
      ret = -1;
      goto ERR;
    }

    sessions->timeout = timeout;

    user_list *users = NULL;
    users = calloc(1, sizeof(user_list));
    if(NULL == users) {
      fprintf(stderr, "! init'ing users list failed\n");
      ret = -1;
      goto ERR;
    }

    users->usrlist = ll_init();
    if(NULL == users->usrlist) {
      fprintf(stderr, "! init'ing users list failed\n");
      ret = -1;
      goto ERR;
    }

    user *admin = calloc(1, sizeof(user));
    char *admin_u = "admin";
    char *admin_p = "password";
    memcpy(admin->name, admin_u, strlen(admin_u));
    memcpy(admin->pass, admin_p, strlen(admin_p));
    admin->perms = 'a';
    if(NULL == admin) {
      fprintf(stderr, "! calloc admin list failed\n");
      ret = -1;
      goto ERR;
    }
    
    ret = push_front(users->usrlist, (void*)admin, ll_free);
    if(0 != ret) {
      fprintf(stderr, "! init'ing users list failed\n");
      goto ERR;
    }

    ret = push_front(users->usrlist, (void*)admin, free);


    int sockfd = 0;
    sockfd = tcp_socketsetup(port, AF_INET, MAX_PENDING);
    if(-1 == sockfd) {
      fprintf(stderr, "! tcp_socketsetup failed\n");
      ret = -1;
      goto ERR;
    }

    char *args[2] = {0};
    args[0] = (char*)sessions;
    args[1] = (char*)users;
    
    ret = tcp_netpoll(sockfd, rh, MAX_CON, POLL_TIMEOUT, (void**)args);
 
ERR:
    if(NULL != users) {
      ll_destroy(users->usrlist);
      free(users);
      users = NULL;
    }
    if(NULL != sessions) {
      ll_destroy(sessions->seslist);
      free(sessions);
      sessions = NULL;
    }
    //free(admin);
    //admin = NULL;
    return ret;
}

static void
usage(void)
{
    fprintf(stderr,
            "Usage: ./capstone -t <timeout_seconds> -d <path_to_server_folder> "
            "-p <listening_port>\n");
}


static void inthandler(int signo) {
  fprintf(stderr, "SIGINT\n");
  if(SIGINT == signo) {
    netpoll_keepalive = 0;
  }
  
}


static int parsecmd(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions) {
  int ret = 0;

  if(NULL == recv) {
    fprintf(stderr, "! parsecmd: no data received\n");
    ret = -1;
    goto ERR;
  }
  
  if(NULL == snd) {
    fprintf(stderr, "! parsecmd: NULL send buffer\n");
    ret = -1;
    goto ERR;
  }


  char op = recv[0];
  switch(op) {
  case USER_OP:
    ret = parseusercmd(recv,snd, users, sessions);
    break;
  default:
    fprintf(stderr, "! opcode not recognized\n");
    ret = -1;
  }

 ERR:
  return ret;
}

static int
parseusercmd( unsigned char *recv, unsigned  char *snd, user_list *users, session_list *sessions) {
  int ret = 0;
  
  unsigned char op = recv[1];
  
  switch(op) {
  case USR_LGN:
    userlogin(recv,snd, users, sessions);
    break;
  case USR_CRW:
  case USR_CAD:
  case USR_CRO:
    usercreatecmd(recv,snd, users, sessions);
    break;
  case USR_DEL:
    userdelete(recv,snd, users, sessions);
    break;


  default:
    fprintf(stderr, "! user opcode not recognized\n");
    ret = -1;
  }


  return ret;
}

static int userlogin(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions) {
  int ret = 0;
  user *usr = NULL;
  unsigned short namelen = 0;
  unsigned short passlen = 0;
  memcpy(&namelen, &recv[4], 2);
  memcpy(&passlen, &recv[6], 2);
  namelen = ntohs(namelen);
  passlen = ntohs(passlen);
  char name[MAX_NAMEPASS] = {0};
  char pass[MAX_NAMEPASS] = {0};
  memcpy(name, &recv[12], namelen);
  memcpy(pass, &recv[12+namelen], passlen);

#ifndef NDEBUG
  fprintf(stderr, "username:%s(%d) password:%s(%d)\n",name,namelen,pass,passlen);
#endif /* NDEBUG */
  usr = authenticate(name,pass,users);

  if(NULL == usr) {
    fprintf(stderr, "! auth failed\n");
    snd[0] = FAIL;
    goto ERR;
  }

  session *ses = add_session(usr,sessions);
  if(NULL == ses) {
    snd[0] = FAIL;
    ret = -1;
    goto ERR;
  }
    
  snd[0] = SUCCESS;
  int sesid = htonl(ses->sesid);
  memcpy(&snd[2], &sesid, 4);
  
  
  
 ERR:
  return ret;
}

static int
usercreatecmd(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions)
{
  int ret = 0;
  int sesid = 0;
  memcpy(&sesid, &recv[8], 4);
  sesid = ntohl(sesid);

  user *new_usr = NULL;
  user *usr = NULL;
  usr = check_sesid(sesid, sessions);
  if (NULL == usr) {
    fprintf(stderr, "! Invalid/expired session ID\n");
    snd[0] = SES_ERR;
    goto ERR;
  }

  int op = 0;
  memcpy(&op, &recv[1], 1);
  ret = check_perms(op, usr->perms);
  if(0 != ret) {
    ret = -1;
    snd[0] = FAIL;
    fprintf(stderr, "! insufficient permissions\n");
    goto ERR;
  }


  new_usr = calloc(1, sizeof(user));
  unsigned short namelen = 0;
  unsigned short passlen = 0;
  memcpy(&namelen, &recv[4], 2);
  memcpy(&passlen, &recv[6], 2);
  namelen = ntohs(namelen);
  passlen = ntohs(passlen);
  memcpy(new_usr->name, &recv[12], namelen);
  memcpy(new_usr->pass, &recv[12+namelen], passlen);
  switch(op) {
  case USR_CRO:
    new_usr->perms = 'r';
    break;
  case USR_CRW:
    new_usr->perms = 'w';
    break;
  case USR_CAD:
    new_usr->perms = 'a';
    break;
  default:
    fprintf(stderr, "Unrecognized user flag\n");
    ret = -1;
    goto ERR;
  }
#ifndef NDEBUG
  fprintf(stderr, "username:%s(%d) password:%s(%d)\n",new_usr->name,namelen,new_usr->pass,passlen);
#endif /* NDEBUG */

  ret = add_user(users, usr, new_usr);
  if(0 == ret) {
    snd[0] = SUCCESS;
  }
  else {
    snd[0] = FAIL;
    goto ERR;
  }

  new_usr = NULL;
 ERR:
  free(new_usr);
  new_usr = NULL;
  return ret;
  
} 


static user * authenticate(char *name, char *pass, user_list *users) {
  node *n = ll_get(users->usrlist, 0);
  user *usr = NULL;

  if(NULL == name || NULL == pass || NULL == users) {
    fprintf(stderr, "! authenticate: can't have NULL arguments\n");
    usr = NULL;
    goto ERR;
  }
  
  while(NULL != n) {
    usr = (user*)(n->data);

    if((0 == strcmp(name, usr->name)) &&
       (0 == strcmp(pass, usr->pass))) {
      break;
    }
    n = n->next;
    usr = NULL;
  }
  
 ERR:
  return usr;
}

static session *add_session(user *usr, session_list *sessions) {
  session *ret = NULL;
  session *ses = calloc(1, sizeof(session));
  int err = 0;
  
  if(NULL == usr || NULL == sessions) {
    fprintf(stderr, "! add_session: can't have NULL arguments\n");
    goto ERR;
  }

  if(UINT_MAX == sessions->cur_i) {
    fprintf(stderr, "! out of session IDs");
    goto ERR;
  }
  
  ses->sesid = sessions->cur_i;
  sessions->cur_i++;
  ses->last_used = time(NULL);
  ses->usr = usr;

  err = push_front(sessions->seslist, (void*)ses, ll_free);
  if(0 != err) {
    fprintf(stderr, "! init'ing users list failed\n");
    goto ERR;
  }
  ret = ses;
  ses = NULL;
 ERR:
  free(ses);
  ses = NULL;
  return ret;
}

void rh(int sfd, void** args) {
  unsigned char recv[MAX_MESSAGE] = {0};
  unsigned char snd[MAX_MESSAGE] = {0};
  int err = 0;
  char **argv = (char**)args;
  session_list *sessions = (session_list*)argv[0];
  user_list *users = (user_list*)argv[1];

  tcp_read_handler(sfd, recv, MAX_MESSAGE);
  
#ifndef NDEBUG
  printf("[<] %s", recv);
#endif /* NDEBUG */

  err = parsecmd(recv,snd, users, sessions);
  if(0 != err) {
    fprintf(stderr, "! error parsing, sending FAIL\n");
    snd[0] = FAIL;
  }

  

#ifndef NDEBUG
  printf("[>] %s", snd);
#endif /* NDEBUG */
  tcp_write_handler(sfd, (char*)snd, MAX_MESSAGE);

  return;

}

static user *check_sesid(unsigned int sesid, session_list *sessions) {  
  node *n = ll_get(sessions->seslist, 0);
  session *s = NULL;
  user *usr = NULL;
  
  while(NULL != n) {
    
    s = (session*)(n->data);

    if(s->sesid == sesid) {
      update_time(s, sessions->timeout);
      if(!s->expired) {
	usr = s->usr;
	break;
      }
      break;
    }
    n = n->next;
    s = NULL;
  }
  
    return usr;
}


static void update_time(session *s, uint timeout) {
  time_t now = time(NULL);
  double t = difftime(now, s->last_used);
  if(t > timeout) {
    s->expired = 1;
  }
  else {
    s->last_used = now;
  }
}

static void ll_free(void *p) {
  node *n = (node*)p;
  free(n->data);
  n->data = NULL;
  n->next = NULL;
  n->f = NULL;
  free(n);
  return;
}



static int add_user(user_list *users, user *req_usr, user *new_usr) {
  int ret = 0;

  if(NULL == req_usr || NULL == users || NULL == new_usr) {
    fprintf(stderr, "! add_user: can't have NULL arguments\n");
    ret = -1;
    goto ERR;
  }

  
  ret = push_back(users->usrlist, (void*)new_usr, ll_free);
  if(0 != ret) {
    ret = -1;
    fprintf(stderr, "! pushing new user failed\n");
    goto ERR;
  }

 ERR:
  return ret;
  
  
}

static int check_perms(int op, char perms) {
  fprintf(stderr, "op:%i perms%c\n", op, perms);
  int ret = -1;
  switch(op) {
  case USR_CRO:
    ret = 0;
    break;
  case USR_CRW:
    if('r' == perms) {
      ret = -1;
    }
    else {
      ret = 0;
    }
    break;
  case USR_CAD:
    if('a' == perms) {
      ret = 0;
    }
    else {
      ret = -1;
    }
    break;

  default:
    fprintf(stderr, "! opcode not recognized\n");
    ret = -1;
    break;
  }

  return ret;
}

static int
userdelete(unsigned char *recv, unsigned char *snd, user_list *users, session_list *sessions) {
  int ret = 0;
  if(NULL == snd || NULL == recv || NULL == users) {
    fprintf(stderr, "! userdelete: can't have NULL arguments\n");
    ret = -1;
    goto ERR;
  }

  int sesid = 0;
  memcpy(&sesid, &recv[8], 4);
  sesid = ntohl(sesid);

  user *usr = NULL;
  usr = check_sesid(sesid, sessions);
  if (NULL == usr) {
    fprintf(stderr, "! Invalid/expired session ID\n");
    snd[0] = SES_ERR;
    goto ERR;
  }

  int op = 0;
  memcpy(&op, &recv[1], 1);
  ret = check_perms(op, usr->perms);
  if(0 != ret) {
    ret = -1;
    snd[0] = FAIL;
    fprintf(stderr, "! insufficient permissions\n");
    goto ERR;
  }


  unsigned short namelen = 0;
  memcpy(&namelen, &recv[4], 2);
  namelen = ntohs(namelen);
  char name[MAX_NAMEPASS] = {0};
  memcpy(name, &recv[12], namelen);
  
#ifndef NDEBUG
  fprintf(stderr, "username:%s(%d)\n",usr->name,namelen);
#endif /* NDEBUG */

  ret = is_user(name, users);
  if(0 == ret) {
    snd[0] = SUCCESS;
  }
  else {
    snd[0] = FAIL;
    goto ERR;
  }

 ERR:
  return ret;

}


//NOT FINISHED
static int is_user(char *name, user_list *users) {
  int ret = -1;
  node *n = ll_get(users->usrlist, 0);
  user *usr = NULL;

  if(NULL == name) {
    fprintf(stderr, "! authenticate: can't have NULL arguments\n");
    usr = NULL;
    goto ERR;
  }
  
  while(NULL != n) {
    usr = (user*)(n->data);

    if(0 == strcmp(name, usr->name)) {
      ret = 0;
      break;
    }
    n = n->next;
    usr = NULL;
  }
  
 ERR:
  return ret;

}
