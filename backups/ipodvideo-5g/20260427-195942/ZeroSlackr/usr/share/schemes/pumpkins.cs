\name Smashing Pumpkins
#  a scheme using the colors of my favorite band's most favorite album: Mellon Collie and the Infinite Sadness
#  created by David Weitzman
#  username: futureyankee

\def white	#ffffcc
\def purple	#666699
\def orange	#ff9900
\def gold	#ffcc00



  header: bg => white, fg => orange, line => orange -1, accent => purple
	  shine => gold, shadow => purple,
	  gradient.top => orange,
	  gradient.middle => gold,
	  gradient.bottom => purple,
	  gradient.bar => gold

   music: bar => orange +2
  
 battery: border => white, bg => white, fill.normal => purple +1, 
		fill.low => orange +1, fill.charge => orange +1,
		bg.low => orange, bg.charging => gold

    lock: border => gold, fill => orange

 loadavg: bg => white, fg => purple, spike => orange

  window: bg => purple, fg => white, border => orange 

  dialog: bg => purple, fg => white, line => white,
          title.fg => white,
          button.bg => purple, button.fg => white, button.border => white,
	  button.sel.bg => white, button.sel.fg => purple, button.sel.border => white, button.sel.inner => purple +1

  error:  bg => purple, fg => white, line => orange,
          title.fg => white,
          button.bg => purple, button.fg => white, button.border => white,
	  button.sel.bg => white, button.sel.fg => gold, button.sel.border => white, button.sel.inner => purple +1

  scroll: box => purple, bg => purple, bar => gold

   input: bg => white, fg => purple, selbg => orange, selfg => purple,
	  border => purple, cursor => orange

    menu: bg => purple, fg => white, choice => white, icon => orange,
          selbg => orange, selfg => purple,
	  selchoice => white, icon0 => orange, icon1 => orange, icon2 => orange, icon3 => purple

  slider: border => white, bg => gold, full => orange

textarea: bg => orange, fg => white

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => gold,
	default.border => purple,
	selected.bg => purple,
	selected.fg => orange,
	selected.border => orange,
	special.bg => orange,
	special.fg => purple,
	special.border => purple

button:
	default.bg => white,
	default.fg => purple,
	default.border => white,
	selected.bg => gold,
	selected.fg => white,
	selected.border => white
