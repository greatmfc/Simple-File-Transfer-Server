#ifndef SP_H
#define SP_H
#include "sft.h"

constexpr char not_found_html[] = "<!DOCTYPE html>\n<html lang=\"en\">\n\n<head>\n\t<meta charset=\"UTF - 8\">\n\t<title>404</title>\n</head>\n\n<body>\n\t<div class=\"text\" style=\"text-align: center\">\n\t\t<h1> 404 Not Found </h1>\n\t\t<h1> Target file is not found in sft. </h1>\n\t<hr>\n\t\t<div>" SFT_VER"</div></div>\n</body>\n\n</html>\n";
constexpr char forbidden_html[] = "<!DOCTYPE html>\n<html lang=\"en\">\n\n<head>\n\t<meta charset=\"UTF - 8\">\n\t<title>403</title>\n</head>\n\n<body>\n\t<div class=\"text\" style=\"text-align: center\">\n\t\t<h1> 403 Forbidden </h1>\n\t\t<h1> Not allowed to access request file. </h1>\n\t<hr>\n\t\t<div>" SFT_VER"</div></div>\n</body>\n\n</html>\n";
#endif // SP_H

