#include <cstring>
#include <pcrecpp.h>
#include <cassert>

#include "CodeLocation.h"
#include "Common.h"

namespace Gungnir {

namespace {

/**
 * Return the number of characters of __FILE__ that make up the path prefix.
 * That is, __FILE__ plus this value will be the relative path from the top
 * directory of the RAMCloud repo.
 */
int
length__FILE__Prefix() {
    const char *start = __FILE__;
    const char *match = strstr(__FILE__, "src/CodeLocation.cc");
    assert(match != nullptr);
    return static_cast<int>(match - start);
}

}

/**
 * Return the base name of the file (i.e., only the last component of the
 * file name, omitting any preceding directories).
 */
const char *
CodeLocation::baseFileName() const {
    const char *lastSlash = strrchr(file, '/');
    if (lastSlash == nullptr) {
        return file;
    }
    return lastSlash + 1;
}

string
CodeLocation::relativeFile() const {
    static int lengthFilePrefix = length__FILE__Prefix();
    // Remove the prefix only if it matches that of __FILE__. This check is
    // needed in case someone compiles different files using different paths.
    if (strncmp(file, __FILE__, lengthFilePrefix) == 0)
        return string(file + lengthFilePrefix);
    else
        return string(file);
}

/**
 * Return the name of the function, qualified by its surrounding classes and
 * namespaces. Note that this strips off the RAMCloud namespace to produce
 * shorter strings.
 *
 * Beware: this method is really really slow (10-20 microseconds); we no
 * longer use it in log messages because it wastes so much time.
 */
string
CodeLocation::qualifiedFunction() const {
    string ret;
    const string pattern(
        format(R"(\s(?:Gungnir::)?(\S*\b%s)\()", function));
    if (pcrecpp::RE(pattern).PartialMatch(prettyFunction, &ret))
        return ret;
    else // shouldn't happen
        return function;
}
}
