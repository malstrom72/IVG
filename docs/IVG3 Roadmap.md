PATH

The PATH instruction is used for drawing arbitrary vector paths consisting of straight lines, Bézier curves, and arcs.
It will be filled with the current fill setting and outlined with the current pen setting.

A PATH block contains a list of path commands.
All commands are lowercase, to distinguish them from top-level drawing primitives (uppercase).
Optional arguments always appear at the end with defaults shown.

syntax:

PATH svg:<svg data> | ( <instructions> [ closed:(yes|no)=no ] )

<instructions> are a block of:

[
	move-to   <x>,<y>
	line-to   <x>,<y>[,<x>,<y>...]
	bezier-to <x>,<y> via:[<cx>,<cy>]
	bezier-to <x>,<y> via:[<c1x>,<c1y>,<c2x>,<c2y>]
	arc-to    <x>,<y>,(<r>|<rx>,<ry>) [sweep:cw|ccw=cw] [large:yes|no=no] [rotate:<deg>=0]
	arc-sweep <cx>,<cy>,<degrees>
]

	•	move-to <x>,<y>
Sets the current drawing point. (Must be first instruction.)
	•	line-to <x>,<y>[,<x>,<y>...]
Draws straight line(s) from the current point to one or more endpoints.
	•	bezier-to <x>,<y> via:[cx,cy]
Draws a quadratic Bézier curve from the current point to <x>,<y> using one control point.
	•	bezier-to <x>,<y> via:[c1x,c1y,c2x,c2y]
Draws a cubic Bézier curve using two control points.
	•	arc-to <x>,<y>,(r|rx,ry)
Draws an elliptical arc from the current point to <x>,<y>.
	•	r = circular radius, or rx,ry = ellipse radii.
	•	[sweep:cw|ccw=cw] chooses direction (clockwise or counterclockwise).
	•	[large:yes|no=no] chooses larger or smaller arc.
	•	[rotate:<deg>=0] rotates the ellipse axes before tracing.
	•	arc-sweep <cx>,<cy>,<degrees>
Draws a circular arc around center <cx>,<cy>.
The radius is the distance from the current point to the center.
Positive <degrees> sweeps clockwise, negative sweeps counterclockwise.
	•	closed:(yes|no)=no
Option on the PATH instruction that closes the path automatically,
connecting the final point back to the first for both fill and stroke

