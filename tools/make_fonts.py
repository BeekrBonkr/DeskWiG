#!/usr/bin/env python3
"""Builds the font subsets embedded in the firmware (fonts/*.ttf).

  sans.ttf   Inter Regular, Latin + Latin-1 + a few symbols   (OFL 1.1)
  bold.ttf   Inter Bold, same range                            (OFL 1.1)
  emoji.ttf  Noto Emoji (monochrome) subset of common emoji    (OFL 1.1)

Usage: python3 tools/make_fonts.py <Inter-Regular.ttf> <Inter-Bold.ttf> "<NotoEmoji[wght].ttf>"
Needs fontTools (pip install fonttools).
"""
import sys
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

LATIN = "U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-2019,U+201C-201D,U+2022,U+2026,U+2030,U+2190-2193,U+2212,U+00B0,U+2103,U+2109,U+20AC,U+2122"

# Weather, time, status, arrows, faces, hands, hearts, objects that make sense on a desk display.
EMOJI = ",".join([
    "U+2600-2601", "U+2602-2603", "U+2604", "U+2614", "U+2615", "U+2618", "U+261D", "U+2620", "U+2622-2623",
    "U+2626", "U+262A", "U+262E-262F", "U+2638-263A", "U+2640", "U+2642", "U+2648-2653", "U+265F-2660", "U+2663",
    "U+2665-2666", "U+2668", "U+267B", "U+267E-267F", "U+2692-2697", "U+2699", "U+269B-269C", "U+26A0-26A1",
    "U+26A7", "U+26AA-26AB", "U+26B0-26B1", "U+26BD-26BE", "U+26C4-26C5", "U+26C8", "U+26CE-26CF", "U+26D1",
    "U+26D3-26D4", "U+26E9-26EA", "U+26F0-26F5", "U+26F7-26FA", "U+26FD", "U+2702", "U+2705", "U+2708-270D",
    "U+270F", "U+2712", "U+2714", "U+2716", "U+271D", "U+2721", "U+2728", "U+2733-2734", "U+2744", "U+2747",
    "U+274C", "U+274E", "U+2753-2755", "U+2757", "U+2763-2764", "U+2795-2797", "U+27A1", "U+27B0", "U+27BF",
    "U+2934-2935", "U+2B05-2B07", "U+2B1B-2B1C", "U+2B50", "U+2B55", "U+3030", "U+303D", "U+3297", "U+3299",
    "U+1F300-1F321", "U+1F324-1F32C", "U+1F32D-1F335", "U+1F337-1F37C", "U+1F37E-1F393", "U+1F396-1F397",
    "U+1F399-1F39B", "U+1F39E-1F3B0", "U+1F3B1-1F3BF", "U+1F3C0-1F3C6", "U+1F3C7-1F3CA", "U+1F3CD-1F3D3",
    "U+1F3D4-1F3DF", "U+1F3E0-1F3F0", "U+1F3F3-1F3F5", "U+1F3F7-1F3FA", "U+1F400-1F43F", "U+1F440",
    "U+1F442-1F4FD", "U+1F4FF-1F53D", "U+1F549-1F54E", "U+1F550-1F567", "U+1F56F-1F570", "U+1F573-1F57A",
    "U+1F587", "U+1F58A-1F58D", "U+1F590", "U+1F595-1F596", "U+1F5A4-1F5A5", "U+1F5A8", "U+1F5B1-1F5B2",
    "U+1F5BC", "U+1F5C2-1F5C4", "U+1F5D1-1F5D3", "U+1F5DC-1F5DE", "U+1F5E1", "U+1F5E3", "U+1F5E8", "U+1F5EF",
    "U+1F5F3", "U+1F5FA-1F5FF", "U+1F600-1F64F", "U+1F680-1F6C5", "U+1F6CB-1F6D2", "U+1F6D5-1F6D7",
    "U+1F6E0-1F6E5", "U+1F6E9", "U+1F6EB-1F6EC", "U+1F6F0", "U+1F6F3-1F6FC", "U+1F7E0-1F7EB",
    "U+1F90C-1F93A", "U+1F93C-1F945", "U+1F947-1F978", "U+1F97A-1F9CB", "U+1F9CD-1F9FF",
    "U+1FA70-1FA74", "U+1FA78-1FA7C", "U+1FA80-1FA86", "U+1FA90-1FAAC", "U+1FAB0-1FABA", "U+1FAC0-1FAC5",
    "U+1FAD0-1FAD9", "U+1FAE0-1FAE7", "U+1FAF0-1FAF6",
])

def make(src, dst, unicodes, instance_wght=None):
    if instance_wght is not None:
        font = TTFont(src)
        font = instancer.instantiateVariableFont(font, {"wght": instance_wght}, inplace=False)
        src_tmp = dst + ".instance.ttf"
        font.save(src_tmp)
        src = src_tmp
    opts = subset.Options()
    opts.flavor = None
    opts.layout_features = []          # no GSUB/GPOS: stb_truetype ignores them anyway
    opts.hinting = False
    opts.desubroutinize = True
    opts.notdef_outline = True
    opts.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    font = subset.load_font(src, opts)
    s = subset.Subsetter(opts)
    s.populate(unicodes=subset.parse_unicodes(unicodes))
    s.subset(font)
    subset.save_font(font, dst, opts)
    n = len(font.getBestCmap())
    import os
    print(f"{dst}: {os.path.getsize(dst)} bytes, {n} glyphs")

if __name__ == "__main__":
    reg, bold, emoji = sys.argv[1:4]
    make(reg,   "fonts/sans.ttf",  LATIN)
    make(bold,  "fonts/bold.ttf",  LATIN)
    make(emoji, "fonts/emoji.ttf", EMOJI, instance_wght=400)
