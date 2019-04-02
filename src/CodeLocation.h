#ifndef GUNGNIR_CODELOCATION_H
#define GUNGNIR_CODELOCATION_H

#include <cstdint>
#include <string>
#include "Common.h"

namespace Gungnir {
/**
 * Describes the location of a line of code.
 * You can get one of these with #HERE.
 */
struct CodeLocation {
    /// Called by #HERE only.
    CodeLocation(const char *file,
                 const uint32_t line,
                 const char *function,
                 const char *prettyFunction)
        : file(file)
          , line(line)
          , function(function)
          , prettyFunction(prettyFunction) {}

    std::string str() const {
        return format("%s at %s:%d",
                      qualifiedFunction().c_str(),
                      relativeFile().c_str(),
                      line);
    }

    const char *baseFileName() const;

    std::string relativeFile() const;

    std::string qualifiedFunction() const;

    /// __FILE__
    const char *file;
    /// __LINE__
    uint32_t line;
    /// __func__
    const char *function;
    /// __PRETTY_FUNCTION__
    const char *prettyFunction;
};

/**
 * Constructs a #CodeLocation describing the line from where it is used.
 */
#define HERE \
    Gungnir::CodeLocation(__FILE__, __LINE__, __func__, __PRETTY_FUNCTION__)


}
#endif //GUNGNIR_CODELOCATION_H
