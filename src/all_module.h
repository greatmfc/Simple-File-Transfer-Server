#ifndef AM_H
#define AM_H

#include "../include/all_libs.h"
#include "epoll_utility.hpp"
#include "logger.hpp"
#include "client.hpp"
#include "server.hpp"

/*
 * The first number specifies a major-version which will lead to \
	structure and interface changes and might not be compatible \
	with previous versions.

 * The second number specifies a minor-version which will lead to \
	function changes such as adding additional functions or \
	changing the behavior of a certain function.
   This won't break the backward compatibility.

 * The third number specifies a bug-fix-version which fix potential bugs in program.

 * The forth number specifies a testing-version when it is '1', \
	a release-version when it is '2'.
*/
#define VERSION "1.11.10.1"
#define LAST_MODIFIED 20230531L

#endif //! AM_H
