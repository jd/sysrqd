/*
 * sysrqd.c - Daemon to control sysrq over network
 *
 * (c) 2005 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


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
#include <signal.h>

#include "sysrqd.h"

char pwd[PASS_MAX_LEN];
int sock_serv;

/* Macro used to write the prompt */
#define write_prompt(s) \
	write (s, PROMPT, sizeof(char) * strlen(PROMPT));

/* Macro used to write a string to the client */
#define write_cli(sock, s) \
	write (sock, s, sizeof(char) * strlen(s));

#define errmsg(s) \
	fprintf(stderr, "%s (%s:%d)\n", s, __FILE__, __LINE__);

/* Authenticate the connection */
int
auth (int sock_client)
{
  char buf[PASS_MAX_LEN];
    
  write_cli (sock_client, "sysrqd password: ");
  read (sock_client, buf, PASS_MAX_LEN);
  if(!strncmp(buf, pwd, strlen(pwd)))
    return 1;
  
  write_cli (sock_client, "Go away!\r\n");
  return 0;
}

/* Read commands */
void
read_cmd (int sock_client, int fd_sysrq)
{
  char buf;
  
  write_prompt (sock_client);
  do
    {
      if(buf == '\n')
	write_prompt (sock_client);

      if((buf >= 48 && buf <= 57) ||
	 (buf >= 97 && buf <= 122 ))
	write(fd_sysrq, &buf, 1);
    }
  while(read (sock_client, &buf, 1) && buf != 'q');
}


/*
 * Listen to a port,
 * authenticate connection
 * and execute commands
*/
int
start_listen (int fd_sysrq)
{
  int sock_client;
  socklen_t size_addr;
  struct sockaddr_in addr;
  struct sockaddr_in addr_client;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SYSRQD_LISTEN_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(!(sock_serv = socket (PF_INET, SOCK_STREAM, 0)))
    {
      perror("Error while creating server socket");
      return 1;
    }

  if(bind (sock_serv, (struct sockaddr *) &addr, sizeof (addr)))
    {
      perror("Unable to bind()");
      return 1;
    }

  if(listen(sock_serv, 2))
    {
      perror("Unable to listen()");
      return 1;
    }

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
  return open (SYSRQ_TRIGGER_PATH, O_SYNC|O_WRONLY);
}

/* Read the sysrqd password */
int
read_pwd (void)
{
  int fd_pwd;
  char * tmp;
  
  if(!(fd_pwd = open (AUTH_FILE, O_RDONLY)))
    return 1;
  
  read (fd_pwd, pwd, PASS_MAX_LEN);
  close (fd_pwd);
  
  /* Strip last \n */
  if((tmp = strchr(pwd, '\n')))
    *tmp = '\0';

  return 0;
}

void
signal_handler (void)
{
  close (sock_serv);
  exit (EXIT_FAILURE);
}

int
catch_signals ()
{
  struct sigaction sa;
  sigset_t mask;

  sigfillset (&mask);

  sa.sa_handler = (void *) &signal_handler;
  sa.sa_mask = mask;
  
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);

  return 0;
}

int
main (void)
{
  int fd_sysrq;

  /* We set our priority */
  if(setpriority (PRIO_PROCESS, 0, SYSRQD_PRIO))
    {
      errmsg ("Unable to set priority.");
      return 1;
    }

  /* Catch some signals */
  catch_signals ();

  /* We keep the sysrq-trigger file opened */
  if(!(fd_sysrq = open_sysrq_trigger ()))
    {
      errmsg ("Error while opening sysrq trigger.");
      return 1;
    }

  /* We read our password */
  if(read_pwd ())
    {
      errmsg ("Error while reading password file.");
      return 1;
    }

  /* Now listen */
  if(!start_listen (fd_sysrq))
    return 1;
  
  /* If we quit, close the trigger */
  close (fd_sysrq);

  return (EXIT_SUCCESS);
}
