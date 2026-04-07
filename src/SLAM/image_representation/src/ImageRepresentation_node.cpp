#include <image_representation/ImageRepresentation.h>
#include <yaml-cpp/yaml.h>

int main(int argc, char **argv)
{
    YAML::Node cfg = YAML::LoadFile(argv[1]);
    image_representation::ImageRepresentation ts(cfg);
    return 0;
}
