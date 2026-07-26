// Single translation unit holding the stb implementations, kept apart from the
// example so the example is not recompiled with them and so the project's
// warning set is not applied to third party code.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"
