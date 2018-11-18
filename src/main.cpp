#include "cppgrep.h"
#include "util/log.h"

int main(int argc, char* argv[])
{
    // control flag for multithreading
    constexpr auto multithreaded {true};

    if (argc == 3)
    {
        auto thread_pool = multithreaded ? std::make_shared<util::misc::ThreadPool>() : nullptr;

        if (auto err = cppgrep::grep(argv[1], argv[2], thread_pool); !err)
        {
            util::log::error(err.error().value_or("Unknown error occured when calling cppgrep."));
        }
    }
    else
    {
        util::log::error("Two arguments are required!\n"
                         "Usage: cppgrep <path> <string>, where <path> is a file or "
                         "directory, and <string> is the text to find.");
    }

    return 0;
}
