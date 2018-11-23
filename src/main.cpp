#include "grep.h"
#include "util/log.h"

using cppgrep::Grep;

int main(int argc, char* argv[])
{
    const auto threads {std::thread::hardware_concurrency()};

    if (argc == 3)
    {
        try
        {
            auto grep  = Grep::build_grep(argv[1], argv[2], threads);
            auto count = grep.search();

            util::log::info("Found %u results.", count);
        }
        catch (std::invalid_argument& e)
        {
            util::log::error(e.what());
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
