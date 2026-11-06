#pragma once
#include <cstdio>
unsigned char* stbi_load_from_file(FILE*, int*, int*, int*, int);
void stbi_image_free(void*);
const char* stbi_failure_reason();
