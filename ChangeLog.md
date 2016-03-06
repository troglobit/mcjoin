ChangeLog
=========

All notable changes to the project are documented in this file.


[v2.0][UNRELEASED] - 2016-03-XX
-------------------------------

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


[UNRELEASED]: https://github.com/troglobit/mcjoin/compare/v1.5...HEAD
[v2.0]:       https://github.com/troglobit/mcjoin/compare/1.5...2.0
[v1.5]:       https://github.com/troglobit/mcjoin/compare/1.4...1.5
[v1.4]:       https://github.com/troglobit/mcjoin/compare/1.3...1.4
