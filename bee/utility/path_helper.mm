#include <bee/utility/path_helper.h>

#include <Foundation/Foundation.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace bee::path_helper {
    path_expected appdata_path() {
        NSArray *array = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
        if ([array count] > 0) {
            return fs::path([[array objectAtIndex:0] fileSystemRepresentation]);
        }
        return unexpected<std::string>("NSSearchPathForDirectoriesInDomains failed.");
    }
}
