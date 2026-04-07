#include <esvo2_core/esvo2_Tracking.h>

int main(int argc, char **argv)
{
    YAML::Node cfg = YAML::LoadFile(argv[1]);
    esvo2_core::esvo2_Tracking tracker(cfg["tracking"]);
    return 0;
}
