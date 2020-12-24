#!/usr/bin/env python3
import sys
import os.path
import getopt
import re
from math import sqrt
try:
	from PIL import Image
except:
	print("Could not load image routines, install PIL ('pillow' on pypi)!", file=sys.stderr)
	exit(1)

############
############
# Instructions for generating a colors.txt file for custom games and/or mods:
# 1) Add the dumpnodes mod to a Minetest world with the chosen game and mods enabled.
# 2) Join ingame and run the /dumpnodes chat command.
# 3) Run this script and poin it to the installation path of the game using -g,
#    the path(s) where mods are stored using -m and the nodes.txt in your world folder.
#    Example command line:
#      ./util/generate_colorstxt.py --game /usr/share/minetest/games/minetest_game \
#        -m ~/.minetest/mods ~/.minetest/worlds/my_world/nodes.txt
# 4) Copy the resulting colors.txt file to your world folder or to any other places
#    and use it with minetestmapper's --colors option.
###########
###########

# minimal sed syntax, s|match|replace| and /match/d supported
REPLACEMENTS = [
	# Delete some nodes that are usually hidden
	r'/^fireflies:firefly /d',
	r'/^butterflies:butterfly_/d',
	# Nicer colors for water and lava
	r's/^(default:(river_)?water_(flowing|source)) [0-9 ]+$/\1 39 66 106 128 224/',
	r's/^(default:lava_(flowing|source)) [0-9 ]+$/\1 255 100 0/',
	# Transparency for glass nodes and panes
	r's/^(default:.*glass) ([0-9 ]+)$/\1 \2 64 16/',
	r's/^(doors:.*glass[^ ]*) ([0-9 ]+)$/\1 \2 64 16/',
	r's/^(xpanes:.*(pane|bar)[^ ]*) ([0-9 ]+)$/\1 \3 64 16/',
]

def usage():
	print("Usage: generate_colorstxt.py [options] [input file] [output file]")
	print("If not specified the input file defaults to ./nodes.txt and the output file to ./colors.txt")
	print("  -g / --game <folder>\t\tSet path to the game (for textures), required")
	print("  -m / --mods <folder>\t\tAdd search path for mod textures")
	print("  --replace <file>\t\tLoad replacements from file (ADVANCED)")

def collect_files(path):
	dirs = []
	with os.scandir(path) as it:
		for entry in it:
			if entry.name[0] == '.': continue
			if entry.is_dir():
				dirs.append(entry.path)
				continue
			if entry.is_file() and '.' in entry.name:
				if entry.name not in textures.keys():
					textures[entry.name] = entry.path
	for path2 in dirs:
		collect_files(path2)

def average_color(filename):
	inp = Image.open(filename).convert('RGBA')
	data = inp.load()

	c0, c1, c2 = [], [], []
	for x in range(inp.size[0]):
		for y in range(inp.size[1]):
			px = data[x, y]
			if px[3] < 128: continue # alpha
			c0.append(px[0]**2)
			c1.append(px[1]**2)
			c2.append(px[2]**2)

	if len(c0) == 0:
		print(f"didn't find color for '{os.path.basename(filename)}'", file=sys.stderr)
		return "0 0 0"
	c0 = sqrt(sum(c0) / len(c0))
	c1 = sqrt(sum(c1) / len(c1))
	c2 = sqrt(sum(c2) / len(c2))
	return "%d %d %d" % (c0, c1, c2)

def apply_sed(line, exprs):
	for expr in exprs:
		if expr[0] == '/':
			if not expr.endswith("/d"): raise ValueError()
			if re.search(expr[1:-2], line):
				return ''
		elif expr[0] == 's':
			expr = expr.split(expr[1])
			if len(expr) != 4 or expr[3] != '': raise ValueError()
			line = re.sub(expr[1], expr[2], line)
		else:
			raise ValueError()
	return line
#

try:
	opts, args = getopt.getopt(sys.argv[1:], "hg:m:", ["help", "game=", "mods=", "replace="])
except getopt.GetoptError as e:
	print(str(e))
	exit(1)
if ('-h', '') in opts or ('--help', '') in opts:
	usage()
	exit(0)

input_file = "./nodes.txt"
output_file = "./colors.txt"
texturepaths = []

try:
	gamepath = next(o[1] for o in opts if o[0] in ('-g', '--game'))
	if not os.path.isdir(os.path.join(gamepath, "mods")):
		print(f"'{gamepath}' doesn't exist or does not contain a game.", file=sys.stderr)
		exit(1)
	texturepaths.append(os.path.join(gamepath, "mods"))
except StopIteration:
	print("No game path set but one is required. (see --help)", file=sys.stderr)
	exit(1)

try:
	tmp = next(o[1] for o in opts if o[0] == "--replace")
	REPLACEMENTS.clear()
	with open(tmp, 'r') as f:
		for line in f:
			if not line or line[0] == '#': continue
			REPLACEMENTS.append(line.strip())
except StopIteration:
	pass

for o in opts:
	if o[0] not in ('-m', '--mods'): continue
	if not os.path.isdir(o[1]):
		print(f"Given path '{o[1]}' does not exist.'", file=sys.stderr)
		exit(1)
	texturepaths.append(o[1])

if len(args) > 2:
	print("Too many arguments.", file=sys.stderr)
	exit(1)
if len(args) > 1:
	output_file = args[1]
if len(args) > 0:
	input_file = args[0]

if not os.path.exists(input_file) or os.path.isdir(input_file):
	print(f"Input file '{input_file}' does not exist.", file=sys.stderr)
	exit(1)

#

print(f"Collecting textures from {len(texturepaths)} path(s)... ", end="", flush=True)
textures = {}
for path in texturepaths:
	collect_files(path)
print("done")

print("Processing nodes...")
fin = open(input_file, 'r')
fout = open(output_file, 'w')
n = 0
for line in fin:
	line = line.rstrip('\r\n')
	if not line or line[0] == '#':
		fout.write(line + '\n')
		continue
	node, tex = line.split(" ")
	if not tex or tex == "blank.png":
		continue
	elif tex not in textures.keys():
		print(f"skip {node} texture not found")
		continue
	color = average_color(textures[tex])
	line = f"{node} {color}"
	#print(f"ok {node}")
	line = apply_sed(line, REPLACEMENTS)
	if line:
		fout.write(line + '\n')
		n += 1
fin.close()
fout.close()
print(f"Done, {n} entries written.")
