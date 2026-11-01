#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Draw the line sketches of the screens with clickable spots.

Each screen's objects are drawn where the game expects clicks (the spot
rectangles of the original executable, in 640x480 pixels), as white
outlines on black, for the Canny ControlNet.  Only the layout comes from
the game; the drawing is ours.  Needs ImageMagick.

    sketch.py            writes town.png, tavern.png, cabin.png and the
                         tavern's people masks here
"""

import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1472, 1104		# the render size of a 4:3 picture
K = W / 640


# No timestamps in the PNGs, so a redraw of an unchanged sketch is the
# same file.
SAME = ["-define", "png:exclude-chunk=date,time,tIME"]


def sx(v):
    return round(v * K)


def rect(x0, y0, x1, y1):
    return "rectangle %d,%d %d,%d" % (sx(x0), sx(y0), sx(x1), sx(y1))


def line(*pts):
    return "polyline " + " ".join("%d,%d" % (sx(x), sx(y)) for x, y in pts)


def ellipse(cx, cy, rx, ry):
    return "ellipse %d,%d %d,%d 0,360" % (sx(cx), sx(cy), sx(rx), sx(ry))


def sign(x0, y0, x1, y1):
    """The board for the screen's title (drawn at y 38): a plank fixed
    flush along the top edge, as a hanging one ends up too low."""
    return [rect(x0, 0, x1, y1), line((x0 + 8, 8), (x1 - 8, 8))]


# Town: a sunny square of a Caribbean port, buildings at the spots.
TOWN = sign(212, 0, 436, 66) + [
    line((0, 150), (640, 150)),				# horizon
    # Leave Town: the road to the harbour on the left.
    line((0, 349), (80, 300), (159, 260)), line((0, 300), (110, 262)),
    # Shipwright: a timber shed at the bottom left.
    rect(10, 360, 150, 425), line((10, 360), (80, 330), (150, 360)),
    # Tavern: a two-storey house with a wide door, left of centre.
    rect(165, 250, 355, 428), rect(235, 340, 290, 428),
    line((160, 250), (260, 200), (360, 250)),
    # Merchant: a shop with an awning, centre right.
    rect(362, 285, 467, 368), line((358, 285), (412, 262), (471, 285)),
    rect(395, 315, 435, 368),
    # Bank: a stone building with columns, right.
    rect(472, 270, 617, 388), line((472, 270), (545, 240), (617, 270)),
    rect(500, 300, 512, 388), rect(578, 300, 590, 388),
    # Governor: the mansion's gate on the hill, far right.
    rect(540, 150, 639, 258), rect(575, 195, 610, 258),
    line((530, 150), (590, 120), (640, 145)),
    line((0, 440), (640, 440)),				# the square's edge
]

# Tavern: the room, people and furniture at the spots.
TAVERN = sign(212, 0, 436, 66) + [
    line((0, 80), (640, 80)),				# ceiling beam
    # The bar along the back left, the bartender behind it.
    rect(70, 270, 210, 300), ellipse(110, 225, 18, 22),
    line((92, 248), (85, 290)), line((128, 248), (135, 290)),
    line((60, 150), (220, 150)), line((60, 190), (220, 190)),	# shelves
    # An arched doorway in the back wall, to the back room.
    line((355, 349), (355, 235), (359, 220), (370, 209), (385, 205),
         (400, 209), (411, 220), (415, 235), (415, 349)),
    # The barmaid standing, centre right.
    ellipse(455, 200, 17, 21), line((440, 222), (425, 355), (485, 355),
                                    (470, 222)),
    # Two small tables on the left, where the map seller (far left) and
    # the informant (by the bar's end) sit when they are in town; the
    # model draws one, which a repaint then widens to his spot.
    ellipse(70, 375, 45, 14), ellipse(190, 345, 38, 12),
    # Three sailors in the foreground around one table, bottom centre
    # right, large and cut by the bottom edge: one at the front left, one
    # behind the table, one at the front right.
    ellipse(415, 415, 80, 28),
    ellipse(265, 320, 26, 31), line((230, 350), (200, 478)),
    line((300, 350), (345, 420)),
    ellipse(325, 285, 22, 26), line((300, 310), (290, 395)),
    line((350, 310), (365, 390)),
    ellipse(550, 300, 26, 31), line((520, 330), (495, 478)),
    line((580, 330), (600, 478)),
    # The door on the right edge.
    rect(585, 95, 639, 478), line((600, 290), (612, 290)),
    line((0, 478), (640, 478)),
]

# The oval portrait frame (pic/frame, pic/idselect): the captain is
# drawn in it at (236, 58), 158x207; the frame's rings round that, a
# crest of leaves on top, scrolls at its foot.
OVAL = [ellipse(316, 163, 81, 106), ellipse(316, 163, 92, 118),
        ellipse(316, 163, 104, 130),
        "polygon %d,%d %d,%d %d,%d" % (sx(270), sx(48), sx(316), sx(2),
                                      sx(362), sx(48)),
        ellipse(316, 290, 40, 14)]
FRAME = OVAL + [line((0, 330), (640, 330))]
# The identity screen: the oval, and under it a long parchment scroll,
# its flat band (x 51-582, y 315-391) where the choices are written.
IDSELECT = OVAL + [rect(51, 315, 582, 391),
                   ellipse(40, 330, 16, 22), ellipse(594, 330, 16, 22)]
# The save screen: a slate in a carved cartouche, its flat face where the
# slots are written (x 100-560, y 30-420).
SAVEGAME = ["roundrectangle %d,%d %d,%d %d,%d" % (sx(60), sx(4), sx(600),
            sx(452), sx(60), sx(60)),
            "roundrectangle %d,%d %d,%d %d,%d" % (sx(92), sx(26), sx(568),
            sx(426), sx(40), sx(40))]


# The hall of fame: a carved gilt pediment across the top with a ribbon
# banner (its title set after), gilt pillars down each side, and between
# them red velvet cloth where the rows are written (x 120-560 from y 200).
FAME = [rect(60, 10, 580, 90), rect(200, 30, 440, 75),
        rect(80, 90, 120, 480), rect(520, 90, 560, 480),
        rect(120, 90, 520, 480)]


# The treasure cave's eight piles, in the order the game draws them (each
# once the fortune passes its mark), at their frames' places (x, y, w,
# h): a heap's mound or a chest's box inside each frame, painted in turn.
CAVE_PILES = [
    ((170, 381, 126, 99), "mound"),
    ((272, 431, 141, 49), "mound"),
    ((1, 358, 88, 121), "box"),
    ((37, 396, 194, 83), "mound"),
    ((100, 337, 126, 89), "box"),
    ((220, 291, 230, 121), "box"),
    ((414, 334, 213, 145), "mound"),
    ((14, 212, 213, 198), "box"),
]


def pile_shape(frame, kind):
    """A heap rises from the frame's bottom to a rounded top; a chest
    is a box with rounded corners, a little inside the frame."""
    x, y, w, h = frame
    if kind == "mound":
        # The mound's lower half cut at the frame's bottom.
        return [ellipse(x + w / 2, y + h * 0.72, w * 0.46, h * 0.62),
                rect(x + w * 0.04, y + h * 0.72, x + w * 0.96,
                     y + h * 0.97),
                "fill black " + rect(x - 10, y + h * 0.97, x + w + 10,
                                     y + h + 60)]
    return ["roundrectangle %d,%d %d,%d %d,%d" % (
        sx(x + w * 0.05), sx(y + h * 0.06), sx(x + w * 0.95),
        sx(y + h * 0.97), sx(w * 0.08), sx(w * 0.08))]


# Where the tavern's people go: filled shapes on their spots, repainted
# into the empty room so each comes out at the size the game expects.
TAVERN_PEOPLE = {
    "bartender": [ellipse(110, 224, 20, 24),
                  "polygon 80,245 140,245 146,282 74,282"],
    "barmaid": [ellipse(455, 198, 19, 23),
                "polygon 436,218 474,218 494,362 416,362"],
    # The sailors: front left, behind the table, front right.
    "sailor_young": [ellipse(265, 318, 30, 35),
                     "polygon 222,348 308,348 350,479 200,479"],
    "sailor_old": [ellipse(325, 283, 25, 29),
                   "polygon 294,308 358,308 372,395 284,395"],
    "sailor_big": [ellipse(550, 298, 30, 35),
                   "polygon 512,328 588,328 612,479 490,479"],
}


# The left table's whole top, redrawn larger so it reaches into the
# informant's frame (a repaint of its end alone draws a second table).
TABLE_END = [ellipse(116, 340, 106, 24)]


# The visitors, when they are in town, both behind the left table and
# inside their sprite frames, the table left alone between their
# shoulders and their legs: the map seller behind its left end (0,255
# 122x188), the informant behind its right end (142,259) (its far edge is at y 320 in the picture, its near
# edge at 364; his frame is drawn taller than the original's 90x121 to
# reach the floor).
MAPMAN = [ellipse(50, 286, 19, 23),
          "polygon 22,306 80,306 88,328 14,328",
          "polygon 18,364 84,364 94,443 8,443"]
# The stranger: a cloaked figure standing in the doorway (348-428 x
# 196-360), head and shoulders, his cloak to the floor.
STRANGER = [ellipse(388, 236, 13, 16),
            "polygon 372,252 404,252 418,358 358,358"]
INFOMAN = [ellipse(178, 280, 15, 18),
           "polygon 158,297 200,297 206,328 152,328",
           "polygon 162,364 204,364 210,446 158,446"]


# Alley: a street of a port town at night, where two men fight; the
# fighters stand on the street from y 140 to about 320, the panel below.
ALLEY = [
    line((0, 214), (640, 214)),				# the street's far side
    rect(100, 0, 500, 214), rect(500, 0, 640, 214),	# the houses
    rect(250, 120, 300, 214), rect(325, 120, 370, 214),	# their doors
    rect(385, 125, 480, 215),				# a cart by them
    line((40, 0), (130, 214)),				# a leaning palm
    line((0, 320), (640, 320)),				# the panel's top
]


# Fort: the walkway along a fort's rampart, the sea beyond the
# battlements; the fighters stand on the walkway, in front of its rail.
FORT = [
    line((0, 120), (640, 120)),				# the sea's horizon
    # One parapet: its merlons standing on its top, the rail before it.
    rect(40, 150, 120, 175), rect(190, 150, 270, 175),	# merlons
    rect(340, 150, 420, 175), rect(490, 150, 570, 175),
    line((0, 175), (640, 175)),				# the parapet's top
    line((0, 214), (640, 214)),				# the walkway's rail
    line((20, 214), (20, 110), (50, 110)),		# a gallows
    line((0, 320), (640, 320)),
]

# Deck: a ship's main deck, the quarterdeck across the back with its
# rail, stairs at each side and doors under it; the fighters on the deck.
DECK = [
    line((130, 140), (510, 140)),			# the quarterdeck's rail
    line((60, 214), (580, 214)),			# the deck at its foot
    line((60, 214), (130, 140)), line((580, 214), (510, 140)),	# stairs
    rect(180, 150, 240, 214), rect(300, 150, 360, 214),	# doors
    rect(420, 150, 480, 214),
    line((320, 0), (320, 140)),				# the mizzen mast
    line((0, 320), (640, 320)),
]


# Weapon choice: the panel drawn at (369, 56), 205x386, in its own
# pixels; each blade upright in the column clicked to take it (rapier
# x 23-75, cutlass 77-112, longsword 114-181, all from y 22 to 303).
WPNSEL_SIZE = (205, 386)
WPNSEL = [
    rect(4, 4, 201, 382), rect(16, 16, 189, 370),	# the frame
    # The rapier: a needle blade under a swept hilt of looping bars.
    ellipse(49, 58, 16, 12), ellipse(49, 70, 10, 8), line((49, 30), (49, 300)),
    line((28, 64), (70, 64)),
    ellipse(94, 52, 12, 8), line((94, 44), (92, 255)),	# the cutlass
    line((92, 255), (100, 230)),
    # The broadsword: a wide blade, a long cross guard, a round pommel.
    ellipse(147, 30, 7, 7), rect(143, 37, 151, 60),
    line((116, 62), (178, 62)),
    "polygon 138,66 156,66 154,286 147,300 140,286",
]


# Boarding: the call to board written on a framed panel at the lower
# right (the frame from (245, 280) to (620, 475), the velvet inside from
# (267, 300) to (600, 450), the text from (274, 306)); a boarder lunges
# on the left, torn sails and rigging behind.
BOARDING_FRAME = (245, 280, 620, 475)
BOARDING_FIELD = (267, 300, 600, 450)
BOARDING = [
    ellipse(150, 75, 28, 32),				# the boarder's head
    line((120, 110), (60, 250), (40, 470)),		# his back and leg
    line((180, 110), (230, 230), (200, 470)),		# his front and leg
    line((60, 140), (0, 170)), line((220, 170), (250, 120)),	# his arms
    line((250, 120), (330, 20)),				# his cutlass
    line((300, 0), (290, 270)), line((380, 0), (560, 260)),	# mast, sail
    rect(*BOARDING_FRAME), rect(*BOARDING_FIELD),
]


# The sea battle's panel (pic/sbat), right of the 480x480 battle area,
# as measured on the original: for each side its name, its speed strip,
# its men and guns, its sails, and its damage picture's window (the
# frames drawn at (489, 137) and (489, 258)), the wind between them.
SBAT_BOXES = [
    (488, 6, 635, 39), (488, 48, 635, 72), (488, 78, 560, 100),
    (564, 78, 635, 100), (488, 107, 635, 131), (488, 137, 634, 223),
    (488, 229, 635, 252),
    (488, 258, 634, 344), (488, 350, 635, 388), (488, 393, 635, 417),
    (488, 422, 560, 446), (564, 422, 635, 446), (488, 451, 635, 475),
]


# The land battle's panel (pic/lbat): the sea battle's boxes, its
# terrain pictures drawn at (489, 137) and (489, 260).
LBAT_BOXES = [b for b in SBAT_BOXES if b != (488, 258, 634, 344)] + \
    [(488, 259, 634, 346)]


# Plunder: the game writes on the clear sky in the middle (from (40, 50),
# 580 wide), so the sails stand at the sides and the pirates and their
# loot along the bottom, below y 300, as on the original.
PLUNDER = [
    line((0, 0), (40, 0), (60, 260)), line((0, 260), (60, 260)),	# a sail, left
    line((640, 0), (560, 0), (520, 300), (640, 320)),		# a sail, right
    line((0, 465), (640, 465)), line((0, 440), (640, 440)),	# the rail
    ellipse(60, 300, 18, 22), line((40, 320), (30, 440)), line((80, 320), (95, 440)),
    ellipse(170, 330, 18, 22), line((150, 350), (145, 440)), line((190, 350), (200, 440)),
    rect(230, 330, 330, 400),					# a chest
    ellipse(380, 320, 18, 22), line((360, 340), (355, 440)), line((400, 340), (410, 440)),
    ellipse(480, 330, 18, 22), line((460, 350), (455, 440)), line((500, 350), (510, 440)),
]


# Cabin: the captain's cabin, its objects at the spots.
CABIN = sign(212, 0, 436, 66) + [
    rect(160, 80, 300, 150), rect(330, 80, 470, 150),	# stern windows
    # Practice fencing: two crossed swords on the wall, upper left.
    line((90, 70), (130, 180)), line((130, 70), (90, 180)),
    # Logbook: a big book on a shelf.
    rect(140, 185, 240, 214), line((135, 216), (250, 216)),
    # Treasure cave: an iron-bound sea chest on the left.
    rect(10, 260, 130, 350), line((10, 285), (130, 285)),
    # Search: a telescope on a stand.
    line((255, 150), (310, 190)), line((282, 170), (270, 278)),
    line((282, 170), (295, 278)),
    # Exit game: the curtained bed at the back centre.
    rect(330, 120, 485, 255), line((330, 160), (485, 160)),
    # The table in the middle, maps spread on it.
    rect(320, 255, 490, 305), line((330, 305), (335, 380)),
    line((480, 305), (475, 380)), rect(340, 262, 480, 298),
    # Map of the Caribbean: a large globe at the bottom centre left.
    ellipse(235, 395, 75, 75), line((160, 470), (310, 470)),
    # Save game: a tall chest of drawers with a mirror, right.
    rect(505, 85, 635, 265), rect(530, 95, 610, 160),
    line((505, 200), (635, 200)), line((505, 232), (635, 232)),
    # Continue travels: a pair of sea boots at the bottom right.
    rect(520, 395, 560, 470), rect(575, 395, 615, 470),
    line((0, 478), (640, 478)),
]


# The cabin picture's table: all of its top cleared, for the maps to be
# put back well inside the chart frame the game draws while no treasure
# map is held (325,229 200x106), so it hides all of them.
CABIN_TABLE_TOP = ["polygon 262,250 488,250 494,306 256,306"]


# The cabin's log book and telescope came out off their spots: that
# corner is cleared, for them to be put back on the spots.
CABIN_CORNER = ["polygon 162,172 272,172 278,298 158,298",
                "polygon 78,196 152,196 152,230 78,230"]


# The town's harbour, ship and shipwright's shed: in the harbour frame
# the game draws while the ship is in port (0,258 174x222), so cleared
# from the picture, which is the town seen with the party ashore.
TOWN_HARBOUR = ["polygon 56,160 128,160 128,262 56,262",
                "polygon 0,322 166,322 166,470 0,470"]
# The closed gate the frame adds, as the shape it keeps whole.
TOWN_GATE = ["polygon 598,220 628,220 628,258 598,258"]
# The flag the model put on a pole in front of the governor's house, its
# pole down to the gate's pillar: taken out, as each nation flies its own
# (pic/town/<nation>).
TOWN_FLAG = ["polygon 490,104 518,104 518,142 506,142 506,174 490,174"]


# The duel screens' lower panel (the alley, the fort, the deck), as
# measured on the original: the panel from y 320, the flags' columns, the
# names' and the crews' boxes; and the arrow pad clicked at (283, 291),
# three cells of 24 each way.
DUEL_BOXES = [(7, 326, 62, 473), (68, 326, 250, 364), (68, 370, 250, 473),
              (388, 326, 570, 364), (390, 370, 570, 473),
              (575, 326, 630, 473), (283, 291, 355, 363)]
PAD = (283, 291, 24)


def pad_arrows():
    """The pad's eight arrows and its middle square, as polygons."""
    x0, y0, c = PAD
    ops = []
    for row in range(3):
        for col in range(3):
            cx, cy = x0 + c * col + c / 2, y0 + c * row + c / 2
            dx, dy = col - 1, row - 1
            if dx == 0 and dy == 0:
                ops.append("polygon %s" % " ".join(
                    "%g,%g" % (cx + u, cy + v)
                    for u, v in ((-4, -4), (4, -4), (4, 4), (-4, 4))))
                continue
            n = (dx * dx + dy * dy) ** 0.5
            ux, uy = dx / n, dy / n		# along the arrow
            px, py = -uy, ux			# across it
            pts = [(8, 0), (1, 6), (1, 2.5), (-8, 2.5), (-8, -2.5),
                   (1, -2.5), (1, -6)]
            ops.append("polygon " + " ".join(
                "%g,%g" % (cx + ux * a + px * b, cy + uy * a + py * b)
                for a, b in pts))
    return ops


