#include <time.h>
#include <stdint.h>

/* #define NDEBUG */
#define NTHREADS 10
#define MAX_PENDING 16
#define POLL_TIMEOUT 6000
#define MAX_CON 10
#define MAX_MESSAGE 2048
#define MAX_FILE 1016
#define MAX_NAMEPASS USHRT_MAX

#define USER_OP 0x1
#define DEL_OP 0x2,
#define LS_OP 0x3
#define GET_OP 0x4
#define MK_OP 0x5
#define PUT_OP 0x6

#define SUCCESS 0x1
#define SES_ERR 0x2
#define PERM_ERR 0x3
#define USR_EXIST 0x4
#define FILE_EXIST 0x5
#define FAIL 0xff

#define USR_LGN 0x0
#define USR_CRO 0x1
#define USR_CRW 0x2
#define USR_CAD 0x3
#define USR_DEL 0xff


/**
 * @brief struct for keeping track of individual user accounts
 *
 * @attr name - user name terminated by \0
 *
 * @attr perms - char that indicates what permissions a user has;
 *               'r' read-only; 'w' read-write; 'a' admin
 *
 */
typedef struct _user user;
struct _user
{
  char name[MAX_NAMEPASS];
  char pass[MAX_NAMEPASS];
  char perms;
};

/**
 * @brief struct for encapsulating the user list and a mutex for operations
 *        on the list
 *
 * @attr usrlist - linked list of user accounts
 *
 * @attr lock - mutex to prevent deadlock, race conditions, etc

 *
 */
typedef struct _user_list user_list;
struct _user_list
{
  ll* usrlist;
  pthread_mutex_t lock;
};


/**
 * @brief struct for keeping track of individual sessions
 *
 * @attr sesid - session ID
 *
 * @attr last_used - time the session was last used
 *
 * @attr expired - indicates if session is already expired (1 if yes 0 if no)
 *
 * @attr usr - pointer to user the session is associated with
 */
typedef struct _session session;
struct _session
{
  unsigned int sesid;
  time_t last_used;
  int expired;
  user *usr;
};


/**
 * @brief struct for encapsulating the user list and a mutex for operations
 *        on the list. Keeps track of next available session ID. 
 *
 * @attr sessions - the linked list of sessions
 *
 * @attr lock - mutex to prevent deadlock, race conditions, etc
 *
 * @attr cur_i - the next available session ID
 *
 */
typedef struct _session_list session_list;
struct _session_list
{
  ll *seslist;
  pthread_mutex_t lock;
  atomic_uint cur_i;
  uint timeout;
};

