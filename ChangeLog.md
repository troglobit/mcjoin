ChangeLog
=========

All notable changes to the project are documented in this file.

[v2.7][] - 2020-11-10
---------------------

New user interface, inspired by mtr.  Easier to track multiple streams
and spot gaps in real-time.

- Adjust ulimit so we can open enough sockets to send/receive multicast
- Revert to tar.gz for distribution tarballs
- Support for adjusting payload length, 42 -> 1648
- Support for adjusting packet send/poll rate
- Support for macOS
- Support for OpenSolaris/Illumos, e.g. OmniOS
- Support for figuring out default interface on *BSD, Illumos, and macOS
- Support for using old UI using `-o` flag
- Support any source port in (S,G) joins
- Greatly improved IPv6 support, portability and multiple groups
- Dropped `-r SEC` option, not needed anymore and was misleading
- Fix #8: Handle IPv6 being disabled, per group and per socket
- Fix #9: No feedback on receiver when using mcjoin as sender
- Fix #10: Calculate and show gaps in received groups


[v2.6][] - 2020-04-22
---------------------

Full IPv6 support, both send and join.

- Increase MAX number of groups to join: 250 -> 2048
- Relocate source code to `src/` sub-directory
- Add support for sending with/to IPv6 address/group
- Update docs slightly


[v2.5][] - 2020-02-23
---------------------

- Add Dockerfile, thanks to Graeme Lawes
- FreeBSD support, by Ryan Libby
- Replaced mutually exclusive options `-q` and `-d` with `-l level`.
  Default log level is `notice`, for true quiet mode, use `-l none`
- Added new daemon mode, which makes mcjoin detach from its controlling
  terminal and run in the background, with all output except progress
  redirected to syslog
- Fix possible file descriptor leak
- Improve error detection if mcjoin cannot find a default interface,
  a valid (source) IP address, or anything else goes wrong
- Add support for `-w SEC`, initial delay before actually starting.
  Useful if started very early in a system bootstrap before network
  has come up properly


[v2.4][] - 2019-04-04
---------------------

- Support for SSM (S,G) join, in addition to the existing ASM (*,G) join
- Find interface bsaed on default route with *best* metric
- Initial support for IPv6 (*,G) and (S,G) joins


[v2.3][] - 2018-09-20
---------------------

- Automatically detect default interface
- Enable support for multiple mcjoin receivers on one host using the
  same destination (Internet) port
- Fix #1: Receiver does not stop after count received packets
- Avoid using GLIBC extension `__progname`
- Updated build instructions in README


[v2.2][] - 2016-09-08
---------------------

- Progress bar on receiver
- Bind receiver to group:port, instead of 0.0.0.0:port, filters noise
- Increased send rate of sender: 1/1 sec --> 1/100 msec
- Set outbound interface for sending multicast
- Portability fixes, now builds and runs on musl libc
- Verify max length of interface name from operator


[v2.1][] - 2016-03-08
---------------------

Add validation of incoming multicast packets, verifying destination
IP address of packet against expected multicast group for socket.


[v2.0][] - 2016-03-07
---------------------

Massive refactor to add support for acting as both sender and receiver
of multicast packets.  This release makes mcjoin a standalone tool for
verifying multicast connectivity.

Backwards compatible with [v1.5][] w.r.t. joining a single or multiple
groups, with an optional `-r, --restart=SEC`.  However, `--long-opts`
have been removed and the `-f FIRST` and `-n NUM` options have been
changed to command arguments on the form `GROUP+NUM`, which integrates
better with the rest of the syntax.


[v1.5][] - 2014-07-18
---------------------

Debian packaging and fix bug in new `-r, --restart=SEC` option causing
recurring join to only last for about one second after leave.


[v1.4][] - 2014-06-19
---------------------

Support for a `-r, --restart=SEC` command line option to force a
sequence of leave/join every SEC seconds.  By Magnus Öberg.


[v1.3][] - 2012-02-09
---------------------

Initial public release.  First few versions were used internally at
OnTime AS and later at Westermo.  Based on the work by David Stevens
and developed further by Joachim Nilsson, on his spare time.


[UNRELEASED]: https://github.com/troglobit/mcjoin/compare/v2.7...HEAD
[v2.7]:       https://github.com/troglobit/mcjoin/compare/v2.6...v2.7
[v2.6]:       https://github.com/troglobit/mcjoin/compare/v2.5...v2.6
[v2.5]:       https://github.com/troglobit/mcjoin/compare/v2.4...v2.5
[v2.4]:       https://github.com/troglobit/mcjoin/compare/v2.3...v2.4
[v2.3]:       https://github.com/troglobit/mcjoin/compare/v2.2...v2.3
[v2.2]:       https://github.com/troglobit/mcjoin/compare/v2.1...v2.2
[v2.1]:       https://github.com/troglobit/mcjoin/compare/v2.0...v2.1
[v2.0]:       https://github.com/troglobit/mcjoin/compare/v1.5...v2.0
[v1.5]:       https://github.com/troglobit/mcjoin/compare/v1.4...v1.5
[v1.4]:       https://github.com/troglobit/mcjoin/compare/v1.3...v1.4
