m c j o i n
===========

A tiny multicast testing tool

```shell
sender$ mcjoin -s
^C
sender$
```

```shell
receiver$ mcjoin
joined group 225.1.2.3 on eth0 ...
.......................................^C
receiver$
```

usage
-----

```shell
$ mcjoin -h

Usage: mcjoin [dhjqsv] [-c COUNT] [-i IFNAME] [-p PORT] [-r SEC] [-t TTL]
	      [GROUP0 .. GROUPN | GROUP+NUM]

Options:
  -c COUNT     Exit after COUNT number of received and/or sent packets
  -d           Debug output
  -h           This help text
  -i IFNAME    Interface to use for multicast groups, default eth0
  -j           Join groups, default unless acting as sender
  -p PORT      UDP port number to listen to, default: 1234
  -q           Quiet mode
  -r SEC       Do a join/leave every SEC seconds
  -s           Act as sender, sends packets to select groups
  -t TTL       TTL to use when sending multicast packets, default 1
  -v           Display program version

Mandatory arguments to long options are mandatory for short options too
Bug report address: Joachim Nilsson <troglobit()gmail!com>

$
```
