# krb5-strength 3.2

[![Build
status](https://github.com/rra/krb5-strength/workflows/build/badge.svg)](https://github.com/rra/krb5-strength/actions)
[![Debian
package](https://img.shields.io/debian/v/krb5-strength)](https://tracker.debian.org/pkg/krb5-strength)

Copyright 2016, 2020 Russ Allbery <eagle@eyrie.org>.  Copyright 2006-2007,
2009-2010, 2012-2014 The Board of Trustees of the Leland Stanford Junior
University.  Copyright 1993 Alec Muffett.  This software is distributed
under a BSD-style license.  Please see the section [License](#license)
below for more information.

## Blurb

krb5-strength provides a password quality plugin for the MIT Kerberos KDC
(specifically the kadmind server) and Heimdal KDC, an external password
quality program for use with Heimdal, and a per-principal password history
implementation for Heimdal.  Passwords can be tested with CrackLib,
checked against a CDB or SQLite database of known weak passwords with some
transformations, checked for length, checked for non-printable or
non-ASCII characters that may be difficult to enter reproducibly, required
to contain particular character classes, or any combination of these
tests.

## Description

Heimdal includes a capability to plug in external password quality checks
and comes with an example that checks passwords against CrackLib.
However, in testing at Stanford, we found that CrackLib with its default
transform rules does not catch passwords that can be guessed using the
same dictionary with other tools, such as Jack the Ripper.  We then
discovered other issues with CrackLib with longer passwords, such as some
bad assumptions about how certain measures of complexity will scale, and
wanted to impose other limitations that it didn't support.

This plugin provides the ability to check password quality against the
standard version of CrackLib, or against a modified version of CrackLib
that only passes passwords that resist attacks from both Crack and Jack
the Ripper using the same rule sets.  It also supports doing simpler
dictionary checks against a CDB database, which is fast with very large
dictionaries, or a SQLite database, which can reject all passwords within
edit distance one of a dictionary word.  It can also impose other
programmatic checks on passwords such as character class requirements.

If you're just now starting with password checking, I recommend using the
SQLite database with a large wordlist and minimum password lengths.  We
found this produced the best results with the least user frustration.

For Heimdal, krb5-strength includes both a program usable as an external
password quality check and a plugin that implements the dynamic module
API.  For MIT Kerberos (1.9 or later), it includes a plugin for the
password quality (pwqual) plugin API.

krb5-strength can be built with either the system CrackLib or with the
modified version of CrackLib included in this package.  Note, however,
that if you're building against the system CrackLib, Heimdal includes in
the distribution a strength-checking plugin and an external password check
program that use the system CrackLib.  With Heimdal, it would probably be
easier to use that plugin or program than build this package unless you
want the modified CrackLib, one of the other dictionary types, or the
additional character class and length checks.

For information about the changes to the CrackLib included in this
toolkit, see `cracklib/HISTORY`.  The primary changes are tighter rules,
which are more aggressive at finding dictionary words with characters
appended and prepended, which tighten the requirements for password
entropy, and which add stricter rules for longer passwords.  They are also
minor changes to fix portability issues, remove some code that doesn't
make sense in the kadmind context, and close a few security issues.  The
standard CrackLib distribution on at least some Linux distributions now
supports an additional interface to configure its behavior, and
krb5-strength should change in the future to use that interface and drop
the embedded copy.

krb5-strength also includes a password history implementation for Heimdal.
This is separate from the password strength implementation but can be
stacked with it so that both strength and history checks are performed.
This history implementation is available only via the Heimdal external
password quality interface.  MIT Kerberos includes its own password
history implementation.

## Requirements

For Heimdal, you may use either the external password quality check tool,
installed as heimdal-strength, or the plugin as you choose.  It has been
tested with Heimdal 1.2.1 and later, but has not recently been tested with
versions prior to 7.0.

For MIT Kerberos, version 1.9 or higher is required for the password
quality plugin interface.  MIT Kerberos does not support an external
password quality check tool directly, so you will need to install the
plugin.

You can optionally build against the system CrackLib library.  Any version
should be supported, but note that some versions, particularly older
versions close to the original code, do things like printing diagnostics
to stderr, calling exit, and otherwise not being well-behaved for use
inside plugins or libraries.  They also have known security
vulnerabilities.  If using a system CrackLib library, use version 2.8.22
or later to avoid these problems.

You can also optionally build against the TinyCDB library, which provides
support for simpler and faster password checking against a CDB dictionary
file, and the SQLite library (a version new enough to support the
`sqlite3_open_v2` API; 3.7 should be more than sufficient), which provides
support for checking whether passwords are within edit distance one of a
dictionary word.

For this module to be effective for either Heimdal or MIT Kerberos, you
will also need to construct a dictionary.  The `mkdict` and `packer`
utilities to build a CrackLib dictionary from a word list are included in
this toolkit but not installed by default.  You can run them out of the
`cracklib` directory after building.  You can also use the utilities that
come with the stock CrackLib package (often already packaged in a Linux
distribution); the database format is compatible.

For building a CDB or SQLite dictionary, use the provided
`krb5-strength-wordlist` program.  For CDB dictionries, the `cdb` utility
must be on your `PATH`.  For SQLite, the DBI and DBD::SQLite Perl modules
are required.  `krb5-strength-wordlist` requires Perl 5.010 or later.

For a word list to use as source for the dictionary, you can use
`/usr/share/dict/words` if it's available on your system, but it would be
better to find a more comprehensive word list.  Since word lists are
bulky, often covered by murky copyrights, and easily locatable on the
Internet with a modicum of searching, none are included in this toolkit.

The password history program, heimdal-history, requires Perl 5.010 or
later plus the following CPAN modules:

* DB_File::Lock
* Crypt::PBKDF2
* Getopt::Long::Descriptive
* IPC::Run
* JSON
* Readonly

and their dependencies.

To bootstrap from a Git checkout, or if you change the Automake files and
need to regenerate Makefile.in, you will need Automake 1.11 or later.  For
bootstrap or if you change configure.ac or any of the m4 files it includes
and need to regenerate configure or config.h.in, you will need Autoconf
2.64 or later.  You will also need Perl 5.010 or later and the DBI,
DBD::SQLite, JSON, Perl6::Slurp, and Readonly modules (from CPAN) to
generate man pages and bootstrap the test suite data from a Git checkout.

## Building and Installation

You can build and install krb5-strength with the standard commands:

```
    ./configure
    make
    make install
```

If you are building from a Git clone, first run `./bootstrap` in the
source directory to generate the build files.  `make install` will
probably have to be done as root.  Building outside of the source
directory is also supported, if you wish, by creating an empty directory
and then running configure with the correct relative path.

By default, the Heimdal external password check function is installed as
`/usr/local/bin/heimdal-strength`, and the plugin is installed as
`/usr/local/lib/krb5/plugins/pwqual/strength.so`.  You can change these
paths with the `--prefix`, `--libdir`, and `--bindir` options to
`configure`.

By default, the embedded version of CrackLib will be used.  To build with
the system version of CrackLib, pass `--with-cracklib` to `configure`.
You can optionally add a directory, giving the root directory where
CrackLib was installed, or separately set the include and library path
with `--with-cracklib-include` and `--with-cracklib-lib`.  You can also
build without any CrackLib support by passing `--without-cracklib` to
`configure`.

krb5-strength will automatically build with TinyCDB if it is found.  To
specify the installation path of TinyCDB, use `--with-tinycdb`.  You can
also separately set the include and library path with
`--with-tinycdb-include` and `--with-tinycdb-lib`.

Similarly, krb5-strength will automatically build with SQLite if it is
found.  To specify the installation path of SQLite, use `--with-sqlite`.
You can also separately set the include and library path with
`--with-sqlite-include` and `--with-sqlite-lib`.

Normally, configure will use `krb5-config` to determine the flags to use
to compile with your Kerberos libraries.  To specify a particular
`krb5-config` script to use, either set the `PATH_KRB5_CONFIG` environment
variable or pass it to configure like:

```
    ./configure PATH_KRB5_CONFIG=/path/to/krb5-config
```

If `krb5-config` isn't found, configure will look for the standard
Kerberos libraries in locations already searched by your compiler.  If the
the `krb5-config` script first in your path is not the one corresponding
to the Kerberos libraries you want to use, or if your Kerberos libraries
and includes aren't in a location searched by default by your compiler,
you need to specify a different Kerberos installation root via
`--with-krb5=PATH`.  For example:

```
    ./configure --with-krb5=/usr/pubsw
```

You can also individually set the paths to the include directory and the
library directory with `--with-krb5-include` and `--with-krb5-lib`.  You
may need to do this if Autoconf can't figure out whether to use lib,
lib32, or lib64 on your platform.

To not use krb5-config and force library probing even if there is a
krb5-config script on your path, set PATH_KRB5_CONFIG to a nonexistent
path:

```
    ./configure PATH_KRB5_CONFIG=/nonexistent
```

`krb5-config` is not used and library probing is always done if either
`--with-krb5-include` or `--with-krb5-lib` are given.

Pass `--enable-silent-rules` to configure for a quieter build (similar to
the Linux kernel).  Use `make warnings` instead of `make` to build with
full GCC compiler warnings (requires either GCC or Clang and may require a
relatively current version of the compiler).

You can pass the `--enable-reduced-depends` flag to configure to try to
minimize the shared library dependencies encoded in the binaries.  This
omits from the link line all the libraries included solely because other
libraries depend on them and instead links the programs only against
libraries whose APIs are called directly.  This will only work with shared
libraries and will only work on platforms where shared libraries properly
encode their own dependencies (this includes most modern platforms such as
all Linux).  It is intended primarily for building packages for Linux
distributions to avoid encoding unnecessary shared library dependencies
that make shared library migrations more difficult.  If none of the above
made any sense to you, don't bother with this flag.

After installing this software, see the man pages for krb5-strength,
heimdal-strength, and heimdal-history for configuration information.

## Testing

krb5-strength comes with a test suite, which you can run after building
with:

```
    make check
```

If a test fails, you can run a single test with verbose output via:

```
    tests/runtests -o <name-of-test>
```

Do this instead of running the test program directly since it will ensure
that necessary environment variables are set up.

To run the test suite, you will need Perl 5.010 or later and the
dependencies of the `heimdal-history` program.  The following additional
Perl modules will also be used by the test suite if present:

* Perl6::Slurp
* Test::MinimumVersion
* Test::Perl::Critic
* Test::Pod
* Test::Spelling
* Test::Strict

All are available on CPAN.  Some tests will be skipped if the modules are
not available.

To enable tests that don't detect functionality problems but are used to
sanity-check the release, set the environment variable `RELEASE_TESTING`
to a true value.  To enable tests that may be sensitive to the local
environment or that produce a lot of false positives without uncovering
many problems, set the environment variable `AUTHOR_TESTING` to a true
value.

## Support

The [krb5-strength web
page](https://www.eyrie.org/~eagle/software/krb5-strength/) will always
have the current version of this package, the current documentation, and
pointers to any additional resources.

For bug tracking, use the [issue tracker on
GitHub](https://github.com/rra/krb5-strength/issues).  However, please be
aware that I tend to be extremely busy and work projects often take
priority.  I'll save your report and get to it as soon as I can, but it
may take me a couple of months.

## Source Repository

krb5-strength is maintained using Git.  You can access the current source
on [GitHub](https://github.com/rra/krb5-strength) or by cloning the
repository at:

https://git.eyrie.org/git/kerberos/krb5-strength.git

or [view the repository on the
web](https://git.eyrie.org/?p=kerberos/krb5-strength.git).

The eyrie.org repository is the canonical one, maintained by the author,
but using GitHub is probably more convenient for most purposes.  Pull
requests are gratefully reviewed and normally accepted.

## License

The krb5-strength package as a whole is covered by the following copyright
statement and license:

> Copyright 2016, 2020
>     Russ Allbery <eagle@eyrie.org>
>
> Copyright 2006-2007, 2009-2010, 2012-2014
>     The Board of Trustees of the Leland Stanford Junior University
>
> Copyright 1993
>     Alec Muffett
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the "Software"),
> to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, sublicense,
> and/or sell copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
> THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
> FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
> DEALINGS IN THE SOFTWARE.
>
> Developed by Daria Phoebe Brashear and Ken Hornstein of Sine Nomine Associates,
> on behalf of Stanford University.
>
> The embedded version of CrackLib (all files in the `cracklib`
> subdirectory) is covered by the Artistic license.  See the file
> `cracklib/LICENCE` for more information.  Combined derivative works that
> include this code, such as binaries built with the embedded CrackLib, will
> need to follow the terms of the Artistic license as well as the above
> license.

Some files in this distribution are individually released under different
licenses, all of which are compatible with the above general package
license but which may require preservation of additional notices.  All
required notices, and detailed information about the licensing of each
file, are recorded in the LICENSE file.

Files covered by a license with an assigned SPDX License Identifier
include SPDX-License-Identifier tags to enable automated processing of
license information.  See https://spdx.org/licenses/ for more information.

For any copyright range specified by files in this package as YYYY-ZZZZ,
the range specifies every single year in that closed interval.
