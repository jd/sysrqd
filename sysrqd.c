#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include "sysrqd.h"

char pwd[PASS_LEN];

/* Macro used to write the prompt */
#define write_prompt(s) \
  write (s, PROMPT, sizeof(char) * strlen(PROMPT));

/* Macro used to write a string to the client */
#define write_cli(sock, s) \
  write (sock, s, sizeof(char) * strlen(s));


/* Authenticate the connection */
int
auth (int sock_client)
{
  char buf[PASS_LEN];
  
  write_cli (sock_client, "sysrqd password: ");
  read (sock_client, buf, PASS_LEN);
  if(!strncmp(buf, pwd, strlen(pwd)))
    return 1;
  
  write_cli (sock_client, "Damn it!\r\n");
  return 0;
}

/* Read commands */
void
read_cmd (int sock_client, int fd_sysrq)
{
  char buf[1];
  
  write_prompt (sock_client);
  do
    {
      if(buf[0] == '\n')
	write_prompt (sock_client);

      if((buf[0] >= 48 && buf[0] <= 57) ||
	 (buf[0] >= 97 && buf[0] <= 122 ))
	write(fd_sysrq, buf, 1);
    }
  while(read (sock_client, buf, 1) && buf[0] != 'q');
}


/*
 * Listen to a port,
 * authenticate connection
 * and execute commands
*/
int
start_listen (int fd_sysrq)
{
  int sock_serv, sock_client;
  socklen_t size_addr;
  struct sockaddr_in addr;
  struct sockaddr_in addr_client;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SYSRQD_LISTEN_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  sock_serv = socket (PF_INET, SOCK_STREAM, 0);

  bind (sock_serv, (struct sockaddr *) &addr, sizeof (addr));
  listen(sock_serv, 2);

  size_addr = sizeof (addr_client);

  while((sock_client = accept (sock_serv, (struct sockaddr *) &addr_client, &size_addr)))
    {
      if(auth (sock_client))
	read_cmd (sock_client, fd_sysrq);
      close(sock_client);
    }
  
  close (sock_serv);

  return 0;
}

int
open_sysrq_trigger (void)
{
  int fd;
  fd = open (SYSRQ_TRIGGER_PATH, O_SYNC|O_WRONLY);
  return fd;
}

/* Read the sysrqd password */
void
read_pwd (void)
{
  int fd_pwd;
  fd_pwd = open (AUTH_FILE, O_RDONLY);

  if(!fd_pwd)
    return;
  
  read (fd_pwd, pwd, PASS_LEN);
}

int
main (void)
{
  int fd_sysrq;

  /* We set our priority */
  setpriority (PRIO_PROCESS, 0, SYSRQD_PRIO);

  /* We keep the sysrq-trigger file opened */
  fd_sysrq = open_sysrq_trigger ();

  /* We read our password */
  read_pwd ();

  /* Now listen*/
  start_listen (fd_sysrq);
  
  /* If we quit, close the trigger */
  close (fd_sysrq);

  return (EXIT_SUCCESS);
}
