Windows Executable Link Stubs
=============================

This project allows the creation of Windows "executable stubs" - exe files
that when run, execute a defined command, passing the command line arguments
on to the subcommand.

Usage
-----

Create a stub wrapper as follows:

    mkwrapper.py -f filename.exe -c "a command"

This will generate an executable, `filename.exe`, which when run, will execute
`a command`. So, for example:

    filename a b c

runs

    a command a b c

Note that there is no requirement for the command to be just an executable
name. A common usage might be to run `py C:\Some\Python\Script.py`.

It is possible to have the supplied arguments inserted somewhere other than
at the end of the line, by including `%s` in the command string. The arguments
will be inserted in place of the `%s`.

Technical Details
-----------------

The command is executed using the Windows `CreateProcess` API, so file
associations will not be considered when running the subcommand.

The stub executable is built from the `stub.c` source. The compile command is
simply `cl /Festub.exe stub.c`. The script is built from `template.py` and the
stub exe using `build.py`.

If you want to use a different stub when building a link (for example, when
testing a change to the stub code) you can use the `--stub` argument to
specify the name of the sub exe directly.

Future Possibilities
--------------------

1. Enhance the command template syntax, for example a %-directive to mean the
   directory of the stub (to allow running programs relative to the stub).
2. Additional control over the subprocess - altered environment, run in a
   specific directory, ...?