def scaled(op):
    """A polygon given in 640x480 pixels, at the render size."""
    if not op.startswith("polygon "):
        return op
    pts = [p.split(",") for p in op.split()[1:]]
    return "polygon " + " ".join("%d,%d" % (sx(float(x)), sx(float(y)))
                                 for x, y in pts)


def bevel(name, boxes, arrows):
    """The light and the shadow of raised boxes and of arrows engraved
    in the pad, as two masks at 640x480 times 3 (grey is how strong):
    a box is lit on its top and left, shaded on its bottom and right; an
    engraved arrow is dark, lit along its lower right edge."""
    m = 3
    for side, edges in (("-light", "tl"), ("-dark", "br")):
        cmd = ["magick", "-size", "%dx%d" % (640 * m, 480 * m), "xc:black",
               "-fill", "none", "-stroke",
               "gray40" if edges == "tl" else "gray85", "-strokewidth",
               str((3 if edges == "tl" else 4) * m)]
        for x0, y0, x1, y1 in boxes:
            x0, y0, x1, y1 = x0 * m, y0 * m, x1 * m, y1 * m
            if edges == "tl":
                cmd += ["-draw", "polyline %d,%d %d,%d %d,%d" % (
                    x0, y1, x0, y0, x1, y0)]
            else:
                cmd += ["-draw", "polyline %d,%d %d,%d %d,%d" % (
                    x1, y0, x1, y1, x0, y1)]
        for op in arrows:
            pts = " ".join("%g,%g" % (float(x) * m + (1.5 if edges == "tl"
                                                       else 0),
                                       float(y) * m + (1.5 if edges == "tl"
                                                       else 0))
                           for x, y in (p.split(",") for p in op.split()[1:]))
            cmd += ["-stroke", "none", "-fill",
                    "gray55" if edges == "tl" else "gray85",
                    "-draw", "polygon " + pts]
        cmd += ["-blur", "0x2"] + SAME + [os.path.join(HERE, name + side
                                                         + ".png")]
        subprocess.run(cmd, check=True)


