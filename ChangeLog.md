ChangeLog
=========

All notable changes to the project are documented in this file.


[v2.12][UNRELEASED]
----------------------

- Drop previously removed `-r` option from usage message, by Jan Klemkow
- Refactor fixed group array to dynamic allocation, by Jan Klemkow
- Fix #28: too small buffer for IPv6 SSM
- Fix #32: high RAM/RSS usage


[v2.11][] - 2022-08-23
----------------------

- Fix issue with spurious timer expiry on Aarch64 (Arm64)
- Fix #26: sender takes 100% CPU usage in daemon mode


[v2.10][] - 2022-03-19
----------------------

- Project CI moved from Travis-CI to GitHub
- Container support: `docker pull ghcr.io/troglobit/mcjoin:latest`
- Minor bug fixes, found by Coverity Scan
- Fix #18: optional trailing group port arguments ip4/ip6, by Adam Young
- Fix #19: `setsid()` problem causing mcjoin to exit when daemonized,
  regression introduced in v2.7.  Found and fixed by Foster Snowhill
- Fix #23: fix uptime display in old output mode, by Foster Snowhill
- Document alternative use-case, by Torstein Eide


[v2.9][] - 2021-01-14
---------------------

Multiple viewing modes, including new stats and rate/bytes view.  See
help text for details.

- Fix #11: Another presentation view, show rate/bytes + plotter
- Fix #14: Update plotter on timer, not input, we want to see the gaps
- Change style of headings from top style to pimctl/mroutectl style
- Improved viewing modes and plotter characters for dupes/delay/etc.
- Improved column sizing for better use of whitespace in narrow ttys
- Show stats on exit
- Support for scrolling log up/down with PgUp/PgDn
- Support for displaying uptime, also shown on exit
- Support for generating duplicate frames, for testing purposes
- Support for toggling debug logs at runtime


[v2.8][] - 2021-01-08
---------------------

- Detect a default interface even on systems without default route
- Support Ctrl-L to refresh a garbled display
- Support for a deadline timeout `-W sec`, issue #12
- Support for redirecting output to a log file; `mcjoin -l debug > file`
- Document reverse path filtering in Linux
- Document how to test with older IGMP versions in Linux
- Fix resize glitch in plotter (new event based framework)
- Fix TTY restore on abort or abnormal exit
- Fix #12: fully restore support for old output, `-o`


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


[UNRELEASED]: https://github.com/troglobit/mcjoin/compare/v2.11...HEAD
[v2.12]:      https://github.com/troglobit/mcjoin/compare/v2.11...v2.12
[v2.11]:      https://github.com/troglobit/mcjoin/compare/v2.10...v2.11
[v2.10]:      https://github.com/troglobit/mcjoin/compare/v2.9...v2.10
[v2.9]:       https://github.com/troglobit/mcjoin/compare/v2.8...v2.9
[v2.8]:       https://github.com/troglobit/mcjoin/compare/v2.7...v2.8
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
