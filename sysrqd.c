/*
 * sysrqd.c - Daemon to control sysrq over network
 *
 * © 2005-2009 Julien Danjou <julien@danjou.info>
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

#include <sched.h>
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
#include <errno.h>
#include <crypt.h>

#define PASS_MAX_LEN 256
#define BIND_MAX_LEN 16
#define PROMPT "sysrq> "
#define SYSRQ_TRIGGER_PATH "/proc/sysrq-trigger"
#define AUTH_FILE "./sysrqd.secret"
#define BINDIP_FILE "./sysrqd.bind"
#define PID_FILE "./sysrqd.pid"
#define SYSRQD_PRIO -19
#define SYSRQD_LISTEN_PORT 4094

char pwd[PASS_MAX_LEN];
int sock_serv;

/* Macro used to write a string to the client */
#define write_cli(s) \
	write(sock_client, s, sizeof(s) - 1);

/* Macro used to write the prompt */
#define write_prompt(s) \
	write_cli(PROMPT);

#define errmsg(s) \
	fprintf(stderr, "%s (%s:%d)\n", s, __FILE__, __LINE__);

/* Authenticate the connection */
static int
auth (int sock_client)
{
    char buf[PASS_MAX_LEN];
    size_t len;
    char *crypt_result;
    
    write_cli("sysrqd password: ");
    memset(buf, 0, sizeof(buf));

    /* Read password */
    len = read(sock_client, buf, sizeof(buf) - 1);

    /* remove \r and \n at end */
    while(len > 0
          && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        len--;

    /* If password has been sent */
    if(len > 0)
    {
        buf[len] = '\0';
        /* Check if our stored password is crypted, i.e. starts with $[0-9]$ */
        if(strlen(pwd) >= 34 && pwd[0] == '$' && pwd[2] == '$' && pwd[1] >= '0' && pwd[1] <= '9')
        {
            crypt_result = crypt(buf, pwd);
            if(!strcmp(crypt_result, pwd))
                return 1;
            else
                write_cli("Go away!\r\n");
        } else {
            if(!strcmp(buf, pwd))
                return 1;
            else
                write_cli("Go away!\r\n");
        }
    }

    return 0;
}

/* Read a configuration file */
static int
read_conffile (const char *file, char* buf, size_t buflen)
{
  int fd;

  if((fd = open (file, O_RDONLY)) == -1)
  {
      syslog(LOG_ERR, "Unable to open file %s: %s",
             file,
             strerror(errno));
      return 1;
  }
	
  memset(buf, 0, buflen);
  read (fd, buf, buflen - 1);
  close (fd);

  buf[buflen - 1] = '\0';

  /* Strip last \n */
  char *tmp;
  if((tmp = strchr(buf, '\n')))
    *tmp = '\0';
	
  return 0;
}

/* Read commands */
static void
read_cmd (int sock_client, int fd_sysrq)
{
  char buf = 0;
  
  write_prompt();
  do
    {
      if(buf == '\n')
	write_prompt();

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
  char bindip[BIND_MAX_LEN];

  addr.sin_family = AF_INET;
  addr.sin_port = htons(SYSRQD_LISTEN_PORT);

  if(read_conffile(BINDIP_FILE, bindip, sizeof(bindip)))
      addr.sin_addr.s_addr = INADDR_ANY;
  else if(!inet_aton(bindip, &addr.sin_addr))
  {
      syslog(LOG_ERR, "Unable to convert IP: %s, using INADDR_ANY",
             strerror(errno));
      addr.sin_addr.s_addr = INADDR_ANY;
  }

  if(!(sock_serv = socket (PF_INET, SOCK_STREAM, 0)))
  {
      syslog(LOG_ERR, "Error while creating server socket: %s",
             strerror(errno));
      return 1;
  }
  
  /* We tries to avoid "socket already in use" errors */
  opt = 1;
  setsockopt(sock_serv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

  if(bind (sock_serv, (struct sockaddr *) &addr, sizeof (addr)))
  {
      syslog(LOG_ERR, "Unable to bind(): %s",
             strerror(errno));
      return 1;
  }

  if(listen(sock_serv, 2))
  {
      syslog(LOG_ERR, "Unable to listen(): %s",
             strerror(errno));
      return 1;
  }

  size_addr = sizeof (addr_client);
  
  syslog(LOG_INFO, "Listening on port tcp/%d", SYSRQD_LISTEN_PORT);

  while((sock_client = accept (sock_serv, (struct sockaddr *) &addr_client, &size_addr)))
    {
      if(auth (sock_client))
	read_cmd (sock_client, fd_sysrq);
      close(sock_client);
    }
  
  close (sock_serv);

  return 0;
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

  /* ignore sigpipe */
  sigemptyset(&mask);
  sigaddset(&mask, SIGPIPE);
  sigprocmask(SIG_BLOCK, &mask, NULL);

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

static void
do_on_exit(void)
{
    syslog(LOG_NOTICE, "Exiting");
}

int
main (void)
{
  int fd_sysrq;
  
  atexit(do_on_exit);

  /* We test if it is worth the pain to fork if setpriority would fail */
  if(getuid() != 0)
    {
      errmsg ("Only root can run this program.");
      return 1;
    }

  /* We read our password */
  if(read_conffile (AUTH_FILE, pwd, sizeof(pwd)))
      return EXIT_FAILURE;

  /* mlock, we want this to always run */
  mlockall(MCL_CURRENT | MCL_FUTURE);



  /* We daemonize */
  daemon(0, 0);

  struct sched_param param;
  param.sched_priority = 99;
  if (sched_setscheduler(0, SCHED_FIFO, & param) != 0) {
      syslog (LOG_PID | LOG_DAEMON, "Unable to set realtime priority");
  exit(EXIT_FAILURE);
  }
  
  openlog ("sysrqd", LOG_PID, LOG_DAEMON);
  syslog(LOG_PID | LOG_DAEMON, "Starting");

  if(write_pidfile(getpid()))
      syslog (LOG_PID | LOG_DAEMON, "Unable to write pidfile");

  /* We set our priority */
  if(setpriority (PRIO_PROCESS, 0, SYSRQD_PRIO))
  {
      syslog (LOG_PID | LOG_DAEMON, "Unable to set priority: %s",
              strerror(errno));
      return EXIT_FAILURE;
  }

  /* Catch some signals */
  catch_signals ();

  /* We keep the sysrq-trigger file opened */
  fd_sysrq = open(SYSRQ_TRIGGER_PATH, O_SYNC|O_WRONLY);
  if(fd_sysrq == -1)
  {
      syslog(LOG_ERR, "Error while opening sysrq trigger: %s",
             strerror(errno));
      return EXIT_FAILURE;
  }

  /* Now listen */
  if(!start_listen (fd_sysrq))
      return EXIT_FAILURE;
  
  /* If we quit, close the trigger */
  close (fd_sysrq);
  
  closelog();
  
  return EXIT_SUCCESS;
}