def mask(name, ops, blur=4):
    """White filled shapes on black, edges softened (by "blur" render
    pixels), for an inpaint."""
    cmd = ["magick", "-size", "%dx%d" % (W, H), "xc:black", "-fill",
           "white", "-stroke", "white", "-strokewidth", "5"]
    for op in ops:
        cmd += ["-draw", scaled(op)]
    cmd += ["-blur", "0x%d" % blur] + SAME + [os.path.join(HERE, name)]
    subprocess.run(cmd, check=True)


def draw(name, ops, size=(640, 480)):
    cmd = ["magick", "-size", "%dx%d" % (sx(size[0]), sx(size[1])),
           "xc:black", "-fill",
           "none", "-stroke", "white", "-strokewidth", "5"]
    for op in ops:
        cmd += ["-draw", scaled(op)]
    cmd += SAME + [os.path.join(HERE, name)]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    draw("town.png", TOWN)
    draw("tavern.png", TAVERN)
    draw("cabin.png", CABIN)
    draw("plunder.png", PLUNDER)
    draw("alley.png", ALLEY + [rect(*b) for b in DUEL_BOXES])
    draw("fort.png", FORT + [rect(*b) for b in DUEL_BOXES])
    draw("deck.png", DECK + [rect(*b) for b in DUEL_BOXES])
    draw("wpnsel.png", WPNSEL, WPNSEL_SIZE)
    draw("boarding.png", BOARDING)
    draw("frame.png", FRAME)
    draw("idselect.png", IDSELECT)
    draw("savegame.png", SAVEGAME)
    draw("fame.png", FAME)
    bevel("sbat-panel", SBAT_BOXES, [])
    bevel("lbat-panel", LBAT_BOXES, [])
    # A spyglass's round view, for the ship portraits: white inside,
    # fading to black at its rim.
    subprocess.run(["magick", "-size", "600x600", "xc:black", "-fill",
                    "white", "-draw", "circle 300,300 300,22", "-blur",
                    "0x14"] + SAME + [os.path.join(HERE, "spyglass.png")],
                   check=True)
    f, i = BOARDING_FRAME, BOARDING_FIELD
    # The frame raised; its inner rim, two pixels in from the velvet.
    bevel("boarding-panel", [f, (i[0] - 2, i[1] - 2, i[2] + 2, i[3] + 2)],
          [])
    x0, y0, c = PAD
    cells = [(x0 + c * i + 1, y0 + c * j + 1, x0 + c * (i + 1) - 1,
              y0 + c * (j + 1) - 1) for i in range(3) for j in range(3)]
    bevel("duel-panel", DUEL_BOXES + cells, pad_arrows())
    # All of them for the repaint, and each alone for the cast pass to
    # know where that person is.
    for n, (r, kind) in enumerate(CAVE_PILES):
        mask("tcave-%d.png" % n, pile_shape(r, kind), blur=3)
    mask("tavern-people.png", sum(TAVERN_PEOPLE.values(), []))
    for who, ops in TAVERN_PEOPLE.items():
        mask("tavern-%s.png" % who.replace("_", "-"), ops)
    mask("tavern-table-end.png", TABLE_END)
    mask("tavern-mapman.png", MAPMAN)
    mask("cabin-table-top.png", CABIN_TABLE_TOP)
    mask("cabin-corner.png", CABIN_CORNER)
    mask("town-harbour.png", TOWN_HARBOUR)
    mask("town-gate.png", TOWN_GATE)
    mask("town-flag.png", TOWN_FLAG)
    mask("tavern-infoman.png", INFOMAN)
    mask("tavern-stranger.png", STRANGER)
    # The town's sky along the sketch's horizon line, which the model
    # drew as a wire where there is sky (x 0-340).
    mask("town-sky-line.png", [rect(0, 140, 350, 158)], blur=3)
