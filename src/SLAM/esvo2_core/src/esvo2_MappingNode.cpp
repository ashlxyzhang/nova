#include <esvo2_core/esvo2_Mapping.h>

int main(int argc, char **argv)
{
    YAML::Node cfg = YAML::LoadFile(argv[1]);
    esvo2_core::esvo2_Mapping mapper(cfg["mapping"]);
    return 0;
}
