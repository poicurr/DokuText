#pragma once

#include <string>

class Bitmap;

bool writePng(const std::string &path, const Bitmap &bitmap);
