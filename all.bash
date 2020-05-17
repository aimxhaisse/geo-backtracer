#!/usr/bin/env bash
#
# Script to control background processes working on a shard (mixer,
# worker).

cd $(dirname $0)

function ok {			# <msg>
    echo -e "ok: $@"
    return 0
}

function ko {			# <msg>
    echo -e "ko: $@"
    return 1
}

function warn-upon-failure {	# <return_value> <msg>
    local ret=$1
    local msg=$2
    if [ $ret -ne 0 ]
    then
	ko "$msg"
    fi
    return $ret
}

function be-quiet {		# <command1>
    $@ 2>&1 > /dev/null
    return $?
}


function get-uid {
    return $(id -u)
}

function usage {
    echo -e "usage: $0 [worker|mixer] [stop|start|restart|status|log]"
}

case $1 in
    "worker")
	PROJECT="geo-bt worker"
	PID="var/worker.pid"
	BIN="bin/bt"
	LOG="log/worker.log"
	CONFIG="etc/worker.yml"
	MODE="worker"
	;;

    "mixer")
	PROJECT="geo-bt mixer"
	PID="var/mixer.pid"
	BIN="bin/bt"
	LOG="log/mixer.log"
	CONFIG="etc/mixer.yml"
	MODE="mixer"
	;;

    *)
	usage
	exit 1
	;;
esac

LOGS="log"
VAR="var"

case $2 in
    "stop")
	ok "stopping $PROJECT"
	if [ -f $PID ]
	then
	    kill -9 $(cat $PID)
	    rm -f $PID
	fi
	ok "daemon stopped"
	exit 0
	;;

    "start")
	ok "starting $PROJECT"
	mkdir -p $VAR
	mkdir -p $LOGS
	touch $LOG
	bin/daemonizer -u $(whoami) -o $LOG -e $LOG -p $PID -- $BIN --type $MODE --config $CONFIG
	warn-upon-failure $? "Can't start the daemon, check your config" || exit 1
	ok "daemon started"
	exit 0
	;;

    "restart")
	$0 stop
	$0 start
	exit 0
	;;

    "status")
	pid=""
	if [ -f $PID ]
	then
            pid=$(ps -ef $(cat $PID 2>/dev/null) 2>/dev/null)
	fi

	if [ "$pid" != "" ]
        then
            ok "$PROJECT is running, with PID $(cat $PID)"
        else
            ko "$PROJECT is down"
        fi
        exit 0
        ;;

    "log")
	if [ -f $LOG ]
	then
	    ok "last 25 log entries:"
	    tail -n 25 $LOG
	else
	    ko "can't find log file ($LOG)"
	fi
	exit 0
	;;

esac

usage
exit 1
