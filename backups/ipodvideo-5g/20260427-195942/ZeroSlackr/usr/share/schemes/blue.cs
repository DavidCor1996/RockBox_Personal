\name Blue Babble

\def white #fff
\def black #000
\def green #333399
\def liblue #0066CC
\def blue   #83acd6


  header: bg => liblue, fg => white, line => green, accent => green
	  shine => green, shadow => green,
	  gradient.top => liblue,
	  gradient.middle => green,
	  gradient.bottom => liblue,
	  gradient.bar => black

   music: bar => <vert green to liblue to blue>

    menu: bg => white, fg => black, choice => <vert liblue to white to liblue> , icon => black,
          selbg => <vert blue to liblue to blue>, selfg => white,
	  selchoice => white, 
	  icon0 => black, icon1 => white, icon2 => black, icon3 => white

  scroll: box => green,
		bg => <horiz green to white to green>,
		bar => <horiz liblue to green to liblue>-1


 battery: border => black, bg => blue, fill.normal => <vert blue to liblue to blue >, 
		fill.low => blue +1, fill.charge => <vert liblue to blue to liblue >,
		bg.low => blue, bg.charging => blue

    lock: border => black, fill => black

 loadavg: bg => white, fg => liblue, spike => black

  window: bg => white, fg => black, border => white -3


  dialog: bg => white, fg => black, line => green,
          title.fg => black,
          button.bg => white, button.fg => black, button.border => blue,
	  button.sel.bg => green, button.sel.fg => white, 
	  button.sel.border => blue, button.sel.inner => liblue

  error:  bg => white, fg => black, line => green,
          title.fg => black,
          button.bg => white, button.fg => black, button.border => blue,
	  button.sel.bg => green, button.sel.fg => white, 
	  button.sel.border => blue, button.sel.inner => liblue

   input: bg => white, fg => black, selbg => <vert liblue to liblue to liblue>, selfg => white,
	  border => black, cursor => liblue

  slider: border => black, bg => white, full => <vert green to liblue to blue>

textarea: bg => white, fg => white

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => black,
	default.border => black,
	selected.bg => <vert liblue to liblue to green>,
	selected.fg => white,
	selected.border => green,
	special.bg => <vert white to liblue to liblue>,
	special.fg => black,
	special.border => black

button:
	default.bg => white,
	default.fg => black,
	default.border => black,
	selected.bg => <vert white to liblue to liblue>,
	selected.fg => white,
	selected.border => green
