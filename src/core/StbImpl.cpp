// Single translation unit for the stb header-only libraries.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8
#pragma warning(push, 0)
#include <stb_image.h>
#include <stb_image_write.h>
#pragma warning(pop)
