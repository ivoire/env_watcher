env_watcher
==========

This library allows to list the environment variables used and set by a
program. env_watcher is hooking environment related functions to list the used
variables.

Usage
-----
The library must be used through the **LD_PRELOAD** mechanism:

    LD_PRELOAD=$INSTALL_PATH/env_watcher.so my_program

In this case, env_watcher will log the environment variables used and print the
result in a file called **results.yaml** in the current directory.

Available options
-----------------
env_watcher comes with some options to control his behavior:

* ENW_VERBOSITY: set the verbosity from 0 to 3
* ENW_RESULTS: path to the file where to the store the results. The file is
               created, truncated and filled with the results when the program
               exits.


Hooked function:
---------------------
env_watcher is hooking the following functions:

* clearenv
* getenv
* putenv
* setenv
* unsetenv


Contributing
------------
If you have any question, bug, feature or patches, feel free to send them by
mail or through the bug tracker.
