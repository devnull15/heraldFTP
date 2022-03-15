#include <threadpool.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>

/**
 * @brief prints command line usage information, separated from main to reduce clutter
 */
static void usage(void);



int main(int argc, char **argv) {
  int ret = 0;
  uint timeout = 0;
  char *serv_dir;
  uint port = 0;
  char c = 0;
  char *err = NULL;


  if(7 != argc) {
    usage();
    ret = -1;
    goto ERR;
  }

  while((c = getopt(argc,argv, "t:d:p:")) != -1) {
    switch (c)
      {
      case 't': // timeout will default to UINT_MAX if number overflows a long
	timeout = strtoul(optarg, &err, 10);
	if(0 != *err) {
	  fprintf(stderr, "Invalid value for -t <timeout_seconds>\n");
	  ret = -1;
	  goto ERR;	  
	}
	break;
      case 'd':
	//check if valid directory?
	serv_dir = optarg;
        break;
      case 'p':
	port = strtoul(optarg, &err, 10);	  
	if(0 != *err || port > 0 || port > USHRT_MAX) {
	  fprintf(stderr, "Invalid value for -p <port_number>\n");
	  ret = -1;
	  goto ERR;	  
	}
        break;
      case '?':
        if (optopt == 't' || optopt == 'd' || optopt == 'p') {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	}
        else {
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

  printf("t = %u / d = %s / p = %hu\n", timeout, serv_dir, port);
  
 ERR:
    return ret;
}

static void usage(void) {
  fprintf(stderr,
	  "Usage: ./capstone -t <timeout_seconds> -d <path_to_server_folder> -p <listening_port>\n"
	  );
}
