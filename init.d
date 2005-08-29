#! /bin/sh
#
# sysrqd init script
# (c) 2005 - Julien Danjou <julien@danjou.info>

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/sysrqd
NAME=sysrqd
DESC=sysrqd
SECRET=/etc/sysrqd.secret

test -x $DAEMON || exit 0
test -f $SECRET || exit 0

# Include sysrqd defaults if available
if [ -f /etc/default/sysrqd ] ; then
	. /etc/default/sysrqd
fi

set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
	start-stop-daemon --start --quiet --pidfile /var/run/$NAME.pid \
		--exec $DAEMON -- $DAEMON_OPTS
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	start-stop-daemon --stop --quiet --pidfile /var/run/$NAME.pid \
		--exec $DAEMON
	echo "$NAME."
	;;
  restart|force-reload)
	#
	#	If the "reload" option is implemented, move the "force-reload"
	#	option to the "reload" entry above. If not, "force-reload" is
	#	just the same as "restart".
	#
	echo -n "Restarting $DESC: "
	start-stop-daemon --stop --quiet --pidfile \
		/var/run/$NAME.pid --exec $DAEMON
	sleep 1
	start-stop-daemon --start --quiet --pidfile \
		/var/run/$NAME.pid --exec $DAEMON -- $DAEMON_OPTS
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	# echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
