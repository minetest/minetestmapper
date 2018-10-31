#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include "cmake_config.h"
#include "TileGenerator.h"

void usage()
{
	const char *usage_text = "minetestmapper [options]\n"
			"  -i/--input <world_path>\n"
			"  -o/--output <output_image.png>\n"
			"  --bgcolor <color>\n"
			"  --scalecolor <color>\n"
			"  --playercolor <color>\n"
			"  --origincolor <color>\n"
			"  --drawscale\n"
			"  --drawplayers\n"
			"  --draworigin\n"
			"  --drawalpha\n"
			"  --noshading\n"
			"  --noemptyimage\n"
			"  --min-y <y>\n"
			"  --max-y <y>\n"
			"  --backend <backend>\n"
			"  --geometry x:y+w+h\n"
			"  --extent\n"
			"  --zoom <zoomlevel>\n"
			"  --colors <colors.txt>\n"
			"  --scales [t][b][l][r]\n"
			"Color format: '#000000'\n";
	std::cout << usage_text;
}

bool file_exists(const std::string &path)
{
	std::ifstream ifs(path.c_str());
	return ifs.is_open();
}

std::string search_colors(const std::string &worldpath)
{
	if(file_exists(worldpath + "/colors.txt"))
		return worldpath + "/colors.txt";

#ifndef _WIN32
	char *home = std::getenv("HOME");
	if(home) {
		std::string check = ((std::string) home) + "/.minetest/colors.txt";
		if(file_exists(check))
			return check;
	}
#endif

	if(!(SHAREDIR[0] == '.' || SHAREDIR[0] == '\0') && file_exists(SHAREDIR "/colors.txt"))
		return SHAREDIR "/colors.txt";

	std::cerr << "Warning: Falling back to using colors.txt from current directory." << std::endl;
	return "colors.txt";
}

int main(int argc, char *argv[])
{
	static struct option long_options[] =
	{
		{"help", no_argument, 0, 'h'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{"bgcolor", required_argument, 0, 'b'},
		{"scalecolor", required_argument, 0, 's'},
		{"origincolor", required_argument, 0, 'r'},
		{"playercolor", required_argument, 0, 'p'},
		{"draworigin", no_argument, 0, 'R'},
		{"drawplayers", no_argument, 0, 'P'},
		{"drawscale", no_argument, 0, 'S'},
		{"drawalpha", no_argument, 0, 'e'},
		{"noshading", no_argument, 0, 'H'},
		{"backend", required_argument, 0, 'd'},
		{"geometry", required_argument, 0, 'g'},
		{"extent", no_argument, 0, 'E'},
		{"min-y", required_argument, 0, 'a'},
		{"max-y", required_argument, 0, 'c'},
		{"zoom", required_argument, 0, 'z'},
		{"colors", required_argument, 0, 'C'},
		{"scales", required_argument, 0, 'f'},
		{"noemptyimage", no_argument, 0, 'n'},
		{0, 0, 0, 0}
	};

	std::string input;
	std::string output;
	std::string colors = "";

	TileGenerator generator;
	bool onlyPrintExtent = false;
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, "hi:o:", long_options, &option_index);
		if (c == -1)
			break; // done

		switch (c) {
			case 'h':
				usage();
				return 0;
				break;
			case 'i':
				input = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'b':
				generator.setBgColor(optarg);
				break;
			case 's':
				generator.setScaleColor(optarg);
				break;
			case 'r':
				generator.setOriginColor(optarg);
				break;
			case 'p':
				generator.setPlayerColor(optarg);
				break;
			case 'R':
				generator.setDrawOrigin(true);
				break;
			case 'P':
				generator.setDrawPlayers(true);
				break;
			case 'S':
				generator.setDrawScale(true);
				break;
			case 'e':
				generator.setDrawAlpha(true);
				break;
			case 'E':
				onlyPrintExtent = true;
				break;
			case 'H':
				generator.setShading(false);
				break;
			case 'd':
				generator.setBackend(optarg);
				break;
			case 'a': {
					std::istringstream iss(optarg);
					int miny;
					iss >> miny;
					generator.setMinY(miny);
				}
				break;
			case 'c': {
					std::istringstream iss(optarg);
					int maxy;
					iss >> maxy;
					generator.setMaxY(maxy);
				}
				break;
			case 'g': {
					std::istringstream geometry(optarg);
					int x, y, w, h;
					char c;
					geometry >> x >> c >> y >> w >> h;
					if (geometry.fail() || c != ':' || w < 1 || h < 1) {
						usage();
						exit(1);
					}
					generator.setGeometry(x, y, w, h);
				}
				break;
			case 'f': {
					uint flags = 0;
					if(strchr(optarg, 't') != NULL)
						flags |= SCALE_TOP;
					if(strchr(optarg, 'b') != NULL)
						flags |= SCALE_BOTTOM;
					if(strchr(optarg, 'l') != NULL)
						flags |= SCALE_LEFT;
					if(strchr(optarg, 'r') != NULL)
						flags |= SCALE_RIGHT;
					generator.setScales(flags);
				}
				break;
			case 'z': {
					std::istringstream iss(optarg);
					int zoom;
					iss >> zoom;
					generator.setZoom(zoom);
				}
				break;
			case 'C':
				colors = optarg;
				break;
			case 'n':
				generator.setDontWriteEmpty(true);
				break;
			default:
				exit(1);
		}
	}

	if (input.empty() || (!onlyPrintExtent && output.empty())) {
		usage();
		return 0;
	}

	try {

		if (onlyPrintExtent) {
			generator.printGeometry(input);
			return 0;
		}

		if(colors == "")
			colors = search_colors(input);
		generator.parseColorsFile(colors);
		generator.generate(input, output);

	} catch(std::runtime_error &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
