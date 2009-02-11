/*
 * sysrqd.c - Daemon to control sysrq over network
 *
 * © 2005-2008 Julien Danjou <julien@danjou.info>
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
 *
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
#include <syslog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>

#define PASS_MAX_LEN 32
#define BIND_MAX_LEN 16
#define PROMPT "sysrq> "
#define SYSRQ_TRIGGER_PATH "/proc/sysrq-trigger"
#define AUTH_FILE "/etc/sysrqd.secret"
#define BINDIP_FILE "/etc/sysrqd.bind"
#define PID_FILE "/var/run/sysrqd.pid"
#define SYSRQD_PRIO -19
#define SYSRQD_LISTEN_PORT 4094

char pwd[PASS_MAX_LEN];
char bindip[BIND_MAX_LEN];
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
static int
auth (int sock_client)
{
  char buf[PASS_MAX_LEN];
    
  write_cli (sock_client, "sysrqd password: ");
  if(read(sock_client, buf, PASS_MAX_LEN) > 0)
      if(strcmp(buf, pwd))
        return 1;
  
  write_cli (sock_client, "Go away!\r\n");
  return 0;
}

/* Read a configuration file */
static int
read_conffile (const char *file, char* buf, size_t buflen)
{
  int fd;
  char * tmp;

  if((fd = open (file, O_RDONLY)) == -1)
    return 1;
	
  memset(buf, 0, buflen);
  read (fd, buf, buflen);
  close (fd);

  pwd[buflen - 1] = '\0';

  /* Strip last \n */
  if((tmp = strchr(buf, '\n')))
    *tmp = '\0';
	
  return 0;
}

/* Read commands */
static void
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
  while(read (sock_client, &buf, 1) == 1 && buf != 'q');
}


/*
 * Listen to a port,
 * authenticate connection
 * and execute commands
*/
static int
start_listen (int fd_sysrq)
{
  int sock_client;
  socklen_t size_addr;
  struct sockaddr_in addr;
  struct sockaddr_in addr_client;
  int opt;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SYSRQD_LISTEN_PORT);

  if (read_conffile(BINDIP_FILE, bindip, BIND_MAX_LEN))
    addr.sin_addr.s_addr = INADDR_ANY;
  else
    inet_aton(bindip, &addr.sin_addr);

  if(!(sock_serv = socket (PF_INET, SOCK_STREAM, 0)))
    {
      perror("Error while creating server socket");
      return 1;
    }
  
  /* We tries to avoid "socket already in use" errors */
  opt = 1;
  setsockopt(sock_serv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

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
  
  syslog(LOG_PID||LOG_DAEMON, "Listening on port tcp/%d", SYSRQD_LISTEN_PORT);

  while((sock_client = accept (sock_serv, (struct sockaddr *) &addr_client, &size_addr)))
    {
      if(auth (sock_client))
	read_cmd (sock_client, fd_sysrq);
      close(sock_client);
    }
  
  close (sock_serv);

  return 0;
}

static int
open_sysrq_trigger (void)
{
  return open (SYSRQ_TRIGGER_PATH, O_SYNC|O_WRONLY);
}

static void __attribute__ ((noreturn))
signal_handler (void)
{
  close (sock_serv);
  exit (EXIT_FAILURE);
}

static int
catch_signals (void)
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


static int
write_pidfile(pid_t pid)
{
  FILE *pidf = fopen(PID_FILE, "w");
  if (pidf == NULL)
    return 1;

  fprintf(pidf, "%d\n", pid);
  fclose(pidf);
  
  return 0;
}

int
main (void)
{
  int fd_sysrq;
  
  /* We test if it is worth the pain to fork if setpriority would fail */
  if(getuid() != 0)
    {
      errmsg ("Only root can run this program.");
      return 1;
    }

  /* We read our password */
  if(read_conffile (AUTH_FILE, pwd, PASS_MAX_LEN))
    {
      errmsg ("Error while reading password file ("AUTH_FILE").");
      return 1;
    }

  /* mlock, we want this to always run */
  mlockall(MCL_CURRENT | MCL_FUTURE);

  /* We daemonize */
  daemon(0, 0);
  
  openlog ("sysrqd", LOG_PID, LOG_DAEMON);
  syslog (LOG_PID||LOG_DAEMON, "sysrqd started");

  if(write_pidfile(getpid()))
    syslog (LOG_PID||LOG_DAEMON, "Unable to write pidfile");

  /* We set our priority */
  if(setpriority (PRIO_PROCESS, 0, SYSRQD_PRIO))
    {
      syslog (LOG_PID||LOG_DAEMON, "Unable to set priority.");
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

  /* Now listen */
  if(!start_listen (fd_sysrq))
    return 1;
  
  /* If we quit, close the trigger */
  close (fd_sysrq);
  
  closelog();
  
  return (EXIT_SUCCESS);
}
